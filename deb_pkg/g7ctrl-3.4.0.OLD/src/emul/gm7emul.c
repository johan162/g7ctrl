/* =========================================================================
 * File:        GM7EMUL.C
 * Description: Emulator to simulate a GM7 device that can send typical
 *              events back to the daemon,
 *
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id$
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
#include <pthread.h>

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
#include "../libxstr/xstr.h"
#include "../config.h"
#include "../build.h"

// Since thes defines are supposed to be defined directly in the linker using
// --defsym to step the build number at each build and apples linker do not support
// this we just give them a 0 number here and never use them.
// This also means that on OSX the buildnumer displayed is meaningless
// TODO. Remove all references to buildnumer when running on OSX
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

#define LEN_10K (10*1024)

#define LOOP_UNIT 5
#define DEVID 3000000011U

#define LOC_SEND_INTERVAL 5
#define KEEP_ALIVE_INTERVAL 17

int tcpip_port = 3400;

/**
 * client socket. Needed for communication between command loop and
 * readline asynchronous callback.
 */
int daemon_sockd = -1;

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
dictionary *inifile_dict = NULL;


/**
 * Handling of arguments to the server
 */
static const char short_options [] = "hvs:p:";
static const struct option long_options [] = {
    { "help", no_argument, NULL, 'h'},
    { "version", no_argument, NULL, 'v'},
    { "server", required_argument, NULL, 's'},
    { "port", required_argument, NULL, 'p'},
    { 0, 0, 0, 0}
};

pthread_mutex_t daemon_write_mutex = PTHREAD_MUTEX_INITIALIZER;

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
        const size_t blen = LEN_10K;
        char *tmpbuff = calloc(blen,1);
        va_list ap;
        va_start(ap, buf);
        vsnprintf(tmpbuff, blen, buf, ap);
        tmpbuff[blen-1] = 0;
        int rc = write(fd, tmpbuff, strnlen(tmpbuff,blen));
        free(tmpbuff);
        return rc;
    }
    return -1;
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
 * Read a reply from a socket with a 5s timeout.
 *
 * @param sock Socket to read from
 * @param buffer Buffer for reply
 * @param maxbufflen Maximum reply length
 * @param nread number of chars read
 * @param timeout Timeout value (in seconds)
 * @return 0 on success, -3 timeout, -2 buffer to small for reply, -1 other error
 */
int
waitread(int sock, char *buffer, int maxbufflen, int *nread, int timeout) {
    fd_set read_fdset;
    struct timeval timeval;

    FD_ZERO(&read_fdset);
    FD_SET(sock, &read_fdset);

    // 5s timeout
    timerclear(&timeval);
    timeval.tv_sec = timeout;
    timeval.tv_usec = 0;
    int rc = select(sock + 1, &read_fdset, NULL, NULL, &timeval);
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
                    "'%s' - GM7 Basic Emulator.\n"
                    "Usage: %s [-h] [-v] [-s ipaddress] [-p port]\n"
                    "Options:\n"
                    " -h, --help      Print help and exit\n"
                    " -v, --version   Print version string and exit\n"
                    " -s, --server    Specify IP address (default=%s)\n"
                    " -p, --port      Specify TCP/IP port (default=%d)\n",
                    "gm7emul", "gm7emul", DEFAULT_SERVER, DEFAULT_CMD_PORT);
                exit(EXIT_SUCCESS);
                break;

            case 'v':
#ifdef __APPLE__
                fprintf(stdout, "%s %s\n%s",
                    "gm7emul", PACKAGE_VERSION,
                    "Copyright (C) 2013-2015  Johan Persson (johan162@gmail.com)\n"
                    "This is free software; see the source for copying conditions.\nThere is NO "
                    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
#else
                fprintf(stdout, "%s %s (build: %lu-%lu)\n%s",
                    "gm7emul", PACKAGE_VERSION, (unsigned long) &__BUILD_DATE, (unsigned long) &__BUILD_NUMBER,
                    "Copyright (C) 2013-2015  Johan Persson (johan162@gmail.com)\n"
                    "This is free software; see the source for copying conditions.\nThere is NO "
                    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
#endif
                exit(EXIT_SUCCESS);
                break;

            case 's':
                if (optarg != NULL) {
                    server_ip = strdup(optarg);
                }
                break;

            case 'p':
                if (optarg != NULL) {
                    tcpip_port = xatoi(optarg);
                }
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

    (void) shutdown(daemon_sockd, SHUT_RDWR);
    (void) close(daemon_sockd);
    fprintf(stdout,"\nBye.\n");
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
 * Initialize communication with the daemon
 * @param[out] sockd Return socket to use for communication with device
 * @return 0 success, -1 failure
 */
int
init_comm(int *sockd) {
    struct addrinfo hints;
    struct addrinfo *servinfo; // will point to the results
    char service[32];

    CLEAR(hints);
    hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    snprintf(service, sizeof (service), "%d", tcpip_port);

    int status = getaddrinfo(server_ip, service, &hints, &servinfo);
    if (status) {
        fprintf(stderr, "Cannot get address info from [%s:%d]\n", server_ip, tcpip_port);
        return -1;
    }

    *sockd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (-1 == *sockd) {
        fprintf(stderr, "Cannot create socket for endpoint [%s:%d]\n", server_ip, tcpip_port);
        return -2;
    }


    if (-1 == connect(*sockd, servinfo->ai_addr, servinfo->ai_addrlen)) {
        shutdown(*sockd, SHUT_RDWR);
        close(*sockd);
        fprintf(stderr, "Cannot connect to [%s:%d]\n", server_ip, tcpip_port);
        return -3;
    }

    freeaddrinfo(servinfo);

    // Read back the server startup message
    return 0;
}

/**
 * Check if the remote has has closed the socket (unexpectedly)
 * This could happen when the server times out incase the user
 * waits really, really long between interaction points.
 * @return 0 if the socket is alive, <> 0 if it is closed.
 */
int
is_disconnected(void) {
    // Detect if remote socket has disconnected. Due to the
    // way Unix socket works the only way to detect a remote socket
    // that is closed is to try to do a read on the socket
    // and if it has been closed it will return 0 bytes read.
    fd_set read_fdset;
    struct timeval timeout;

    FD_ZERO(&read_fdset);
    FD_SET(daemon_sockd, &read_fdset);

    timerclear(&timeout);
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 0.1 s timeout
    int rc = select(daemon_sockd + 1, &read_fdset, NULL, NULL, &timeout);
    if (0 == rc) {
        // Timeout - but the the socket is still alive
        return 0;
    }
    // We have some data so this could either be a disconnect notification or
    // some data to find out what we need to try to read the stream but
    // add the MSG_PEEK so not to remove any chars from the socket.
    char buff;
    const int nread = recv(daemon_sockd, &buff, sizeof (char), MSG_PEEK);
    return nread == 0;
}



/**
 * Needed to communicate between callback and main command loop
 */
volatile int quitCmdLoop = FALSE;
volatile _Bool sendDouble=FALSE;
volatile _Bool sendGFEN=FALSE;
int
send_locevent(const int sockd, const int evid) {
    char buff[255];
    if( sendDouble ) {
        snprintf(buff,sizeof(buff),
            "3000000001,20140107232526,17.961028,59.366470,0,0,0,0,%d,4.20V,0\r\n"
            "3000000001,20140107232526,17.961028,59.366470,0,0,0,0,%d,4.20V,1\r\n",
            evid,evid);
        sendDouble=FALSE;
        fprintf(stderr,"Sent back double event\n");
    } else if( sendGFEN ) {
            snprintf(buff,sizeof(buff),
            "3000000001,20140107232526,17.961028,59.366470,0,0,0,0,%d,4.20V,0\r\n",50);
            sendGFEN=FALSE;
            fprintf(stderr,"Sent back GFEN event\n");
    } else {
            snprintf(buff,sizeof(buff),
            "3000000001,20140107232526,17.961028,59.366470,0,0,0,0,%d,4.20V,0\r\n",
            evid);

    }
    pthread_mutex_lock(&daemon_write_mutex);

    int rc = _writef(sockd,"%s",buff);
    
    pthread_mutex_unlock(&daemon_write_mutex);
    if( rc == (int)strlen(buff) ) {
        return 0;
    } else {
        return -1;
    }
}

typedef struct {
    unsigned short ka_header;
    unsigned short ka_seq;
    unsigned long  ka_devid;
} keepalive_t;

int
send_keepalive(const int sockd, const unsigned short seq, const unsigned long devid) {

    unsigned char ka_buff[8];
    ka_buff[0] = 0xD0;
    ka_buff[1] = 0xD7;
    ka_buff[3] = seq / 256U;
    ka_buff[2] = (seq - 256U * ka_buff[3]);
    unsigned long tmp = devid;
    ka_buff[7] = tmp / 16777216U;
    tmp -= ka_buff[7]*16777216U;
    ka_buff[6] = tmp / 65536U;
    tmp -= ka_buff[6]*65536U;
    ka_buff[5] = tmp / 256U;
    tmp -= ka_buff[5]*256U;
    ka_buff[4] = tmp;
    pthread_mutex_lock(&daemon_write_mutex);
    ssize_t rc = write(sockd, ka_buff, 8);
    pthread_mutex_unlock(&daemon_write_mutex);    
    if (8 == rc) {
        fprintf(stderr,"KEEP_ALIVE sent: (%x,%x) (%x,%x) (%x,%x,%x,%x)\n",ka_buff[0],ka_buff[1],ka_buff[2],ka_buff[3],ka_buff[4],ka_buff[5],ka_buff[6],ka_buff[7]);
        // 3000000001 = 178, 208, 94, 1 = 0xb2, 0xd0, 0x5e, 0x01
        // (5,cd,72,27)
    } else {
        fprintf(stderr,"ERROR: write() error %d ( %d : %s)\n",(int)rc,errno,strerror(errno));
        return -1;
    }
    return 0;
}



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void *
keep_alive_sender(void *arg) {
    pthread_detach(pthread_self());

    unsigned short seq=0;

    while( 1 ) {
        sleep(KEEP_ALIVE_INTERVAL);        
        send_keepalive(daemon_sockd,seq++,DEVID);
    }

    pthread_exit(NULL);
    return (void *) 0;
}

void *
track_sender(void *arg) {

    pthread_detach(pthread_self());

    while( 1 ) {
        sleep(LOC_SEND_INTERVAL);
        if( -1 == send_locevent(daemon_sockd,1)) {
            fprintf(stderr, "Cannot write data to daemon.\n");
            break;
        } else {
            fprintf(stderr, "Sent location event.\n");
        }
    }

    pthread_exit(NULL);
    return (void *) 0;
}

#pragma GCC diagnostic pop

void
emul_loop(void) {

    char buffer[256];
    int nread;
    pthread_t ka_thread, track_thread;
    while( 1 ) {

        if (0 == init_comm(&daemon_sockd)) {
            fprintf(stderr, "Connected.\n");

            fprintf(stderr,"Starting KEEP_ALIVE thread\n");
            pthread_create(&ka_thread, NULL, keep_alive_sender, NULL);

            fprintf(stderr,"Starting TRACKER thread\n");
            pthread_create(&track_thread, NULL, track_sender, NULL);


            while( 1 ) {;

                if( is_disconnected() )
                    break;

                *buffer='\0';
                int rc = waitread(daemon_sockd, buffer, sizeof (buffer), &nread, LOOP_UNIT);
                if( 0 == rc ) {
                    if( nread > 0 ) {
                        // First check if this is a reply from a KEEP_ALIVE
                        if ( nread==8 && ((char) 0xD0 == *buffer && (char) 0xD7 == *(buffer + 1)) ) {
                                unsigned _dev =
                                (unsigned char) buffer[4]*1U +
                                (unsigned char) buffer[5]*256U +
                                (unsigned char) buffer[6]*65536U +
                                (unsigned char) buffer[7]*16777216U;
                                unsigned _seq = (unsigned char) buffer[2]*1 + (unsigned char) buffer[3]*256;
                                fprintf(stderr, "KEEP_ALIVE reply [deviceid=%u : seq=%04u]\n", _dev, _seq);
                        } else {
                            // We received a command. Send a FAKE reply after one second
                            xstrtrim_crnl(buffer);
                            fprintf(stderr,"Received cmd: %s\n",buffer);

                            // Extract CMDNAME & TAG from command
                            char *bptr=buffer;
                            while( *bptr != '+' ) bptr++;
                            bptr++;
                            char cmdname[13],*cptr=cmdname;
                            while( (*cptr++ = *bptr++) != '+' )
                                ;
                            *(cptr-1) = '\0';
                            char tagbuff[6],*tptr=tagbuff;
                            while( (*tptr++ = *bptr++) != '=' )
                                ;
                            *(tptr-1) = '\0';
                            usleep(100000);
                            char replybuff[1024];
                            // Check if this is one of the special "test" commands
                            if( 0 == strcmp(cmdname,"SIMID") ) {
                                sendDouble = TRUE;
                                _writef(daemon_sockd,"$OK:%s+%s=%u,99\r\n",cmdname,tagbuff,DEVID);
                                fprintf(stderr,"Prep for Sending back double\n");
                            } else if( 0 == strcmp(cmdname,"IMEI") ) {
                                sendGFEN = TRUE;
                                _writef(daemon_sockd,"$OK:%s+%s=%u,99\r\n",cmdname,tagbuff,DEVID);
                                fprintf(stderr,"Prep for Sending back GFEN\n");
                            } else if( 0 == strcmp("GETLOCATION",cmdname) ) {
                                snprintf(replybuff,sizeof(replybuff),"%u,20140206224257,17.960205,59.366856,0,0,0,4,0,4.12V,0",DEVID);
                                _writef(daemon_sockd,"%s\r\n",replybuff);
                                fprintf(stderr,"Sent back LOCATION\n");
                            } else {
                                _writef(daemon_sockd,"$OK:%s+%s=%u,99\r\n",cmdname,tagbuff,DEVID);
                                fprintf(stderr,"Sent back generic $OK\n");
                            }
                        }
                    } else {
                        fprintf(stderr,"Error: Empty command received!");
                    }
                }
                usleep(100000);

            }
            pthread_cancel(ka_thread);
            pthread_cancel(track_thread);
            fprintf(stderr,"Cancelled KEEP_ALIVE, TRACK threads\n");
            (void) shutdown(daemon_sockd, SHUT_RDWR);
            (void) close(daemon_sockd);
            daemon_sockd=-1;
            fprintf(stderr, "Disconnected.\n");
        }
        sleep(2);
    }
}

/**
 * Main entry
 * @param argc Argument count
 * @param argv Argument vector
 * @return EXIT_SUCCESS at program termination
 */
int
main(int argc, char **argv) {
    parsecmdline(argc, argv);
    setup_sighandlers();

    if (tcpip_port == -1) {
        tcpip_port = DEFAULT_CMD_PORT;
    }

    // The command loop is active until the server disconnects or the user
    // quits the command shell
    emul_loop();

    // ... and exit in good standing
    _exit(EXIT_SUCCESS);

}

/* EOF */
