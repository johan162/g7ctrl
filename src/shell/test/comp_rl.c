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


// Clear variable section in memory
#define CLEAR(x) memset (&(x), 0, sizeof(x))

#define FALSE (0)
#define TRUE (-1)

#define SHELL_PROMPT "% "

#define HISTORY_FILE ".rlhistory"
#define HISTORY_LENGTH 100

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


char *cmd_list[] = {
  "get","set","do","help","db",
  "preset","nick",  
  ".date",".cachestat",".usb",".target",".ver",".lc",".ld",
  ".address",".table",".nick",".ln",".dn",".ratereset",
  (char *)NULL
};

char *db_cmd_list[] = {
  "help", "deletelocations", "tail", "head",
  "mailgpx","mailcsv","size","dist","export",
  "mailpos","lastloc","sort",
  (char *)NULL
};

char *do_cmd_list[] = {
  "test", "clrec", "dlrec", "reboot", "reset",  
  (char *)NULL
};

char *get_cmd_list[] = {
  "address","ver", "locg","gfevt","phone",
  "roam","led","gfen","sleep","loc",
  "imei","sim","ver","nrec","batt","track","mswitch","tz",
  "sms","comm","vip","ps","config","rec","lowbatt","sens",
  (char *)NULL
};

char *set_cmd_list[] = {
  "tz","roam","led","gfen","sleep","loc","phone",
  "imei","sim","ver","nrec","batt","track","mswitch","tz",
  "sms","comm","vip","ps","config","rec","lowbatt","sens",
  (char *)NULL
};

char *preset_cmd_list[] = {
  "use", "list", "refresh","help",
  (char *)NULL
};

char *nick_cmd_list[] = {
  "use", 
  (char *)NULL
};



/**
 * @param text
 * @param state
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


/**
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* db_cmd_generator(const char* text, int state)
{
    static int db_cmd_list_index, db_cmd_len;
    char *name;
 
    if (0==state) {
        db_cmd_list_index = 0;
        db_cmd_len = strlen(text);
    }
 
    // Find out which next command matches
    // up to cmd_len characters
    while ( NULL != (name = db_cmd_list[db_cmd_list_index]) ) {
        db_cmd_list_index++;
 
        if (strncmp (name, text, db_cmd_len) == 0)
            return (strdup(name));
    }
 
    /* If no names matched, then return NULL. */
    return ((char *)NULL);
 
}



/**
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* do_cmd_generator(const char* text, int state)
{
    static int do_cmd_list_index, do_cmd_len;
    char *name;
 
    if (0==state) {
        do_cmd_list_index = 0;
        do_cmd_len = strlen(text);
    }
 
    // Find out which next command matches
    // up to cmd_len characters
    while ( NULL != (name = do_cmd_list[do_cmd_list_index]) ) {
        do_cmd_list_index++;
 
        if (strncmp (name, text, do_cmd_len) == 0)
            return (strdup(name));
    }
 
    /* If no names matched, then return NULL. */
    return ((char *)NULL);
 
}

/**
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* nick_cmd_generator(const char* text, int state)
{
    static int nick_cmd_list_index, nick_cmd_len;
    char *name;
 
    if (0==state) {
        nick_cmd_list_index = 0;
        nick_cmd_len = strlen(text);
    }
 
    // Find out which next command matches
    // up to cmd_len characters
    while ( NULL != (name = nick_cmd_list[nick_cmd_list_index]) ) {
        nick_cmd_list_index++;
 
        if (strncmp (name, text, nick_cmd_len) == 0)
            return (strdup(name));
    }
 
    /* If no names matched, then return NULL. */
    return ((char *)NULL);
 
}


/**
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* preset_cmd_generator(const char* text, int state)
{
    static int preset_cmd_list_index, preset_cmd_len;
    char *name;
 
    if (0==state) {
        preset_cmd_list_index = 0;
        preset_cmd_len = strlen(text);
    }
 
    // Find out which next command matches
    // up to cmd_len characters
    while ( NULL != (name = preset_cmd_list[preset_cmd_list_index]) ) {
        preset_cmd_list_index++;
 
        if (strncmp (name, text, preset_cmd_len) == 0)
            return (strdup(name));
    }
 
    /* If no names matched, then return NULL. */
    return ((char *)NULL);
 
}


/**
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* get_cmd_generator(const char* text, int state)
{
    static int get_cmd_list_index, get_cmd_len;
    char *name;
 
    if (0==state) {
        get_cmd_list_index = 0;
        get_cmd_len = strlen(text);
    }
 
    // Find out which next command matches
    // up to cmd_len characters
    while ( NULL != (name = get_cmd_list[get_cmd_list_index]) ) {
        get_cmd_list_index++;
 
        if (strncmp (name, text, get_cmd_len) == 0)
            return (strdup(name));
    }
 
    /* If no names matched, then return NULL. */
    return ((char *)NULL);
 
}

/**
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* set_cmd_generator(const char* text, int state)
{
    static int set_cmd_list_index, set_cmd_len;
    char *name;
 
    if (0==state) {
        set_cmd_list_index = 0;
        set_cmd_len = strlen(text);
    }
 
    // Find out which next command matches
    // up to cmd_len characters
    while ( NULL != (name = set_cmd_list[set_cmd_list_index]) ) {
        set_cmd_list_index++;
 
        if (strncmp (name, text, set_cmd_len) == 0)
            return (strdup(name));
    }
 
    /* If no names matched, then return NULL. */
    return ((char *)NULL);
 
}


/**
 */
static char** cmd_completion( const char * text , int start,  int end)
{
    char **matches;
    matches = (char **)NULL;

    if( 0==strncmp("db ",rl_line_buffer,3) ) {
      if (strlen(rl_line_buffer) > 5) {
	// If we have a complete word after 'get' then we don't do any completion
	// this is indicated by an empty text since the cursor would be at the
	// space after the word.
	if( *text )
	  matches = rl_completion_matches ((char*)text, &db_cmd_generator);
      } else {
	matches = rl_completion_matches ((char*)text, &db_cmd_generator);
      }
    }
    else if( 0==strncmp("do ",rl_line_buffer,3) ) {
      if (strlen(rl_line_buffer) > 4) {
	// If we have a complete word after 'get' then we don't do any completion
	// this is indicated by an empty text since the cursor would be at the
	// space after the word.
	if( *text )
	  matches = rl_completion_matches ((char*)text, &do_cmd_generator);
      } else {
	matches = rl_completion_matches ((char*)text, &do_cmd_generator);
      }
    }
    else if( 0==strncmp("set ",rl_line_buffer,4) ) {
      if (strlen(rl_line_buffer) > 5) {
	// If we have a complete word after 'get' then we don't do any completion
	// this is indicated by an empty text since the cursor would be at the
	// space after the word.
	if( *text )
	  matches = rl_completion_matches ((char*)text, &set_cmd_generator);
      } else {
	matches = rl_completion_matches ((char*)text, &set_cmd_generator);
      }
    }
    else if( 0==strncmp("get ",rl_line_buffer,4) ) {
      if (strlen(rl_line_buffer) > 5) {
	// If we have a complete word after 'get' then we don't do any completion
	// this is indicated by an empty text since the cursor would be at the
	// space after the word.
	if( *text )
	  matches = rl_completion_matches ((char*)text, &get_cmd_generator);
      } else {
	matches = rl_completion_matches ((char*)text, &get_cmd_generator);
      }
    }
    else if( 0==strncmp("preset ",rl_line_buffer,7) ) {
      if (strlen(rl_line_buffer) > 8) {
	// If we have a complete word after 'get' then we don't do any completion
	// this is indicated by an empty text since the cursor would be at the
	// space after the word.
	if( *text )
	  matches = rl_completion_matches ((char*)text, &preset_cmd_generator);
      } else {
	matches = rl_completion_matches ((char*)text, &preset_cmd_generator);
      }
    }
    else if( 0==strncmp("nick ",rl_line_buffer,5) ) {
      if (strlen(rl_line_buffer) > 6) {
	// If we have a complete word after 'get' then we don't do any completion
	// this is indicated by an empty text since the cursor would be at the
	// space after the word.
	if( *text )
	  matches = rl_completion_matches ((char*)text, &nick_cmd_generator);
      } else {
	matches = rl_completion_matches ((char*)text, &nick_cmd_generator);
      }
    }
    else
      matches = rl_completion_matches ((char*)text, &cmd_generator);
      //    else
      //      rl_bind_key('\t',rl_abort);
 
    return (matches);
 
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
    rl_attempted_completion_function = cmd_completion;

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

