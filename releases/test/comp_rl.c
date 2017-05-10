/* =========================================================================
 * File:        TEST_RL.C
 * Description: A basic shell interface for the g7ctrl daemon. This will
 *              allow a setup whereby this shell is specified as a users
 *              "normal" login shell. This way you can connect to the
 *              daemon by logging in to the server using ssh or telnet as
 *              that user.
 *
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: g7shell.c 944 2015-04-08 21:20:25Z ljp $
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
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <sys/time.h>
#include <pwd.h>
#include <stdarg.h>


#include <sys/types.h>
#include <sys/stat.h>

// Needed for MAX macro
#include <sys/param.h>

// Readline library (must be compiled with -lreadline)
#include <readline/readline.h>
#include <readline/history.h>

// Since these defines are supposed to be defined directly in the linker using
// --defsym to step the build number at each build and apples linker do not support
// this we just give them a 0 number here and never use them.
// This also means that on OSX the build number displayed is meaningless
// TODO. Remove all references to build number when running on OSX
#ifdef __APPLE__
char   __BUILD_DATE = '\0';
char   __BUILD_NUMBER = '\0';
#endif

// Clear variable section in memory
#define CLEAR(x) memset (&(x), 0, sizeof(x))

#define FALSE (0)
#define TRUE (-1)

#define DEFAULT_CMD_PORT 3100
#define DEFAULT_SERVER "127.0.0.1"

#define SHELL_PROMPT "% "

#define HISTORY_FILE ".g7ctrl_history"
#define HISTORY_LENGTH 100

#define LEN_10K (10*1024)
#define LEN_100K (100*1024)
#define LEN_10M (1000*1024*10)

char hfilename[256];

/**
 * Server identification
 */
char shell_version[] = "1.0"; 

/**
 * Flag set by signal handler
 */
volatile sig_atomic_t received_signal;


/**
 * Handling of arguments to the server
 */
static const char short_options [] = "hv";
static const struct option long_options [] = {
    { "help", no_argument, NULL, 'h'},
    { "version", no_argument, NULL, 'v'},
    { 0, 0, 0, 0}
};

/**
 * Trim a string in-place by removing beginning and ending spaces
 * @param str String to trim
 */
void
xstrtrim(char *str) {
    char *tmp = strdup(str), *startptr = tmp;
    int n = strlen(str);
    char *endptr = tmp + n - 1;

    while (*startptr == ' ') {
        startptr++;
    }

    while (n > 0 && *endptr == ' ') {
        --n;
        --endptr;
    }

    while (startptr <= endptr) {
        *str++ = *startptr++;
    }

    *str = '\0';
    free(tmp);
}

/**
 * Parse all command line options given to the server at startup. The server accepts both
 * long and short version of command line options.
 * @param argc
 * @param argv
 */
void
parsecmdline(int argc, char **argv) {

    // Parse command line options
    int opt, idx;
    opterr = 0; // Supress error string from getopt_long()
    if (argc > 5) {
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

    while (-1 != (opt = getopt_long(argc, argv, short_options, long_options, &idx))) {

        switch (opt) {
            case 0: /* getopt_long() flag */
                break;

            case 'h':
                fprintf(stdout,"Test completion of readline\n");
                exit(EXIT_SUCCESS);
                break;

            case 'v':

                fprintf(stdout, "%s %s\n%s",
                    "g7sh", shell_version, 
                    "Copyright (C) 2013-2015  Johan Persson (johan162@gmail.com)\n"
                    "This is free software; see the source for copying conditions.\nThere is NO "
                    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");

                exit(EXIT_SUCCESS);
                break;

            case ':':
                fprintf(stderr, "Option `%c' needs an argument.\n", optopt);
                exit(EXIT_FAILURE);
                break;

            case '?':
                fprintf(stderr, "Invalid specification of program option(s). See --help for more information.\n");
                exit(EXIT_FAILURE);
                break;
        }
    }

    if (argc > 1 && optind < argc) {
        fprintf(stderr, "Options not valid.\n");
        exit(EXIT_FAILURE);
    }

}


/**
 * Global signal handler. We catch SIGHUP, SIGINT and SIGABRT
 * @param signo
 */
void
sighandler(int signo) {
    received_signal = signo;

    rl_callback_handler_remove();
    rl_cleanup_after_signal();    
    write_history(hfilename);
    history_truncate_file(hfilename, HISTORY_LENGTH);
    fprintf(stdout,"\n");    
    _exit(EXIT_SUCCESS);
}

/**
 * System exit handler for g7sh (empty)
 */
void
exithandler(void) {
    /* empty */
}

/**
 * Setup signal handlers
 */
void
setup_sighandlers(void) {

    // Block all signals //except SIGINT
    sigset_t mysigset;
    sigfillset(&mysigset);
    sigdelset(&mysigset, SIGINT);
    sigprocmask(SIG_SETMASK, &mysigset, NULL);

    // Setup SIGINT handler
    struct sigaction act;
    CLEAR(act);
    act.sa_handler = &sighandler;
    act.sa_flags = SA_RESTART;
    sigaction(SIGINT, &act, (struct sigaction *) NULL);
    atexit(exithandler);
}



/**
 * Needed to communicate between callback and main command loop
 */
volatile int quitCmdLoop = FALSE;
/**
 * Callback function for the asynchronous version of readline. This callback
 * is activated when a full line has been read from the terminal
 * @param line Line entered by user
 */
void
cb_rl_readline(char *line) {

    if (NULL == line) {
        quitCmdLoop = TRUE;
        return;
    }

    xstrtrim(line);
    if ((0 == strncmp(line, "exit", 4)) || (0 == strncmp(line, "quit", 4))) {
        quitCmdLoop = TRUE;
        free(line);
        return;
    }

    if (line) {
        
        add_history(line);

        fprintf(stdout,"Entered command: \"%s\"\n",line);
        
        free(line);
        
    } else {
        
        fprintf(stdout,"(EMPTY LINE)\n");
        
    }
}




/**
 * Main command loop. We use the asynchronous version of readline to be able
 * to quickly detect a disconnected remote server.
 */
void
cmd_loop(void) {

    int rc;
    fd_set read_fdset;
    struct timeval timeout;
    rl_callback_handler_install(SHELL_PROMPT,cb_rl_readline);

    do {

        FD_ZERO(&read_fdset);
        FD_SET(STDIN_FILENO, &read_fdset);

        timerclear(&timeout);
        timeout.tv_sec = 1;
	timeout.tv_usec = 0;

        rc = select(STDIN_FILENO + 1, &read_fdset, NULL, NULL, &timeout);

        if( 0 == rc ) {

            // Do something in between entering characters
            
        } else if ( rc < 0 ) {

            fprintf(stderr, "Communication problem. Connection closed ( %s ).\n",strerror(errno));
            break;

        } else {

	  rl_callback_read_char();
	}

    } while( ! quitCmdLoop );

    rl_callback_handler_remove();
    fprintf(stdout,"\n");
}

/**
 * Main entry point for g7sh
 * @param argc Argument count
 * @param argv Argument vector
 * @return EXIT_SUCCESS at program termination
 */
int
main(int argc, char **argv) {
    parsecmdline(argc, argv);
    setup_sighandlers();

    // Setup history file by first finding the user home directory
    // This is done by first checking for environment HOME and if that
    // fails get the pw structure.
    const char *homedir = getenv("HOME");
    if (0 == strnlen(homedir, 256)) {
        struct passwd *pw = getpwuid(getuid());
        homedir = pw->pw_dir;
    }
    snprintf(hfilename, sizeof (hfilename), "%s/%s", homedir, HISTORY_FILE);

    // Try to find old history file
    read_history(hfilename);

    // The command loop is active until the server disconnects or the user
    // quits the command shell
    cmd_loop();

    // Update history file for next time
    write_history(hfilename);

    // We only keep the HISTORY_LENGTH last commands
    history_truncate_file(hfilename, HISTORY_LENGTH);

    // ... and exit in good standing
    _exit(EXIT_SUCCESS);

}

/* EOF */

