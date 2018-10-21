/* =========================================================================
 * File:        G7CTRL
 * Description: Main module for the g7ctrl daemon
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

#include "config.h"

// Standard UNIX includes
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <pthread.h>
#include <grp.h>
#include <time.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>

// prctl() only exists in Linux and not in BSD (and hence not in OSX)
#ifndef __APPLE__
#include <sys/prctl.h>
#endif

#include "g7ctrl.h"
#include "build.h"
#include "utils.h"
#include "futils.h"
#include "logger.h"
#include "g7config.h"
#include "lockfile.h"
#include "socklistener.h"
#include "connwatcher.h"
#include "libxstr/xstr.h"
#include "presets.h"
#include "sighandling.h"
#include "g7sendcmd.h"
#include "g7config.h"
#include "geoloc_cache.h"
#include "geoloc.h"


// Since these defines are supposed to be defined directly in the linker using
// --defsym to step the build number at each build and apples linker do not support
// this we just give them a 0 number here and never use them.
// This also means that on OSX the build number displayed is meaningless
// TODO. Remove all references to build number when running on OSX
#ifdef __APPLE__
char __BUILD_DATE = '\0';
char __BUILD_NUMBER = '\0';
#endif

/*
 * Server identification
 */
char server_program_name[32] = {0};

/*
 * ts_serverstart
 * Timestamp when server was started
 */
time_t ts_serverstart;

/** Holds the list wih information about all connected clients, Note that
 * this list is sparse (can have holes in it with empty slots) since
 * any client can disconnected at any time
 */
struct client_info *client_info_list = NULL;

/** Number of connected clients*/
int num_clients = 0;

/*
 * Mutexes to protect
 * 1) The data structure when multiple clients are connected
 * 2) The creation and killing of thread and thread counts
 */
pthread_mutex_t socks_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cmdtag_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cmdqueue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t logger_mutex = PTHREAD_MUTEX_INITIALIZER;



/*
 * Setup to handle program start up argument in both short and long format for use with the
 * getopt() library function.
 */
static const char short_options [] = "d:s:hvi:l:V:p:t:";
static const struct option long_options [] = {
    { "daemon", no_argument, NULL, 'd'},
    { "stty", required_argument, NULL, 's'},
    { "help", no_argument, NULL, 'h'},
    { "inifile", required_argument, NULL, 'i'},
    { "version", no_argument, NULL, 'v'},
    { "logfile", required_argument, NULL, 'l'},
    { "verbose", required_argument, NULL, 'V'},
    { "cmdport", required_argument, NULL, 'p'},
    { "trkport", required_argument, NULL, 't'},
    { "datadir", required_argument, NULL, 'x'},
    { "dbdir", required_argument, NULL, 'y'},
    { "pidfile", required_argument, NULL, 'z'},
    { 0, 0, 0, 0}
};


int arg_verbose_log=-1;
int arg_tcpip_device_port=-1;
int arg_tcpip_cmd_port=-1;
/**
 * Parse all command line options given to the server at startup. The server accepts both
 * long and short version of command line options.
 * @param argc Number of arguments (from invocation of main() )
 * @param argv Argument vector of strings
 */
void
parsecmdline(int argc, char **argv) {
    // Parse command line options
    int opt, idx;
    *daemon_config_file = '\0';
    *pidfile = '\0';
#ifndef __APPLE__    
    stty_device_startidx = 0;
#endif
    *logfile_name = '\0';
    verbose_log = -1;
    opterr = 0; // Suppress error string from getopt_long()
    if (argc > 17) {
        fprintf(stderr, "Too many arguments. Try '-h'.");
        exit(EXIT_FAILURE);
    }

    /*
     * Loop through all given input strings and check maximum length.
     * No single argument may be longer than 256 bytes (this could be
     * an indication of a buffer overflow attack)
     */
    for (int i = 1; i < argc; i++) {
        if (strnlen(argv[i], 256) >= 256) {
            fprintf(stderr, "Argument %d is too long.", i);
            exit(EXIT_FAILURE);
        }
    }
#ifdef DEBUG_SIMULATE
    const char *sim_info = " ** DEBUG BUILD ** WILL NOT CONNECT TO DEVICE. THIS iS ONLY SIMULATION.";
#else
    const char *sim_info = "";
#endif

    while (-1 != (opt = getopt_long(argc, argv, short_options, long_options, &idx))) {

        switch (opt) {
            case 0: /* getopt_long() flag */
                break;

            case 'h':
                fprintf(stdout,
                        "'%s' Copyright 2013-2018 Johan Persson, (johan162@gmail.com) \n"
                        "This is free software; see the source for copying conditions.\nThere is NO "
                        "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
                        "%s\n"
                        "Usage: %s [options]\n"
                        "Synopsis:\n"
                        "Utility to communicate with a xTreme G7 GPS Tracker.\n"
                        "Options:\n"
                        " -h,      --help            Print help and exit\n"
                        " -v,      --version         Print version string and exit\n"
                        " -i file, --inifile=file    Use specified file as config file\n"
                        " -d,      --daemon          Run asynchroneus (i.e. background)\n"
                        " -s,      --stty            Specify index of tty device to use\n"
                        " -p,      --cmdport=n       Specify listening TCP/IP port for command\n"
                        " -t,      --trkport=n       Specify listening TCP/IP port for tracker\n"
                        " -l,      --logfile=file    Specify logile\n"
                        " -V n,    --verbose         Specify verbosity of logging\n"
                        " --pidfile=file             Specify location for PID lockfile\n"
                        " --datadir=file             Specify location for data directory\n"
                        " --dbdir=file               Specify location for DB directory\n",
                        server_program_name, sim_info, server_program_name);
                exit(EXIT_SUCCESS);
                break;

            case 'v':

#ifdef __APPLE__
                fprintf(stdout,
                        "%s %s\n"
                        "%s\n"
                        "Copyright (C) 2013-2015  Johan Persson (johan162@gmail.com)\n\n"
                        "This program is free software: you can redistribute it and/or modify\n"
                        "it under the terms of the GNU General Public License as published by\n"
                        "the Free Software Foundation, either version 3 of the License, or\n"
                        "any later version.\n\n"
                        "This program is distributed in the hope that it will be useful,\n"
                        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                        "GNU General Public License for more details.\n",
                        server_program_name, PACKAGE_VERSION, sim_info);
#else
                fprintf(stdout,
                        "%s %s (build: %lu.%lu)\n\n"
                        "%s\n"
                        "Copyright (C) 2013-2015  Johan Persson (johan162@gmail.com)\n\n"
                        "This program is free software: you can redistribute it and/or modify\n"
                        "it under the terms of the GNU General Public License as published by\n"
                        "the Free Software Foundation, either version 3 of the License, or\n"
                        "any later version.\n\n"
                        "This program is distributed in the hope that it will be useful,\n"
                        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                        "GNU General Public License for more details.\n",
                        server_program_name, PACKAGE_VERSION, (unsigned long) &__BUILD_DATE, (unsigned long) &__BUILD_NUMBER, sim_info);

#endif
                exit(EXIT_SUCCESS);
                break;

            case 'i':
                if (optarg != NULL) {
                    xmb_strncpy(daemon_config_file, optarg, INIFILE_LEN);
                    daemon_config_file[INIFILE_LEN - 1] = '\0';
                    if (strlen(daemon_config_file) == INIFILE_LEN - 1) {
                        fprintf(stderr, "ini file given as argument is invalid. Too long.");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'd':
                daemonize = TRUE;
                break;
#ifndef __APPLE__
            case 's':
                if (optarg != NULL) {
                    stty_device_startidx = xatoi(optarg);
                    if (stty_device_startidx < 0 || stty_device_startidx > 8) {
                        logmsg(LOG_CRIT, "Illegal USB device %d", stty_device_startidx);
                        exit(EXIT_FAILURE);
                    }
                }
                break;
#endif
            case 'p':
                if (optarg != NULL) {
                    arg_tcpip_cmd_port = xatoi(optarg);
                    if (arg_tcpip_cmd_port < 1025 || arg_tcpip_cmd_port > 60000) {
                        logmsg(LOG_CRIT, "Illegal command port %d", arg_tcpip_cmd_port);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 't':
                if (optarg != NULL) {
                    arg_tcpip_device_port = xatoi(optarg);
                    if (arg_tcpip_device_port < 1025 || arg_tcpip_device_port > 60000) {
                        logmsg(LOG_CRIT, "Illegal device port %d", arg_tcpip_device_port);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'V':
                if (optarg && (*optarg == '1' || *optarg == '2' || *optarg == '3'))
                    arg_verbose_log = *optarg - '0';
                else {
                    logmsg(LOG_CRIT, "Illegal verbose level specified. must be in range [1-3]. Aborting.");
                    exit(EXIT_FAILURE);
                }
                break;

            case 'l':
                if (optarg != NULL) {
                    xmb_strncpy(logfile_name, optarg, MAX_LOGFILE_NAME_LEN - 1);
                    logfile_name[MAX_LOGFILE_NAME_LEN - 1] = '\0';
                    if (strlen(logfile_name) == MAX_LOGFILE_NAME_LEN - 1) {
                        fprintf(stderr, "logfile file given as argument is invalid. Too long.");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'z':
                if (optarg != NULL) {
                    xmb_strncpy(pidfile, optarg, PIDFILE_LEN);
                    pidfile[PIDFILE_LEN - 1] = '\0';
                    if (strlen(pidfile) == PIDFILE_LEN - 1) {
                        fprintf(stderr, "PID file given as argument is invalid. Too long.");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'x':
                if (optarg != NULL) {
                    xmb_strncpy(data_dir, optarg, MAX_DATA_DIR_LEN);
                    data_dir[MAX_DATA_DIR_LEN - 1] = '\0';
                    if (strlen(data_dir) == MAX_DATA_DIR_LEN - 1) {
                        fprintf(stderr, "Data directory given as argument is invalid. Too long.");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'y':
                if (optarg != NULL) {
                    xmb_strncpy(db_dir, optarg, MAX_DB_DIR_LEN);
                    db_dir[MAX_DB_DIR_LEN - 1] = '\0';
                    if (strlen(db_dir) == MAX_DB_DIR_LEN - 1) {
                        fprintf(stderr, "Database directory given as argument is invalid. Too long.");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case '?':
                fprintf(stderr, "Invalid specification of program option(s). See --help for more information.\n");
                exit(EXIT_FAILURE);
                break;
        }
    }
#ifdef DEBUG_SIMULATE
    // Never run as a daemon in a debug build
    daemonize = 0;
#endif

    if (tcpip_cmd_port && (tcpip_cmd_port == tcpip_device_port)) {
        fprintf(stderr, "Same port number specified as both command and device port. They must be different\n");
        exit(EXIT_FAILURE);
    }

    if (daemonize && 0 == strcmp(logfile_name, "stdout")) {
        fprintf(stderr, "Cannot use 'stdout' as logfile when running as daemon");
        exit(EXIT_FAILURE);
    }

    if (argc > 1 && optind < argc) {
        fprintf(stderr, "Options not valid.\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * Check that the directory structure for the data files is in place.
 * Create directories as necessary
 */
void
chkdirstructure(void) {

    if (-1 == chkcreatedir(data_dir, "")) {
        exit(EXIT_FAILURE);
    }

    if (-1 == chkcreatedir(db_dir, "")) {
        exit(EXIT_FAILURE);
    }
    
    char cachedir[255];
    snprintf(cachedir,sizeof(cachedir),"%s/%s",db_dir,DEFAULT_MINIMAP_GEOCACHE_DIR);
    if (-1 == chkcreatedir(cachedir, "")) {
        exit(EXIT_FAILURE);
    }

}

/*
 * All necessary low level household stuff to kick us off as a
 * daemon process, i.e. fork, disconnect from any tty, close
 * the standard file handlers and so on
 */
void startdaemon(void) {

    // Fork off the child
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Cannot fork daemon.");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Exit parent. Note the use of _exit() rather than exit()
        // The exit() performs the atexit() cleanup handler
        // which we do not want since that would delete the lockfile
        _exit(EXIT_SUCCESS);
    }

    // Get access to files written by the daemon
    // This is not quite necessary since we explicitly set
    // the file attributes on the database and the log file
    // when they are created.
    umask(0);

    // Create a session ID so the child isn't treated as an orphan
    pid_t sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "Cannot fork daemon and create session ID.");
        exit(EXIT_FAILURE);
    }

    // Fork again to ensure we are not a session group leader
    // and hence can never regain a controlling terminal
//    pid = fork();
//    if (pid < 0) {
//        syslog(LOG_ERR, "Cannot do second fork to create daemon.");
//        exit(EXIT_FAILURE);
//    }
//
//    if (pid > 0) {
//        // Exit parent. Note the use of _exit() rather than exit()
//        // The exit() performs the atexit() cleanup handler
//        // which we do not want since that would delete the lockfile
//        _exit(EXIT_SUCCESS);
//    }

    // Use root as working directory
    if (chdir("/") < 0) {
        syslog(LOG_ERR, "Cannot change working directory to '/' for daemon.");
        exit(EXIT_FAILURE);
    }

    // Close all file descriptors
    for (int i = getdtablesize(); i >= 0; --i) {
        (void) _dbg_close(i);
    }

    // Reopen stdin, stdout, stderr so they point harmlessly to /dev/null
    // (Some brain dead library routines might write to, for example, stderr)
    int i = open("/dev/null", O_RDWR);
    int ret1 = dup(i);
    int ret2 = dup(i);
    if (-1 == ret1 || -1 == ret2) {
        syslog(LOG_ERR, "Cannot start daemon and set descriptors 0,1,2 to /dev/null.");
        exit(EXIT_FAILURE);
    }

}

/**
 * Exit handler. Automatically called when the process calls "exit()"
 * If we come here it means that some fatal error has happened since a
 * normal exit will not go through the exit handler since the daemon
 * will terminate with a _exit() call.
 */
void
exithandler(void) {
    syslog(LOG_CRIT, "Aborting daemon due to fatal error. See log \"%s\"", logfile_name);
    delete_lockfile();
}

/**
 * Check what user we are running as and change the user (if allowed) to the
 * specified user.
 */
void
chkswitchuser(void) {
    // Check if we are starting as root
    struct passwd *pwe = getpwuid(getuid());

    if (0 == strcmp(pwe->pw_name, "root")) {
        if (strcmp(run_as_user, "root")) {
            errno = 0;
            pwe = getpwnam(run_as_user);
            if (pwe == NULL) {
                logmsg(LOG_ERR, "Specified user to run as, '%s', does not exist", run_as_user);
                exit(EXIT_FAILURE);
            }


            // Make sure the data directory belongs to this new user since we will
            // be updating the DB in that directory
            char cmdbuff[64];

            logmsg(LOG_DEBUG, "Adjusting owner of file structures (%s, %s)", data_dir, db_dir);
            snprintf(cmdbuff, sizeof (cmdbuff) - 1, "chown -R %s: %s %s", run_as_user, data_dir, db_dir);
            int ret = system(cmdbuff);
            if (-1 == ret) {
                logmsg(LOG_ERR, "Cannot execute chown() command for data & db directory (%d : %s)", errno, strerror(errno));
                exit(EXIT_FAILURE);
            }



            if (strcmp(logfile_name, "syslog") && strcmp(logfile_name, "stdout")) {
                if (-1 == chown(logfile_name, pwe->pw_uid, pwe->pw_gid)) {
                    logmsg(LOG_CRIT, "Cannot change owner for logfile %s (%d : %s).", logfile_name, errno, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }

#if IS_OSX
            static const char * needed_ttyACM_grp = "wheel";
#else
            static const char * needed_ttyACM_grp = "dialout";
#endif
            // Make sure we run as belonging to the 'dialout' group needed to access a tty ACM device
            // TODO: We should read the group owner directly from the device
            struct group *gre = getgrnam(needed_ttyACM_grp);
            if (NULL == gre) {
                logmsg(LOG_ERR, "Specified group to run as, '%s', does not exist. ( %d : %s )",
                        needed_ttyACM_grp, errno, strerror(errno));
                exit(EXIT_FAILURE);
            }

            // Make sure we also belong to the group owning the ttyACM device
            gid_t groups[2];
            groups[0] = pwe->pw_gid;
            groups[1] = gre->gr_gid;
            if (-1 == setgroups(2, groups)) {
                logmsg(LOG_ERR, "Cannot set groups. Check that '%s' belongs to the '%s' group. ( %d : %s )",
                        pwe->pw_name, needed_ttyACM_grp, errno, strerror(errno));
                exit(EXIT_FAILURE);
            }

            // Now switch user that owns this process
            if (-1 == setgid(pwe->pw_gid) || -1 == setuid(pwe->pw_uid)) {
                logmsg(LOG_ERR, "Cannot set gid and/or uid. (%d : %s)", errno, strerror(errno));
                exit(EXIT_FAILURE);
            } else {
                logmsg(LOG_DEBUG, "Changing user,uid to '%s',%d", pwe->pw_name, pwe->pw_uid);
            }
        }
        // prctl() only exists in Linux
#ifndef __APPLE__
        // #pragma message "Compiling with prctl() support"
        // After a possible setuid() and setgrp() the "dumpable" flag is reset which means
        // that no core is dumped in case of a SIGSEGV signal. We want a coredump in case
        // of a memory overwrite so we make sure this is allowed
        if (-1 == prctl(PR_SET_DUMPABLE, 1, 0, 0, 0)) {
            logmsg(LOG_ERR, "Can not set PR_SET_DUMPABLE for current process");
        }
#endif
    } else {
        logmsg(LOG_DEBUG, "Not started as root so no change of running user will take place.");
    }
}

/**
 * The main entry point for the daemon. In principle it oes the following
 * 1) Handles command line arguments
 * 2) Reads the config file settings
 * 3) Check that the file structure for DB is in place
 * 4) Rebord as a daeamon if requested
 * 5) If started as root then change to the optional specified user
 * 6) Starts the main socket listening thread. This thread only stops on
 *    SIGABT or SIGSTOP signals which also implies that the daemon should
 *    terminate.
 * @param argc
 * @param argv
 * @return EXIT_SUCCESS to shell if stopped normally, EXIT_FAILURE on error
 *         starting the daemon.
 */
int
main(int argc, char *argv[]) {
    pthread_t watcher_thread;
    
    // Set the local locale (needed for UTF-8 character handling among other things)
    // Without this the xmblen() would wrong assumption on the character encoding used
    // since the clib-library always assume the POSIX (or "C" locale) at startup
    (void)setlocale(LC_ALL, "");
    (void)setlocale(LC_NUMERIC, "C");
  
    // Remember the program name we are started as
    char prognamebuffer[256];
    xmb_strncpy(prognamebuffer, argv[0], sizeof (prognamebuffer) - 1);
    xmb_strncpy(server_program_name, basename(prognamebuffer), sizeof (server_program_name) - 1);
    server_program_name[sizeof (server_program_name) - 1] = '\0';

    // Remember when the server was started
    tzset();
    ts_serverstart = time(NULL);

    // Parse and setup commandd line options
    parsecmdline(argc, argv);
        
    if (0 == strcmp("stdout", logfile_name) && daemonize) {
        syslog(LOG_CRIT, "Aborting. 'stdout' is not a valid logfile when started in daemon mode.");
        exit(EXIT_FAILURE);
    }    

    // Setup MALLOC to dump in case of memory corruption, i.e. double free
    // or overrun. This might be less efficient but this will be enabled
    // until we are 101% sure there are no mistakes in the code.
    static char *var = "MALLOC_CHECK";
    static char *val = "2";
    setenv(var, val, 1);

    // Setup exit() handler
    atexit(exithandler);

    // Setup name for logfile if not specified on the command line
    if (0 == strlen(logfile_name)) {
        xmb_strncpy(logfile_name, DEFAULT_LOG_FILE, MAX_LOGFILE_NAME_LEN);
    }

    // Setup name for data directory if not specified on the command line
    if (0 == strlen(data_dir)) {
        xmb_strncpy(data_dir, DEFAULT_DATA_DIR, MAX_DATA_DIR_LEN);
    }

    // Setup name for db directory if not specified on the command line
    if (0 == strlen(db_dir)) {
        xmb_strncpy(db_dir, DEFAULT_DB_DIR, MAX_DB_DIR_LEN);
    }

    setup_inifile();
    read_inisettings_startup();
    
    // Check for misconfigurations
    if ( strlen(google_api_key) < 15 && (use_address_lookup || include_minimap ) ) {
        syslog(LOG_CRIT, "Aborting. To use address lookup or minimaps in mail a valid Google API key must be specified.");
        exit(EXIT_FAILURE);
    }

    // Override settings if they are given on the command line
    if( arg_verbose_log > -1 ) {
        verbose_log = arg_verbose_log;
    }
    
    if( arg_tcpip_cmd_port > -1 ) {
        tcpip_cmd_port = arg_tcpip_cmd_port;
    }
    
    if( arg_tcpip_device_port > -1 ) {
        tcpip_device_port = arg_tcpip_device_port;
    }

    setup_logger(PACKAGE_NAME);

    if (daemonize) {
        startdaemon();
    }

    // Setup process lockfile so that we have only one running instance
    // get_lockfile(); 
  
    // Get the overall settings from the ini-file
    read_inisettings();

    // And close inifile
    close_inifile();
    
    // From now on it is safe to use the logfile since we are guaranteed to the the only running
    // version of the program.

#ifdef __APPLE__
    logmsg(LOG_INFO, "=== Starting g7ctrl %s ===", PACKAGE_VERSION);
#else
    logmsg(LOG_INFO, "=== Starting g7ctrl %s (build: %lu-%lu) ===", PACKAGE_VERSION,
            (unsigned long) &__BUILD_DATE, (unsigned long) &__BUILD_NUMBER);
#endif
#ifdef DEBUG_SIMULATE
    logmsg(LOG_INFO, "DEBUG MODE - SIMULATING DEVICE CONNECTIONS");
#endif
    logmsg(LOG_INFO, "Using ini-file '%s'", daemon_config_file);

    // Check that the DB directory structure is in place
    chkdirstructure();

    // Change user
    chkswitchuser();

    // Read in all command presets
    if (init_presets() < 0) {
        exit(EXIT_FAILURE);
    }
    refreshPresets(-1);

    // Setup signal handling. This creates a separate signal receiving
    // thread to avoid possible deadlocks. It also creates a handle
    // for serious errors like SIGSEGV
    setup_sighandling();
    
    // Setup geo-location cache structures
    init_geoloc_cache();
    
    // ... and read possible saved geo-loction caches
    (void)read_address_geocache();    
    (void)read_minimap_geocache();
    
    // ... finally restore cache statistics
    (void)read_geocache_stat();
    
    if( strlen(google_api_key) > 15 ) {
        // Assume this is a valid key so we can increase the rate limit a bit
        geocode_rate_limit_init(GOOGLE_APIKEY_RLIMIT_MS);
        staticmap_rate_limit_init(GOOGLE_APIKEY_RLIMIT_MS);
    } else {
        // For anonymous calls we have a smaller limit
        geocode_rate_limit_init(GOOGLE_ANONYMOUS_RLIMIT_MS);
        staticmap_rate_limit_init(GOOGLE_ANONYMOUS_RLIMIT_MS);
    }


    // The watcher thread will be watching for a USB connection
    pthread_create(&watcher_thread, NULL, connwatcher_thread, NULL);

    // Structure to keep track of all connected clients (both command and trackers)
    client_info_list = calloc(max_clients, sizeof (struct client_info));
    if (NULL == client_info_list) {
        fprintf(stderr, "FATAL: Out of memory. Aborting server.");
        exit(EXIT_FAILURE);
    }
    for( size_t i=0; i< max_clients; i++ ) {
        client_info_list[i].target_cli_idx = -1;
    }

    // Initialize the command queue we use for GPRS command
    cmdqueue_init();

    // *********************************************************************************
    // *********************************************************************************
    // **     This is the real main starting point of the program                     **
    // *********************************************************************************
    // *********************************************************************************

    // Startup the main socket server listener. The call to startupsrv() will
    // not return until the daemon is terminated by a user signal.
    if (EXIT_FAILURE == startupsrv()) {
        logmsg(LOG_ERR, "Unable to start '%s' server.", PACKAGE_NAME);
        _exit(EXIT_FAILURE);
    }

    // *********************************************************************************
    // *********************************************************************************
    // **     This is the shutdown point of the program                               **
    // *********************************************************************************
    // *********************************************************************************

    logmsg(LOG_INFO, "Received signal %d. Shutting down daemon", received_signal);
    
    logmsg(LOG_DEBUG, "Saving geocache statistics and cache vectors" );
    
    int rc=write_address_geocache();
    if( 0==rc ) {
      logmsg(LOG_INFO, "Saved address geocache" );
    } else {
      logmsg(LOG_ERR, "Could NOT save address geocache" );
    }

    rc=write_minimap_geocache();    
    if( 0==rc ) {
      logmsg(LOG_INFO, "Saved minimap geocache" );
    } else {
      logmsg(LOG_ERR, "Could NOT save minimap geocache" );
    }

    rc=write_geocache_stat();
    if( 0==rc ) {
      logmsg(LOG_INFO, "Saved geocache statistics" );
    } else {
      logmsg(LOG_ERR, "Could NOT save geocache statistics" );
    }
    
    // logmsg(LOG_INFO, "Cleaning up and exit" );    
    // delete_lockfile();
    _exit(EXIT_SUCCESS);

}
