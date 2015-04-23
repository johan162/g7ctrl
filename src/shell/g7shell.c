/* =========================================================================
 * File:        G7SHELL.C
 * Description: A basic shell interface for the g7ctrl daemon. This will
 *              allow a setup whereby this shell is specified as a users
 *              "normal" login shell. This way you can connect to the
 *              daemon by logging in to the server using ssh or telnet as
 *              that user.
 *
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

// Application specific libs, parse inifiles
#ifdef HAVE_LIBINIPARSER
#include <iniparser.h>
#else
#include "../libiniparser/iniparser.h"
#endif

// Needed for getaddrinfo()
#include <netdb.h>

// Needed for MAX macro
#include <sys/param.h>

// Readline library (must be compiled with -lreadline)
#include <readline/readline.h>
#include <readline/history.h>

// Local header files
#include "../xstr.h"
#include "../config.h"
#include "../build.h"

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

#define SHELL_PROMPT "g7ctrl> "
#define SHELL_PROMPT_ARG "(arg)>> "


#define HISTORY_FILE ".g7ctrl_history"
#define HISTORY_LENGTH 100

#define LEN_10K (10*1024)
#define LEN_100K (100*1024)
#define LEN_10M (1000*1024*10)

char hfilename[256];

/**
 * client socket. Needed for communication between command loop and
 * readline asynchronous callback.
 */
int cli_sockd = -1;


/**
 * Server identification
 */
char shell_version[] = PACKAGE_VERSION; // This define gets set by the config process

/**
 * Config variables read from ini file
 */
static int tcpip_port = -1; // Read from INI-file
static char g7ctrl_pwd[128] = {'\0'}; // Read from INI-file

/**
 * Address to connect to (the ip of the g7ctrl server)
 */
static char *server_ip = NULL; // Localhost

/**
 * Flag set by signal handler
 */
volatile sig_atomic_t received_signal;

/**
 * dict
 * Holds the read dictionary from the inifile
 */
dictionary *dict = NULL;

/**
 * Single shot command line for "-e" argument
 */
char *singleCmdLine;
_Bool singleCmd=0;


/**
 * Handling of arguments to the server
 */
static const char short_options [] = "hvs:p:e:";
static const struct option long_options [] = {
    { "help", no_argument, NULL, 'h'},
    { "version", no_argument, NULL, 'v'},
    { "server", required_argument, NULL, 's'},
    { "port", required_argument, NULL, 'p'},
    { "exec", required_argument, NULL, 'e'},
    { 0, 0, 0, 0}
};


/*
 * Command lists for completion
 */

char *cmd_list[] = {
    "get", "set", "do", "help", "db",
    "preset", ".date", ".cachestat", ".usb", ".target", ".ver", ".lc", ".ld",
    ".address", ".table", ".nick", ".ln", ".dn", ".ratereset",
    "exit", "quit",
    (char *) NULL
};

char *db_cmd_list[] = {
    "deletelocations", "tail", "head",
    "mailgpx", "mailcsv", "size", "dist", "export",
    "mailpos", "lastloc", "sort",
    (char *) NULL
};

char *dbsort_cmd_list[] = {
    "device", "arrival",
    (char *) NULL     
};

char *do_cmd_list[] = {
    "test", "clrec", "dlrec", "reboot", "reset",
    (char *) NULL
};

char *get_cmd_list[] = {
    "address", "ver", "locg", "gfevt", "phone",
    "roam", "led", "gfen", "sleep", "loc",
    "imei", "sim", "ver", "nrec", "batt", "track", "mswitch", "tz",
    "sms", "comm", "vip", "ps", "config", "rec", "lowbatt", "sens",
    (char *) NULL
};

char *set_cmd_list[] = {
    "roam", "led", "gfen", "sleep", "phone",
    "track", "mswitch", "tz","sms", "comm", 
    "vip", "ps", "config", "rec", "lowbatt", 
    "sens",
    (char *) NULL
};

char *preset_cmd_list[] = {
    "use", "list", "refresh", "help",
    (char *) NULL
};

char *onoff_cmd_list[] = {
    "on", "off", 
    (char *) NULL
};

char *help_cmd_list[] = {
    "db", "preset", ".date", ".cachestat", ".usb", 
    ".target", ".ver", ".lc", ".ld", ".address", ".table", ".nick", 
    ".ln", ".dn", ".ratereset",
    "address", "ver", "locg", "gfevt", "phone",
    "roam", "led", "gfen", "sleep", "loc",
    "imei", "sim", "ver", "nrec", "batt", "track", "mswitch", "tz",
    "sms", "comm", "vip", "ps", "config", "rec", "lowbatt", "sens",
    "test", "clrec", "dlrec", "reboot", "reset",    
    (char *) NULL
};

int _writef(int fd, const char *buf, ...) __attribute__ ((format(printf,2,3)));

/**
 * _writef
 * Utility function
 * Simplify a formatted write to a file descriptor
 *
 * @param fd Descriptor to write to
 * @param buf Format string
 * @param ... Argument to
 * @return < 0 failure, >= 0 number of chars written
 */
int
_writef(int fd, const char *buf, ...) {
    if( fd >= 0 ) {
        const size_t blen = LEN_100K;
        char *tmpbuff = calloc(blen,1);
        va_list ap;
        va_start(ap, buf);
        vsnprintf(tmpbuff, blen, buf, ap);
        tmpbuff[blen-1] = 0;
        int rc = write(fd, tmpbuff, strnlen(tmpbuff,blen));
        free(tmpbuff);
        va_end(ap);
        return rc;
    }
    return -1;
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
                fprintf(stdout,
                    "(C) 2013-2015 Johan Persson, (johan162@gmail.com) \n"
                    "This is free software; see the source for copying conditions.\nThere is NO "
                    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
                    "Synopsis:\n"
                    "'%s' - Interactive shell for g7ctrl.\n"
                    "Usage: %s [-h] [-v] [-s ipaddress] [-p port]\n"
                    "Options:\n"
                    " -h, --help      Print help and exit\n"
                    " -e --exec cmd   Execute a command and terminate\n"
                    " -v, --version   Print version string and exit\n"
                    " -s, --server    Specify IP address (default=%s)\n"
                    " -p, --port      Specify TCP/IP port (default=%d)\n",
                    "g7sh", "g7sh", DEFAULT_SERVER, DEFAULT_CMD_PORT);
                exit(EXIT_SUCCESS);
                break;

            case 'v':
#ifdef __APPLE__
                fprintf(stdout, "%s %s\n%s",
                    "g7sh", shell_version,
                    "Copyright (C) 2013-2015  Johan Persson (johan162@gmail.com)\n"
                    "This is free software; see the source for copying conditions.\nThere is NO "
                    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
#else
                fprintf(stdout, "%s %s (build: %lu-%lu)\n%s",
                    "g7sh", shell_version, (unsigned long) &__BUILD_DATE, (unsigned long) &__BUILD_NUMBER,
                    "Copyright (C) 2013-2015  Johan Persson (johan162@gmail.com)\n"
                    "This is free software; see the source for copying conditions.\nThere is NO "
                    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");

#endif
                exit(EXIT_SUCCESS);
                break;

            case 's':
                server_ip = strdup(optarg);
                break;

            case 'e':
                singleCmd = 1;
                singleCmdLine = strdup(optarg);
                break;

            case 'p':
                tcpip_port = xatoi(optarg);
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
    if (server_ip == NULL) {
        server_ip = strdup(DEFAULT_SERVER);
    }

}

/**
 * Global signal handler. We catch SIGHUP, SIGINT and SIGABRT
 * @param signo
 */
void
sighandler(int signo) {
    received_signal = signo;

    (void) shutdown(cli_sockd, SHUT_RDWR);
    (void) close(cli_sockd);
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
 * Utility variant of the standard read() function which handles signal
 * interruption and connection reset in a graceful way. Basically it will
 * resume the read() operation when a temporary error has happened.
 *
 * @param sockd Socket descriptor
 * @param[out] buffer Buffer to hold the read string
 * @param buffLen Maximum length of the storage buffer
 * @return the number of bytes read, 0 on connection close/reset, otherwise
 *          < 0 indicates an error and errno is set.
 */
int
socket_read(const int sockd, void *buffer, const size_t buffLen) {
    int rc;
    while( TRUE ) {
        rc = read(sockd,buffer, buffLen);
        if( (-1 == rc) && (EINTR == errno) ) {
            continue;
        } else {
            break;
        }
    }
    return rc;
}

/**
 * Read a reply from a socket with a 25s timeout.
 *
 * @param sock Socket to read from
 * @param buffer Buffer for reply
 * @param maxbufflen Maximum reply length
 * @param nread number of chars read
 * @return 0 on success, -3 timeout, -2 buffer to small for reply, -1 other error
 */
int
waitread(int sock, char *buffer, int maxbufflen, int *nread) {
    fd_set read_fdset;
    struct timeval timeout;

    FD_ZERO(&read_fdset);
    FD_SET(sock, &read_fdset);

    // 5s timeout
    timerclear(&timeout);
    timeout.tv_sec = 180; // 3 min timeout (updating the DB can take a long time between updates if we are doing location lookups)
    timeout.tv_usec = 0;
    int rc = select(sock + 1, &read_fdset, NULL, NULL, &timeout);
    if (0 == rc) {
        // Timeout
        *nread = 0;
        *buffer = '\0';
        return -3;
    } else if( rc < 0 ) {
        return -1;
    } else {
        *nread = socket_read(sock, buffer, maxbufflen);
        if (*nread >= maxbufflen - 1) {
            // Reply is longer than we can receive
            *nread = 0;
            *buffer = '\0';
            return -2;
        } else if (*nread > 0 ) {
            buffer[*nread] = '\0';
        } else {
            *buffer = '\0';
            return -99;
        }
    }
    return 0;
}

#define NUM_RETRIES 3
/**
 * Read reply from readsock until a "\r\n" sequence is found. Write partial
 * replies to writesock as we go if writesock > 0
 * @param readsock Socket/fd to read from
 * @param writesock Socket/fd to write patyal replies from (if writesock == 1)
 * no replies will be written.
 * @param buffer Reply buffer (stores the complete reply)
 * @param maxbufflen Maximum reply buffer
 * @return  0 on success, -1 on failure, -2 on buffer too small to hold reply
 * -3 = connection reset
 */
int
readwrite(int readsock, int writesock, char *buffer, int maxbufflen) {

    const size_t READ_BUFFLEN = LEN_10M;
    char *pbuff = calloc(READ_BUFFLEN, sizeof (char));
    *buffer = '\0';
    int totlen = 0, rc = 0, nread = 0;
    int retries = NUM_RETRIES;
    while (totlen < maxbufflen) {
        rc = waitread(readsock, pbuff, READ_BUFFLEN, &nread);
        if (0 == rc) {
            retries = NUM_RETRIES;
            pbuff[nread] = '\0';

            if (totlen + nread < maxbufflen) {
                strcat(buffer, pbuff);
                totlen += nread;
            } else {
                free(pbuff);
                return -2; // Buffer too small
            }

            // Current output from daemon is finished with a "\r\n" sequence but we
            // don't want to include that in the output to the user
            if (nread >= 2 && '\r' == pbuff[nread - 2] && '\n' == pbuff[nread - 1]) {
                pbuff[nread - 2] = '\0';
                _writef(writesock, "%s\n", pbuff);
                break;
            } else if (writesock > 0) {
                // Write back a partial response
                _writef(writesock, "%s", pbuff);
            }

        } else if (-3 == rc) {
            // Try 3 times when we get a timeout
            retries--;
            if (retries <= 0) {
                free(pbuff);
                return -1;
            }
        } else if ( rc < 0 ) {
            free(pbuff);
            if( -99 == rc )
                return -3;
            else
                return -1;
        }
    }

    buffer[maxbufflen - 1] = '\0';
    free(pbuff);
    return 0;
}


static const char * G7CTRL_PASSWORD_LABEL = "Password:";
static const char * G7CTRL_AUTHENTICATION_FAILED = "Authentication failed.";

/**
 * Initialize communication with device. Handle potential passwords question
 * @param[out] sockd Return socket to use for communication with device
 * @return 0 success, -1 failure
 */
int
g7ctrl_initcomm(int *sockd) {
    struct addrinfo hints;
    struct addrinfo *servinfo; // will point to the results
    char service[32];
    char reply[1024] = {'\0'};

    CLEAR(hints);
    hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    snprintf(service, sizeof (service), "%d", tcpip_port);

    int status = getaddrinfo(server_ip, service, &hints, &servinfo);
    if (status) {
        fprintf(stderr, "Cannot get address info from [%s:%d]\n", server_ip, tcpip_port);
        freeaddrinfo(servinfo);
        return -1;
    }

    *sockd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (-1 == *sockd) {
        fprintf(stderr, "Cannot create socket for endpoint [%s:%d]\n", server_ip, tcpip_port);
        freeaddrinfo(servinfo);
        return -2;
    }


    if (-1 == connect(*sockd, servinfo->ai_addr, servinfo->ai_addrlen)) {
        shutdown(*sockd, SHUT_RDWR);
        close(*sockd);
        fprintf(stderr, "Cannot connect to [%s:%d]\n", server_ip, tcpip_port);
        freeaddrinfo(servinfo);
        return -3;
    }

    freeaddrinfo(servinfo);

    // Read back the server startup message

    int rc = readwrite(*sockd, -1, reply, sizeof (reply));
    if (-1 == rc || -2 == rc) {
        shutdown(*sockd, SHUT_RDWR);
        close(*sockd);
        return -4;
    }
    int finished = FALSE;
    char *buffer;
    if (0 == strncmp(reply, G7CTRL_PASSWORD_LABEL, strlen(G7CTRL_PASSWORD_LABEL))) {
        do {
            xstrtrim_crnl(reply);
            fprintf(stdout, "%s ", reply);
            if (NULL == (buffer = readline(""))) {
                finished = TRUE;
            } else {
                xstrtrim_crnl(buffer);
                _writef(*sockd, "%s\r\n", buffer);
                rc = readwrite(*sockd, -1, reply, sizeof (reply));
                if (-1 == rc || -2 == rc || -3 == rc ) {
                    shutdown(*sockd, SHUT_RDWR);
                    close(*sockd);
                    return -4;
                }
                finished = strncmp(reply, G7CTRL_PASSWORD_LABEL, strlen(G7CTRL_PASSWORD_LABEL));
                if (buffer)
                    free(buffer);
            }

        } while (!finished);

    }

    // Check if this was a authentication failed and in that case quit

    if (0 == strncmp(reply, G7CTRL_AUTHENTICATION_FAILED, strlen(G7CTRL_AUTHENTICATION_FAILED))) {
        fprintf(stderr, "%s\n", G7CTRL_AUTHENTICATION_FAILED);
        (void) close(*sockd);
        return -1;
    }
    // Suppress the initial server response if we are doing single commands
    if( ! singleCmd )
        fprintf(stdout, "%s", reply);
    return 0;
}

/**
 * Drain all received data on a socket until timeout which is interpretated as
 * no more data. Note
 * @param sockd Socket descriptor to drain
 */
void
sockdrain(int sockd) {
    fd_set read_fdset;
    struct timeval timeout;
    char *buff = calloc(LEN_100K,sizeof(char));
    
    do {

        FD_ZERO(&read_fdset);
        FD_SET(sockd, &read_fdset);
        timerclear(&timeout);
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000; // 0.2 s timeout
        int rc = select(sockd + 1, &read_fdset, NULL, NULL, &timeout);
        if (0 == rc) {
            // Timeout - but the the socket is still alive
            free(buff);
            return ;
        }
        socket_read(sockd,buff,LEN_100K);        
        
    } while( 1 );
}

/**
 * Check if the remote has has closed the socket (unexpectedly)
 * This could happen when the server times out incase the user
 * waits really, really long between interaction points.
 * @param sockd Socket to use with communication with device
 * @return 0 if the socket is alive, <> 0 if it is closed.
 */
int
is_disconnected(int sockd) {
    // Detect if remote socket has disconnected. Due to the
    // way Unix socket works the only way to detect a remote socket
    // that is closed is to try to do a read on the socket
    // and if it has been closed it will return 0 bytes read.
    fd_set read_fdset;
    struct timeval timeout;

    FD_ZERO(&read_fdset);
    FD_SET(sockd, &read_fdset);

    timerclear(&timeout);
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 0.1 s timeout
    int rc = select(sockd + 1, &read_fdset, NULL, NULL, &timeout);
    if (0 == rc) {
        // Timeout - but the the socket is still alive
        return 0;
    }
    // We have some data so this could either be a disconnect notification or
    // some data to find out what we need to try to read the stream but
    // add the MSG_PEEK so not to remove any chars from the socket.
    char buff;
    const int nread = recv(sockd, &buff, sizeof (char), MSG_PEEK);
    return nread == 0;
}

/**
 * Send a specified command to the g7ctrl server
 * @param sockd Client socket
 * @param cmd Command to send
 * @param reply Reply received back
 * @param maxreplylen Max length of reply buffer
 * @return 0 on success, -1 on failure, -3 on command reset
 */
int
g7ctrl_command(int sockd, char *cmd, char *reply, int maxreplylen) {

    // Send the command
    char tmpbuff[512];
    snprintf(tmpbuff, sizeof (tmpbuff), "%s\r\n", cmd);

    if (is_disconnected(sockd)) {
        return -3;
    }
    int rc = 0;
    ssize_t nw = write(sockd, tmpbuff, strlen(tmpbuff));
    if (nw != (ssize_t) strlen(tmpbuff)) {
        rc = -1;
    } else {
        rc = readwrite(sockd, STDOUT_FILENO, reply, maxreplylen);
    }
    return rc;

}

/**
 * Find out the name of the config file
 * @return 0 on success, -1 on failure
 */
int
setup_inifile(void) {
    char inifile[256];
    snprintf(inifile, sizeof (inifile), "%s/%s/%s", CONFDIR, PACKAGE, PACKAGE_NAME ".conf");
    inifile[sizeof (inifile)-1] = '\0';

    // Close stderr to avoid error message from braindead iniparser_load())
    close(STDERR_FILENO);
    dict = iniparser_load(inifile);
    dup2(STDOUT_FILENO,STDERR_FILENO);

    if (NULL == dict) {
       return -1;
    }
    return 0;
}

/**
 * Read the values we need from the daemons config file
 * @return 0 on success, -1 on failure
 */
int
read_inifile(void) {
    if (dict) {
        if (tcpip_port == -1) {
            tcpip_port = iniparser_getint(dict, "config:cmd_port", DEFAULT_CMD_PORT);
        }
        strncpy(g7ctrl_pwd, iniparser_getstring(dict, "config:client_pwd", ""), 127);
        g7ctrl_pwd[127] = '\0';
        return 0;
    } else {
        return -1;
    }
}


/**
 * Completion generator basic commands
 * @param text
 * @param state
 * @return NULL if no match was found
 */
static char* 
_generator(const char* text, int state, char **cmdlist) {
    static int list_index, len;
    char *name;

    if (0 == state) {
        list_index = 0;
        len = strlen(text);
    }

    // Find out which next command matches
    // up to len characters
    while (NULL != (name = cmdlist[list_index])) {
        list_index++;

        if (strncmp(name, text, len) == 0)
            return (strdup(name));
    }

    /* If no names matched, then return NULL. */
    return ((char *) NULL);

}

/**
 * Completion generator basic commands
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* 
cmd_generator(const char* text, int state) {
    return _generator(text,state,cmd_list);
}

/**
 * Completion generator database commands
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* 
db_cmd_generator(const char* text, int state) {
    return _generator(text,state,db_cmd_list);
}

/**
 * Completion generator database sort commands
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* 
dbsort_cmd_generator(const char* text, int state) {
    return _generator(text,state,dbsort_cmd_list);    
}

/**
 * Completion generator "do" commands
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* 
do_cmd_generator(const char* text, int state) {
   return _generator(text,state,do_cmd_list);
}

/**
 * Completion generator preset commands
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* 
preset_cmd_generator(const char* text, int state) {
    return _generator(text,state,preset_cmd_list);
}

/**
 * Completion generator "get" commands
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* 
get_cmd_generator(const char* text, int state) {
    return _generator(text,state,get_cmd_list);    
}

/**
 * Completion generator "set" commands
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* 
set_cmd_generator(const char* text, int state) {
    return _generator(text,state,set_cmd_list);
}

/**
 * Completion generator "help" commands
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* 
help_cmd_generator(const char* text, int state) {
    return _generator(text,state,help_cmd_list);
}

/**
 * Completion generator "on/off" commands
 * @param text
 * @param state
 * @return NULL if no match was found
 */
char* 
onoff_cmd_generator(const char* text, int state) {
    return _generator(text,state,onoff_cmd_list);
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
/**
 * Readline entry point for completion function. The reason for calling multiple
 * generators is that depending on the context the user is restricted to only
 * a subset of all possible commands. For example, if the user has started with
 * "db " then only the database commands are given as options. Once the user
 * as entered two parts of a command words no more completions should be possible.
 * This is handled by the code that checks if there are more characters than minimum
 * entered and at the same time the "text" argument is empty then we don't call
 * any generator function and just return NULL.
 * The entire line buffer is found in the "rl_line_buffer" variable.
 * @param text The partial text that should be completed
 * @param start Start index in line buffer of partial text to match
 * @param end End index in line buffer of partial text to match
 * @return A list of matches, NULL if no matches
 */
static char** 
cmd_completion(const char * text, int start, int end) {
    char **matches = (char **) NULL;
    int mlen=0;
    
    // Disable default filename completion
    rl_attempted_completion_over = 1;
    
    if (0 == strncmp("db sort ", rl_line_buffer, 8)) {
        if (strlen(rl_line_buffer) > 9) {
            // If we have a complete word after 'db sort ' then we don't do any completion
            // this is indicated by an empty text since the cursor would be at the
            // space after the word.
            if (*text)
                matches = rl_completion_matches((char*) text, &dbsort_cmd_generator);
        } else {
            matches = rl_completion_matches((char*) text, &dbsort_cmd_generator);
        }
    } else if (0 == strncmp("db ", rl_line_buffer, 3)) {
        if (strlen(rl_line_buffer) > 4) {
            // If we have a complete word after 'db ' then we don't do any completion
            // this is indicated by an empty text since the cursor would be at the
            // space after the word.
            if (*text)
                matches = rl_completion_matches((char*) text, &db_cmd_generator);
        } else {
            matches = rl_completion_matches((char*) text, &db_cmd_generator);
        }
    } else if (0 == strncmp("do ", rl_line_buffer, 3)) {
        if (strlen(rl_line_buffer) > 4) {
            // If we have a complete word after 'do ' then we don't do any completion
            // this is indicated by an empty text since the cursor would be at the
            // space after the word.
            if (*text)
                matches = rl_completion_matches((char*) text, &do_cmd_generator);
        } else {
            matches = rl_completion_matches((char*) text, &do_cmd_generator);
        }
    } else if (
            0 == strncmp("set roam ", rl_line_buffer, (mlen=9) ) ||
            0 == strncmp("set led ", rl_line_buffer, (mlen=8) ) ||
            0 == strncmp("set phone ", rl_line_buffer, (mlen=10) ) 
            ) {
        if ((int)strlen(rl_line_buffer) > mlen+1) {
            // If we have a complete word after 'set ' then we don't do any completion
            // this is indicated by an empty text since the cursor would be at the
            // space after the word.
            if (*text)
                matches = rl_completion_matches((char*) text, &onoff_cmd_generator);
        } else {
            matches = rl_completion_matches((char*) text, &onoff_cmd_generator);
        }        
    } else if (0 == strncmp("set ", rl_line_buffer, 4)) {
        if (strlen(rl_line_buffer) > 5) {
            // If we have a complete word after 'set ' then we don't do any completion
            // this is indicated by an empty text since the cursor would be at the
            // space after the word.
            if (*text)
                matches = rl_completion_matches((char*) text, &set_cmd_generator);
        } else {
            matches = rl_completion_matches((char*) text, &set_cmd_generator);
        }
    } else if (0 == strncmp("get ", rl_line_buffer, 4)) {
        if (strlen(rl_line_buffer) > 5) {
            // If we have a complete word after 'get ' then we don't do any completion
            // this is indicated by an empty text since the cursor would be at the
            // space after the word.
            if (*text)
                matches = rl_completion_matches((char*) text, &get_cmd_generator);
        } else {
            matches = rl_completion_matches((char*) text, &get_cmd_generator);
        }
    } else if (0 == strncmp("preset ", rl_line_buffer, 7)) {
        if (strlen(rl_line_buffer) > 8) {
            // If we have a complete word after 'preset ' then we don't do any completion
            // this is indicated by an empty text since the cursor would be at the
            // space after the word.
            if (*text)
                matches = rl_completion_matches((char*) text, &preset_cmd_generator);
        } else {
            matches = rl_completion_matches((char*) text, &preset_cmd_generator);
        }   
    } else if (0 == strncmp("help db ", rl_line_buffer, 8)) {
        if (strlen(rl_line_buffer) > 9) {
            // If we have a complete word after 'help ' then we don't do any completion
            // this is indicated by an empty text since the cursor would be at the
            // space after the word.
            if (*text)
                matches = rl_completion_matches((char*) text, &db_cmd_generator);
        } else {
            matches = rl_completion_matches((char*) text, &db_cmd_generator);
        }               
    } else if (0 == strncmp("help ", rl_line_buffer, 5)) {
        if (strlen(rl_line_buffer) > 6) {
            // If we have a complete word after 'help ' then we don't do any completion
            // this is indicated by an empty text since the cursor would be at the
            // space after the word.
            if (*text)
                matches = rl_completion_matches((char*) text, &help_cmd_generator);
        } else {
            matches = rl_completion_matches((char*) text, &help_cmd_generator);
        }                
    } else {
        matches = rl_completion_matches((char*) text, &cmd_generator);
    }
    return (matches);
}
#pragma GCC diagnostic pop


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
        //rl_callback_handler_remove();
        return;
    }

    xstrtrim(line);
    if ((0 == strncmp(line, "exit", 4)) || (0 == strncmp(line, "quit", 4))) {
        quitCmdLoop = TRUE;
        //rl_callback_handler_remove();
        free(line);
        return;
    }

    if (line) {
        if (*line) {
            // Don't add only numeric commands since that must be an argument
            // and there is no point of adding arguments to the history
            if (!isdigit(*line))
                add_history(line);
        }
        size_t const maxreplylen = LEN_10M;
        char *reply = calloc(maxreplylen, sizeof (char));
        const int rc = g7ctrl_command(cli_sockd, line, reply, maxreplylen);
        
        // We use a second level prompt to indicate that we are in a command argument
        // input sequence. We know this from the ending question mark in the label
        // sent from the daemon.
        int len = strlen(reply);
        if( len > 3 && reply[len-3]=='?')
            rl_set_prompt(SHELL_PROMPT_ARG);
        else
            rl_set_prompt(SHELL_PROMPT);
        
        // Check if there was an error
        if (rc < 0) {
            if (-3 == rc) {
                fprintf(stderr, "Remote server closed connection.\n");
                quitCmdLoop = TRUE;
            } else {
                fprintf(stderr, "Unknown server error.\n");
            }
        }
        free(reply);
        free(line);
    }
}


/**
 * Execute one single command specified as argument
 * @param command Command string to be executed
 */
int
single_command(const char *command) {
    if (g7ctrl_initcomm(&cli_sockd)) {
        fprintf(stderr, "Cannot initialize communication with server.\n");
        return -1;
    }
    size_t const maxreplylen = 1000 * 1024;
    
    // Split string at ';'
    struct splitfields flds;
    if( -1 == xstrsplitfields(command, ';', &flds) ) {
        const char *errMsg="Syntax error in command string.";
        ssize_t nw = write(cli_sockd,errMsg,strlen(errMsg));
        if( nw != (ssize_t)strlen(errMsg) )
            return -2;
        else
            return -1;
    }
    
    
    char *reply = calloc(maxreplylen, sizeof (char));
    size_t i=0;
    int rc=0;
    while( flds.nf && strlen(flds.fld[i]) > 0 ) {
        rc = g7ctrl_command(cli_sockd, flds.fld[i++], reply, maxreplylen);
        --flds.nf;
    }
    free(reply);
    return rc;
}


/**
 * Read commands from a given file. Each line is supposed to have a single command
 * @param fileName
 * @return -1 on failure, 0 on success
 */
int
commands_from_file(char *fileName) {
    FILE *fp = fopen(fileName,"r");
    if( NULL == fp) {
        fprintf(stderr, "Cannot open command file.\n");
        return -1;
    }
    
    char *line = calloc(256,1);
    while( (line = fgets(line,256,fp)) ) {
        if( strchr(line,'\n') )
            *strchr(line,'\n') = '\0';
        if( strlen(line) > 0 ) {
            if( single_command(line) < 0 ) {
                fclose(fp);
                free(line);
                return -1;
            }
        }
    }
    
    free(line);
    fclose(fp);
    return 0;
}


/**
 * Main command loop. We use the asynchronous version of readline to be able
 * to quickly detect a disconnected remote server.
 */
void
cmd_loop(void) {


    if (g7ctrl_initcomm(&cli_sockd)) {
        fprintf(stderr, "Cannot initialize communication with server.\n");
        return;
    }
    fprintf(stdout,"Press tab-key for auto-completion of commands.\n");
    
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

	    // Use timeout to check if remote server have closed
            if( is_disconnected(cli_sockd) ) {
                fprintf(stderr, "\nRemote server closed connection.\n");
                break;
            }

        } else if ( rc < 0 ) {

            fprintf(stderr, "Communication problem. Connection closed ( %s ).\n",strerror(errno));
            break;

        } else {

	  rl_callback_read_char();
	}

    } while( ! quitCmdLoop );

    rl_callback_handler_remove();
    (void) shutdown(cli_sockd, SHUT_RDWR);
    (void) close(cli_sockd);
    fprintf(stdout,"\n");
}

/**
 * Main entry point for g7sh
 * @param argc Argument count
 * @param argv Argument vector
 * @return EXIT_SUCCESS or EXIT_FAILURE depending on if the program exits in good or
 * bad standing.
 */
int
main(int argc, char **argv) {
    parsecmdline(argc, argv);
    setup_sighandlers();
    
    // Try to find a possible ini-file
    if (0 == setup_inifile()) {
        read_inifile();
    }
    
    // Setup default command port if user didn't explicitly specify port
    if (tcpip_port == -1) {
        tcpip_port = DEFAULT_CMD_PORT;
    }

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

    if( singleCmd ) {
        
        // Just do one single command
        if( 0 == single_command(singleCmdLine) )
            _exit(EXIT_SUCCESS);
        else
            _exit(EXIT_FAILURE);

    } else  {
       
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

}

/* EOF */
