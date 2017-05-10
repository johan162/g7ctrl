/* =========================================================================
 * File:        RL_COMP_EX.C
 * Description: Example of using commmand completion with the readline
 *              library together with the async interface
 *
 * Author:      Johan Persson (johan162@gmail.com)
 *
 * Compile wih: gcc --std=gnu99 -Wall -Wextra rl_comp_ex.c -lreadline 
 *
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
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <sys/time.h>
#include <pwd.h>

// Needed for MAX macro
#include <sys/param.h>

// Readline library (must be compiled with -lreadline)
#include <readline/readline.h>
#include <readline/history.h>

// Clear variable section in memory
#define CLEAR(x) memset (&(x), 0, sizeof(x))

#define FALSE (0)
#define TRUE (-1)

#define SHELL_PROMPT "% "

#define HISTORY_FILE ".rlhistory"
#define HISTORY_LENGTH 100

char hfilename[256];

/**
 * Program version
 */
char prog_version[] = "1.0"; 

/**
 * Flag set by signal handler
 */
volatile sig_atomic_t received_signal;

/**
 * Needed to communicate between callback and main command loop
 */
volatile int quitCmdLoop = FALSE;

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
    if (argc > 1) {
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
                    "g7sh", prog_version, 
                    "Copyright (C) 2015  Johan Persson (johan162@gmail.com)\n"
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
 * @param signo Received signal
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
 * System exit handler 
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

    // Install exit handler
    atexit(exithandler);
}

/**
 * List of possible commands to be completed.
 * Must end with a NULL to mark the end of the array.
 */
char *cmd_list[] = {
  "get","set","do","help","db","myverylongcommand",
  "exit","quit",
  (char *)NULL
};


/**
 * @param text The partial etxt to complete
 * @param state 0 indicates the first time whoch means that we need to initialize
 * @return NULL if no match was found
 */
char* cmd_generator(const char* text, int state)
{
    static int cmd_list_index, cmd_len;
    char *name;
 
    if (0==state) {
        cmd_list_index = 0;
        cmd_len = strlen(text);
    }
 
    // Find out which next command matches
    // up to cmd_len characters
    while ( NULL != (name = cmd_list[cmd_list_index]) ) {
        cmd_list_index++;
 
        if (strncmp (name, text, cmd_len) == 0)
            return (strdup(name));
    }
 
    /* If no names matched, then return NULL. */
    return ((char *)NULL);
 
}


// Supress the warning of the unused start and end arguments to the
// cmd_completion function
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/**
 * The custom command function. This is the main point to manipulate and
 * customize the completion. If a context sensitive apporach is needed
 * then the complete commadn line read so far can be found in the global
 * libary variable 'rl_line_buffer'.
 * @param text The text to be compete
 * @param start The start index in rl_line_buffer of the text to be completed
 * @param end The end index of the text to be completed in rl_line_buffer
 */
static char** cmd_completion( const char * text , int start,  int end)
{
    char **matches = (char **)NULL;

    // Disable the deafult file name completion
    rl_attempted_completion_over=1;

    // Generate the matches
    matches = rl_completion_matches ((char*)text, &cmd_generator);
    
    return (matches);
 
}
#pragma GCC diagnostic pop
 
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
 * Main command loop. This shows the use of the asynchroneus interface which allows
 * prgram actions between each key stroke and and avoid the program getting stuck
 * witing for user input.
 */
void
cmd_loop(void) {

    int rc;
    fd_set read_fdset;
    struct timeval timeout;

    // Install the asynchroneus callback function
    rl_callback_handler_install(SHELL_PROMPT,cb_rl_readline);

    // Install our custom command completion function
    rl_attempted_completion_function = cmd_completion;

    // This loop is terminated by listening to the global variable
    // quitCmdLoop that gets set by the callback on receiving an "exit"
    // or "quit" command.
    do {

        FD_ZERO(&read_fdset);
        FD_SET(STDIN_FILENO, &read_fdset);

        timerclear(&timeout);
        timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	// Wait for users key-press or until we receive a timeout
        rc = select(STDIN_FILENO + 1, &read_fdset, NULL, NULL, &timeout);

        if( 0 == rc ) {

	  // Do something in between entering characters or on time-outs
	  // This is purely appliction dependent. If you are talking to a server
	  // this would for example be a good point to check if the server has 
	  // disconnected.

	  // ...... do something .......
            
        } else if ( rc < 0 ) {

            fprintf(stderr, "Communication problem. Connection closed ( %s ).\n",strerror(errno));
            break;

        } else {

	  // A valid keypress so tell readline to do its thing
	  rl_callback_read_char();

	}

    } while( ! quitCmdLoop );

    // Cleanup handlers
    rl_callback_handler_remove();

    // ... and end with a blank line
    fprintf(stdout,"\n");
}

/**
 * Main entry point for command completion example
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

