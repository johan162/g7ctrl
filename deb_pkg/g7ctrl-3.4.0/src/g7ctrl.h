/* =========================================================================
 * File:        G7CTRL.H
 * Description: Header for main module
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

#ifndef G7CTRL_H
#define	G7CTRL_H

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * Information about each connected client (both devices and cmd clients)
 * We use the same structure to record information about the connected client
 * regardless of the type of client connected.
 */
struct client_info {
   /*
    * Fields used for both command and device clients
    */
   time_t    cli_ts;            // Connection timestamp
   int       cli_socket;        // Socket used for communication with the client
                                // Used for both command clients and devices connected
                                // over GPRS
   char      cli_ipadr[16];     // Client IP address ("xxx.xxx.xxx.xxx\0" = 16 chars)
   pthread_t cli_thread;        // Thread ID for managing thread
   _Bool     cli_is_cmdconn;       // TRUE if this is a command connection
   
   /*
    * Fields only used for command clients
    */  
   int       target_socket;     // Target device socket to use for GPRScommunication. 
                                // For the case of USB target this is = -1
   unsigned  target_deviceid;   // Target device ID
   ssize_t   target_cli_idx;    // Index in client_info_list for the specified target
                                // set to -1 when the target is USB
   ssize_t   target_usb_idx;    // For the case of USB target this gives the index  
                                // of the USB port which is the same as the index in th
                                // the USB info array
   _Bool     use_unicode_table; // Use unicode drawing characters for tables sent back to user
   
   /*
    * Fields only used for device connections
    */
   unsigned  cli_devid;         // Tracker device ID as integer. Always have 10 digits
};

/**
 * List with information on all connected clients
 */
extern struct client_info *client_info_list;

/**
 * Number of connected clients.
 */
extern int num_clients;


/**
 * Mutex to protect the data structures that keep track of all the threads
 */
extern pthread_mutex_t socks_mutex ;
extern pthread_mutex_t cmdtag_mutex ;
extern pthread_mutex_t cmdqueue_mutex ;
extern pthread_mutex_t logger_mutex ;

#ifdef	__cplusplus
}
#endif

#endif	/* G7CTRL_H */

