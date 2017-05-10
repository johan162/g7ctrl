/* =========================================================================
 * File:        SOCKLISTENER.C
 * Description: The daemons real work is done here after the initial
 *              daemon house holding has been setup.
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
#include <pwd.h>
#include <signal.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/param.h> // To get MIN/MAX

#include "config.h"
#include "g7ctrl.h"
#include "build.h"
#include "utils.h"
#include "logger.h"
#include "g7config.h"
#include "lockfile.h"
#include "socklistener.h"
#include "g7cmd.h"
#include "tracker.h"
#include "sighandling.h"

/**
 * Create a new listening socket on the local host using the supplied port number.
 * Note: In case of any errors the program will be aborted with exit(EXIT_FAILURE)
 * so the returned socket is always a valid socket.
 * @param listeningPort Port to listen on
 * @return Listening socket descriptor on success
 */
int
setup_listening_socket(const int listeningPort) {
    struct sockaddr_in socketaddress;
    int listening_socket;
    // -------------------------------------------------------------------
    // Create the socket for cmd  connection (TCP)
    // -------------------------------------------------------------------
    if ((listening_socket = socket(AF_INET, SOCK_STREAM, 0)) < 1) {
        logmsg(LOG_ERR, "Unable to create socket. (%d : %s)", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // To allow the server to be restarted quickly we set the SO_REUSEADDR flag
    // Otherwise the server would give a "Port in use" error if the server were restarted
    // within approx. 30s after it was shutdown. This is due to the extra safety wait
    // that Unix does after a socket has been shut down just to be really, really make sure
    // that there is no more data coming.
    int so_flagval = 1;
    if (-1 == setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, (char *) &so_flagval, sizeof (int))) {
        logmsg(LOG_ERR, "Unable to set socket options. (%d : %s)", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    //memset(&socketaddress, 0, sizeof (socketaddress));
    CLEAR(socketaddress);
    socketaddress.sin_family = AF_INET;
    socketaddress.sin_addr.s_addr = htonl(INADDR_ANY);
    socketaddress.sin_port = (short unsigned) htons(listeningPort);

    if (bind(listening_socket, (struct sockaddr *) & socketaddress, sizeof (socketaddress)) != 0) {
        logmsg(LOG_ERR, "Unable to bind socket to port. Most likely some other application is using this port.");
        exit(EXIT_FAILURE);
    }

    // Listen on socket, queue up to 5 connections
    if (listen(listening_socket, 5) != 0) {
        logmsg(LOG_ERR, "Unable to listen on socket ");
        exit(EXIT_FAILURE);
    }

    // We don't want to risk that a child holds this descriptor
    if (-1 == set_cloexec_flag(listening_socket, 1)) {
        logmsg(LOG_ERR, "Failed to run set_cloexec_flag(). ( %d : %s)", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }    
    
    return listening_socket;
}

/**
 * Start the main socket server that listens for clients that connects
 * to us. For each new command client a command thread that will manage that client
 * is started with the cmd_clientsrv() thread. For each device that connects
 * to us a device thread will be started with the tracker_clientsrv() thread.
 *
 * @return 0
 */
int
startupsrv(void) {
    int cmd_sockd = -1, tracker_sockd = -1, newsocket = -1;
    unsigned i, tmpint;
    struct sockaddr_in remote_socketaddress;
    int ret;
    char *dotaddr = NULL;
    fd_set read_fdset;
    struct timeval timeout;

    
    cmd_sockd = setup_listening_socket(tcpip_cmd_port);
    tracker_sockd = setup_listening_socket(tcpip_device_port);
    
    logmsg(LOG_INFO, "Listening on port=%d for commands.", tcpip_cmd_port);
    logmsg(LOG_INFO, "Listening on port=%d for tracker connections.", tcpip_device_port);

    // Run until we receive a SIGQUIT or SIGINT awaiting client connections and
    // setting up new client communication channels
    while (1) {

        // =======================================================================
        // MAIN WAIT FOR NEW CONNECTIONS
        // =======================================================================

        // We must reset this each time since select() modifies it
        FD_ZERO(&read_fdset);
        FD_SET((unsigned) cmd_sockd, &read_fdset);
        FD_SET((unsigned) tracker_sockd, &read_fdset);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        ret = select(MAX(tracker_sockd, cmd_sockd) + 1, &read_fdset, NULL, NULL, &timeout);

        if (0 == ret) {
            // Timeout, so take the opportunity to check if we received a
            // signal telling us to quit. We don't care about what signal. Since the signal
            // handler is only allowing signals that we consider stopping signals)
            if (received_signal)
                break;
            else
                continue;
        }

        int command_connection = 0;

        // Check first if it was the command socket that received the data
        if (FD_ISSET((unsigned) cmd_sockd, &read_fdset)) {
            
            // --------------------------------------------------------------------------
            // Command connection
            // --------------------------------------------------------------------------
            logmsg(LOG_DEBUG, "Command connection.");
            command_connection = 1;
            tmpint = sizeof (remote_socketaddress);
            newsocket = accept(cmd_sockd, (struct sockaddr *) & remote_socketaddress, &tmpint);
            if (newsocket < 0) {
                // Unrecoverable error
                logmsg(LOG_CRIT, "Could not create new client socket ( %d : %s ) ", errno, strerror(errno));
                exit(EXIT_FAILURE);
            }
        
        } else if (FD_ISSET((unsigned) tracker_sockd, &read_fdset)) {
            
            // --------------------------------------------------------------------------
            // Tracker connection
            // --------------------------------------------------------------------------
            logmsg(LOG_DEBUG, "Tracker connection.");
            tmpint = sizeof (remote_socketaddress);
            newsocket = accept(tracker_sockd, (struct sockaddr *) & remote_socketaddress, &tmpint);
            if (newsocket < 0) {
                // Unrecoverable error
                logmsg(LOG_CRIT, "Could not create new tracker socket ( %d : %s ) ", errno, strerror(errno));
                exit(EXIT_FAILURE);
            }

        } else {
            // This should never happen. This case indicates a network/kernel problem on the server
            logmsg(LOG_CRIT, "Internal serious error. Accepted port connection that we were not listening on.");
            exit(EXIT_FAILURE);
        }
        
        dotaddr = inet_ntoa(remote_socketaddress.sin_addr);
        set_cloexec_flag(newsocket, 1);

        logmsg(LOG_INFO, "Client number %d have connected from IP: %s on socket %d", num_clients + 1, dotaddr, newsocket);

        // Lock mutes while we are manipulating the client_info_list
        pthread_mutex_lock(&socks_mutex);

        // Find the first empty slot for storing the client thread id
        // We use one global array structure that stores information about each
        // connection (both command and tracker)
        i = 0;
        while (i < max_clients && client_info_list[i].cli_thread)
            i++;

        if (i < max_clients) {

            // Remember the details about this connection
            client_info_list[i].cli_socket = newsocket;
            strncpy(client_info_list[i].cli_ipadr, dotaddr, 15);
            client_info_list[i].cli_ipadr[15] = '\0';
            client_info_list[i].cli_ts = time(NULL); // Timestamp for connection
            client_info_list[i].cli_devid = 0; // Device ID gets set by the first KEEP_ALIVE packets
            
            // Set default values for the target device. Until the user changes this with a 
            // .use command we will assume that we should talk over USB
            client_info_list[i].target_deviceid = 0;
            client_info_list[i].target_socket = -1;   // This indicates USB as the default target
            client_info_list[i].target_cli_idx = -1;
            client_info_list[i].target_usb_idx = 0;
            client_info_list[i].use_unicode_table = FALSE;

            if (command_connection) {
                client_info_list[i].cli_is_cmdconn = TRUE;
                ret = pthread_create(&client_info_list[i].cli_thread, NULL, cmd_clientsrv, (void *) & client_info_list[i]);
            } else {
                client_info_list[i].cli_is_cmdconn = FALSE;
                ret = pthread_create(&client_info_list[i].cli_thread, NULL, tracker_clientsrv, (void *) & client_info_list[i]);
            }

            if (ret != 0) {
                logmsg(LOG_CRIT, "Could not create thread for client ( %d :  %s )", errno, strerror(errno));
                exit(EXIT_FAILURE);
            } else {
                num_clients++;
            }

        } else {

            logmsg(LOG_ERR, "Client connection not allowed. Maximum number of clients (%zd) already connected.", max_clients);
            _writef(newsocket, "[ERR] Too many client connections.");
            _dbg_close(newsocket);

        }

        pthread_mutex_unlock(&socks_mutex);

    }

    logmsg(LOG_DEBUG, "Closing main listening socket.");
    if (-1 == _dbg_close(cmd_sockd)) {
        logmsg(LOG_ERR, "Failed to close main listening socket. ( %d : %s )", errno, strerror(errno));
    }

    return 0;
}

/* EOF */
