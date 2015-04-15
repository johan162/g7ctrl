/* =========================================================================
 * File:        LOCKFILE.C
 * Description: Functions to manage the lockfile used to avoid
 *              running to instances of the program at once.
 * Author:      Johan Persson (johan162@gmail.com)
 *
 * Copyright (C) 2013-2015  Johan Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 * =========================================================================
 */

// We want the full POSIX and C99 standard
#define _GNU_SOURCE

// Standard UNIX includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <libgen.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include "config.h"
#include "lockfile.h"
#include "utils.h"
#include "logger.h"
#include "g7ctrl.h"
#include "g7config.h"

/**
 * We use a lockfile with the server PID stored to avoid that multiple
 * daemons are started. Since the lock file is stored in the standard /var/run
 * this also means that if the daemon is running as any other user than "root"
 * the lock file cannot be removed once it has been created (sine the server
 * normally changes user it runs as from root to a non privileged user). However
 * this is not terrible since at startup the daemon will check the lockfile and
 * if it exists also verify that the PID stored in the lockfile is a valid PID for a
 * process. If not it is regarded as a stale lockfile and overwritten.
 */
void
delete_lockfile(void) {

    logmsg(LOG_NOTICE, "Exiting. Removing lockfile '%s'.", pidfile);
    if (-1 == unlink(pidfile)) {
        int gid = (int) getgid();
        int uid = (int) getuid();
        logmsg(LOG_ERR, "Cannot remove lock-file (%s) while running as uid=%d, gid=%d. (%d : %s)",
                pidfile, uid, gid, errno, strerror(errno));
    }
}

/**
 * Updated the process id in an already existing lockfile (if the file is stale)
 * or create new lockfile if no existing is found.
 * @param filename      The name of the lockfile
 * @return              -1 = General file error, -2 existing process is running. Any positive
 *                      number represents the file handle of the lockfile
 */
int
tryopen_pidfile(const char *pidFile) {
    const mode_t fmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int fd;

    fd = open(pidFile, O_RDWR | O_CREAT | O_CLOEXEC, fmode);
    if (-1 == fd) {
        return -1;
    }

    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    if (-1 == fcntl(fd, F_SETLK, &fl)) {
        close(fd);
        if (errno == EAGAIN || errno == EACCES) {
            return -2;
        } else
            return -1;
    }

    if (-1 == ftruncate(fd, 0)) {
        close(fd);
        return -1;
    }

    char buf[32];
    snprintf(buf, sizeof (buf), "%ld\n", (long) getpid());
    if (write(fd, buf, strlen(buf)) != (int) strlen(buf)) {
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * Setup a lockfile based on the program name
 */
void
get_lockfile(void) {

    // The PID file could have been already specified by the user as a command
    // line argument. In that case just create the lockfile.
    // If not specified by the user the default location will be /var/run/PACKAGE/PACKAGE.pid
    // this assumes that this function is called while still running as root
    if (!*pidfile) {

        struct passwd *pwe = getpwuid(getuid());

        if (0 == strcmp(pwe->pw_name, "root")) {

            char dirbuff[512];
            struct stat dstat;
            const mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
            snprintf(dirbuff, sizeof (dirbuff), "/var/run/%s", PACKAGE);
            _vsyslogf(LOG_DEBUG, "Checking directory for lockfile '%s'", dirbuff);
            if (-1 == stat(dirbuff, &dstat)) {
                if (-1 == mkdir(dirbuff, mode)) {
                    _vsyslogf(LOG_CRIT, "Cannot create PID directory %s (%d : %s).",
                            dirbuff, errno, strerror(errno));
                    exit(EXIT_FAILURE);
                } else {
                    _vsyslogf(LOG_DEBUG, "Created lockfile directory '%s'", dirbuff);
                }
            }

            // We will eventually not run as root and hence must adjust the permissions
            // on the PID directory so that we later on can remove the PID file
            errno = 0;
            pwe = getpwnam(run_as_user);
            if (pwe == NULL) {
                _vsyslogf(LOG_CRIT, "Specified user to run as, '%s', does not exist.", run_as_user);
                exit(EXIT_FAILURE);
            }

            // Now change owner of the directory to the user running the daemon
            if (-1 == chown(dirbuff, pwe->pw_uid, pwe->pw_gid)) {
                _vsyslogf(LOG_CRIT, "Cannot change owner for PID directory %s (%d : %s).", dirbuff, errno, strerror(errno));
                exit(EXIT_FAILURE);
            }

        } else {
            _vsyslogf(LOG_CRIT, "A PID file must be specified as argument when not started as root");
            fprintf(stderr, "No PID file specified. Aborting.\n");
            exit(EXIT_FAILURE);
        }

        snprintf(pidfile, PIDFILE_LEN - 1, "/var/run/%s/%s.pid", PACKAGE, PACKAGE);
    }


    // The file handle is returned if the call is successful
    int fd = tryopen_pidfile(pidfile);
    if (0 > fd) {
        if (-2 == fd) {
            _vsyslogf(LOG_CRIT, "Daemon already running. Aborting.");
            fprintf(stderr, "Daemon already running. Aborting.\n");
        } else {
            _vsyslogf(LOG_CRIT, "Failed to create pidfile (%d : %s).", errno, strerror(errno));
            fprintf(stderr, "Failed to create pidfile (%d : %s)\n", errno, strerror(errno));
        }
        _exit(EXIT_FAILURE);
    }
    close(fd);
    _vsyslogf(LOG_DEBUG, "Created lockfile=\"%s\"", pidfile);
}


/* EOF */
