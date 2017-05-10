/* =========================================================================
 * File:        LOGGER.C
 * Description: Routines to do logging
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

// Standard defines
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <syslog.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include "config.h"
#include "logger.h"
#include "utils.h"
#include "libxstr/xstr.h"
#include "futils.h"
#include "g7ctrl.h"
#include "g7config.h"

// Last logmessage
#define MAX_LASTLOGMSG 1024
#define MAX_PACKAGENAME 128

static char last_logmsg[MAX_LASTLOGMSG] = {'\0'};

static char package_name[MAX_PACKAGENAME] = {'\0'};

// Silence warnings for the clang pragmas when compiling with gcc
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

/**
 * Utility function
 * Simplify a formatted write to a file descriptor
 *
 * @param fd File descriptor to write to
 * @param buf Format string
 * @param ... number of arguments depends on the format string
 * @return 0 on success , <> 0 on failure
 */
int
_writef_log(int fd, const char *buf, ...) {
    if (fd >= 0) {
        const int blen = MAX(1024 * 20, strlen(buf) + 1);
        char *tmpbuff = calloc(blen, 1);
        if (tmpbuff == NULL) {
            syslog(LOG_ERR, "FATAL: Cannot allocate buffer in _writef_log()");
            _exit(EXIT_FAILURE);
        }
        va_list ap;
        va_start(ap, buf);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        vsnprintf(tmpbuff, blen, buf, ap);
#pragma clang diagnostic pop

        tmpbuff[blen - 1] = 0;
        int ret = write(fd, tmpbuff, strnlen(tmpbuff, blen));
        free(tmpbuff);
        va_end(ap);
        return ret;
    }
    return -1;
}


/**
 * _vsyslogf
 * Write a message to the system logger with printf() formatting capabilities
 * @param priority Priority
 * @param msg Format string
 * @param ... Arguments for format string
 */
#define _MAX_VSYSLOG_MSGBUF (10*1024)

void
_vsyslogf(int priority, const char *msg, ...) {
    char *tmpbuff;
    tmpbuff = calloc(_MAX_VSYSLOG_MSGBUF, 1);
    if (NULL == tmpbuff) {
        syslog(LOG_CRIT, "PANIC: Cannot allocate memory in _vsyslogf()");
        _exit(EXIT_FAILURE);
    }

    va_list ap;
    va_start(ap, msg);

    int erroffset = 0;
    if (priority == LOG_ERR) {
        tmpbuff[0] = '*';
        tmpbuff[1] = '*';
        tmpbuff[2] = '*';
        tmpbuff[3] = ' ';
        erroffset = 4;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    vsnprintf(tmpbuff + erroffset, _MAX_VSYSLOG_MSGBUF - 1 - erroffset, msg, ap);
#pragma clang diagnostic pop

    tmpbuff[_MAX_VSYSLOG_MSGBUF - 1] = 0;
    syslog(priority, "%s", tmpbuff);
    free(tmpbuff);
    va_end(ap);
}

#define MAXLOGFILEBUFF 10*1024
#define LASTLOGMSG_LEN 4096
static char _lastlogmsg[LASTLOGMSG_LEN] = {'\0'};
static int _lastlogcnt = 0;

/**
 * Log message to either specified log file or if no file is specified use
 * system logger. The name of the output device to use is set in the main
 * program and communicated here with a global variable
 *
 * @param priority Log message priority (LOG_NOTICE, LOG_INFO and so on)
 * @param msg The message format string
 * @param ... The number of arguments depends on the format string
 */
void logmsg(int priority, const char *msg, ...) {
    static const int blen = 20 * 1024;
    static int _loginit = 0;
    char *msgbuff, *tmpbuff, *logfilebuff;

    pthread_mutex_lock(&logger_mutex);

    msgbuff = calloc(1, blen);
    tmpbuff = calloc(1, blen);
    logfilebuff = calloc(1, blen);
    if (msgbuff == NULL || tmpbuff == NULL || logfilebuff == NULL) {
        free(msgbuff);
        free(tmpbuff);
        free(logfilebuff);
        syslog(priority, "FATAL. Can not allocate message buffers in logmsg().");
        pthread_mutex_unlock(&logger_mutex);
        return;
    }

    // We only print errors by default and info if the verbose flag is set
    if ((priority == LOG_ERR) || (priority == LOG_CRIT) || (priority == LOG_WARNING) ||
            ((priority == LOG_INFO) && verbose_log > 0) ||
            ((priority == LOG_NOTICE) && verbose_log > 1) ||
            ((priority == LOG_DEBUG) && verbose_log > 2)) {
        va_list ap;
        va_start(ap, msg);

        int erroffset = 0;
        if (priority == LOG_DEBUG) {
            tmpbuff[0] = '>';
            tmpbuff[1] = '>';
            tmpbuff[2] = '>';
            tmpbuff[3] = ' ';
            erroffset = 4;
        } else if (priority == LOG_WARNING) {
            tmpbuff[0] = '*';
            tmpbuff[1] = ' ';
            erroffset = 2;
        } else if (priority == LOG_ERR) {
            tmpbuff[0] = '*';
            tmpbuff[1] = '*';
            tmpbuff[2] = ' ';
            erroffset = 3;
        } else if (priority == LOG_CRIT) {
            tmpbuff[0] = '*';
            tmpbuff[1] = '*';
            tmpbuff[2] = '*';
            tmpbuff[3] = '*';
            tmpbuff[4] = ' ';
            erroffset = 5;
        }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        vsnprintf(tmpbuff + erroffset, blen - 1 - erroffset, msg, ap);
#pragma clang diagnostic pop

        tmpbuff[blen - 1] = 0;
        xstrtrim_crnl(tmpbuff);

        // We don't allow any CR or NL characters in the log so replace
        // them with spaces
        for (size_t i = 0; i < strlen(tmpbuff); i++)
            if (tmpbuff[i] == '\r' || tmpbuff[i] == '\n')
                tmpbuff[i] = ' ';

        if (*logfile_name == '\0' || strcmp(logfile_name, LOGFILE_SYSLOG) == 0) {
            if (!_loginit) {
                openlog(package_name, LOG_PID | LOG_CONS, LOG_DAEMON);
                _loginit = 1;
            }
            syslog(priority, "%s", tmpbuff);
            // Syslogger
        } else {
            mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            int fd;

            // Use file
            if (strcmp(logfile_name, "stdout") == 0) {
                fd = STDOUT_FILENO;
            } else {
                fd = open(logfile_name, O_APPEND | O_CREAT | O_WRONLY, mode);
            }
            if (fd < 0) {
                // Give a message on syslog and terminate
                if (!_loginit) {
                    openlog(PACKAGE_NAME, LOG_PID | LOG_CONS, LOG_USER);
                    _loginit = 1;
                }
                syslog(LOG_ERR, "Couldn't open specified file as logfile. (%s)", tmpbuff);
                exit(EXIT_FAILURE);
            } else {
                char timebuff[32];
                time_t now = time(NULL);

                ctime_r(&now, timebuff);

                // Get rid of the year at end of time string
                timebuff[strnlen(timebuff, sizeof (timebuff)) - 5] = 0;
                snprintf(msgbuff, blen - 1, "%s: %s\n", timebuff, tmpbuff);
                msgbuff[blen - 1] = 0;

                if (!LOG_COMPRESS) {
                    _writef_log(fd, msgbuff);
                } else {
                    // If this message has already been written we ignore this and keep
                    // count on the number of times we written this message.
                    if (strcmp(tmpbuff, _lastlogmsg)) {
                        if (_lastlogcnt > 0) {
                            if (1 == _lastlogcnt) {
                                _writef_log(fd, "%s: %s\n", timebuff, _lastlogmsg);
                            } else {
                                _writef_log(fd, "%s: Msg repeated (#%d) \"%s\"\n", timebuff, _lastlogcnt, _lastlogmsg);
                            }
                            _lastlogcnt = 0;
                            *_lastlogmsg = '\0';
                        } else {
                            strncpy(_lastlogmsg, tmpbuff, LASTLOGMSG_LEN);
                        }
                        _writef_log(fd, msgbuff);
                    } else {
                        _lastlogcnt++;
                    }
                }
                if (fd != STDOUT_FILENO) {
                    close(fd);
                }
                strncpy(last_logmsg, msgbuff, MAX_LASTLOGMSG - 1);
                last_logmsg[MAX_LASTLOGMSG - 1] = '\0';
            }
        }

        va_end(ap);
    }

    free(msgbuff);
    free(tmpbuff);
    free(logfilebuff);

    pthread_mutex_unlock(&logger_mutex);
}

/**
 * Setup the logfile. Create the necessary directory if needed and then change
 * permissions so that the daemon has still permissions if it is run as other
 * user than root
 * @return
 */
int
setup_logger(char *packageName) {

    struct passwd *pwe = getpwuid(getuid());
    strncpy(package_name,packageName,MAX_PACKAGENAME-1);
    package_name[MAX_PACKAGENAME-1] = '\0';

    if (0 == strcmp(pwe->pw_name, "root")) {
        // We are running as root

        struct stat dstat;
        const mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
        char *logfile = strdup(logfile_name);
        char *dirbuff = dirname(logfile);

        if (-1 == stat(dirbuff, &dstat)) {
            if (-1 == mkdir(dirbuff, mode)) {
                openlog(package_name, LOG_PID | LOG_CONS, LOG_USER);
                syslog(LOG_ERR, "Couldn't open specified file as logfile \"%s\" ( %d : \"%s\")", dirbuff, errno, strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

        // We will eventually not run as root and hence must adjust the permissions
        // on the logfile directory so that we later on can write to the log
        errno = 0;
        pwe = getpwnam(run_as_user);
        if (pwe == NULL) {
            fprintf(stderr, "Cannot setup logfile directory.");
            syslog(LOG_ERR, "Cannot setup logfile directory. The daemon user \"%s\" does not exist.\n", run_as_user);
            exit(EXIT_FAILURE);
        }

        if (-1 == chown(dirbuff, pwe->pw_uid, pwe->pw_gid)) {
            fprintf(stderr, "Cannot change owner of logfile directory %s (%d : %s).", dirbuff, errno, strerror(errno));
            syslog(LOG_ERR, "Cannot change owner of logfile directory %s (%d : %s).", dirbuff, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        free(logfile);

    } else {

        // Check that the logfile can be written
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        int fd = open(logfile_name, O_APPEND | O_CREAT | O_WRONLY, mode);
        if (fd < 0) {
            fprintf(stderr, "Cannot access logfile \"%s\". Check permissions!\n", logfile_name);
            syslog(LOG_ERR, "Cannot access logfile \"%s\". Check permissions!\n", logfile_name);
            exit(EXIT_FAILURE);
        }
        close(fd);
    }
    return 0;
}

#pragma GCC diagnostic pop

/* EOF */
