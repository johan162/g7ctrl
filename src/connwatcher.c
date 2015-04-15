/* =========================================================================
 * File:        CONNWATCHER.C
 * Description: Watch the virtual serial port for new devices connecting
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
#include <fcntl.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <math.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <sys/param.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef __APPLE__
#include <sys/inotify.h>
#endif

#include "config.h"

#include "g7ctrl.h"
#include "g7config.h"
#include "utils.h"
#include "logger.h"
#include "xstr.h"
#include "connwatcher.h"
#include "futils.h"
#include "serial.h"
#include "sighandling.h"
#include "g7sendcmd.h"
#include "unicode_tbl.h"

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + NAME_MAX + 1 ) )

/**
 * Global flag that is TRUE whenever a device is connected. FALSE otherwise
 * This is a volatile flag set by the watcher thread. This is not in general
 * protected by a mutex (though it probably should). Any race effects is guaranteed
 * to be temporary and retrying a failed command will alwyas have the desired
 * effect.
 */
//volatile int watcher_usb_connected;

/**
 * Keeps track of the USB connection status. It takes roughly 20s from that
 * the device is physically attached until we have established a connection.
 * During that time the status is "is_attached==TRUE" and "is_connected==FALSE"
 * When the device connection is fully established confirmed by reading the
 * Device ID then the state is "is_attached==TRUE" and "is_connected==TRUE"
 */
struct usb_conn_status_t {
    _Bool is_attached; // Is the device physically attached
    _Bool is_connected; // Is this device connected
    time_t ts; // Time when connection was made
    int stty_device_num;
    char stty_device_name[128];
    unsigned device_id;
};

#define MAX_USB_CONNECTIONS 6

#ifdef __APPLE__
size_t defined_osx_connections = 0;
#endif

size_t num_usb_connections = 0;

/**
 * One entry in the USB connection vector correspond to the ttyACMn device with the same
 * index. For example index 0 corresponds to ttyACM0, 1 to ttyACM1 and so on
 */
static struct usb_conn_status_t usb_conn_status[MAX_USB_CONNECTIONS];

/**
 * Set the target device that we are going to send the commands to. The
 * usb index refers to the index number for the connected USB device as as given
 * by the index position in usb_conn_status[] vector
 * @param[in] cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate back
 *                 to the client as well as information about which device the client
 *                 has set as target device.
 * @param[in] client_nbr The order number of the client as liste by the "lc"
 * command. If set to "-1" then the communication is reset to USB.
 * @return 0 on success, -1 on failure
 * @see set_gprs_device_target_by_nickname()
 */
int
set_usb_device_target_by_index(struct client_info *cli_info, const size_t usb_idx) {

    const int sockd = cli_info->cli_socket;

    //  Check that USB index is within range    
    if (usb_idx >= MAX_USB_CONNECTIONS) {
        _writef(sockd, "Unknown USB index specified (%zd)", usb_idx);
        logmsg(LOG_ERR, "Unknown USB index specified (%zd)", usb_idx);
        return -1;
    }

    // Verify that there is a device connected to this USB index
    if (0 == num_usb_connections || !usb_conn_status[usb_idx].is_connected) {
        _writef(sockd, "No device connected to USB index (%zd)", usb_idx);
        logmsg(LOG_ERR, "No device connected to USB index (%zd)", usb_idx);
        return -1;
    }

    cli_info->target_socket = -1;
    cli_info->target_deviceid = 0;
    cli_info->target_cli_idx = -1;
    cli_info->target_usb_idx = usb_idx;

    _writef(sockd, "USB target set to index=%zd", usb_idx);
    logmsg(LOG_DEBUG, "USB target set to index=%zd", usb_idx);

    return 0;

}

int
get_usb_devicename(const size_t idx, char **device) {
    if (idx < MAX_USB_CONNECTIONS) {
        *device = usb_conn_status[idx].stty_device_name;
        return 0;
    }
    *device = NULL;
    return -1;
}

/**
 * 
 * @param stty_device_num
 * @param connected
 */
void
update_usb_stat(size_t idx, _Bool connected) {
    if (connected) {
        if (usb_conn_status[idx].is_connected) {
            // Nothing to do
        } else {
            usb_conn_status[idx].is_connected = TRUE;
            usb_conn_status[idx].ts = time(NULL);
            num_usb_connections++;
        }
    } else {
        if (usb_conn_status[idx].is_connected) {
            usb_conn_status[idx].is_attached = FALSE;
            usb_conn_status[idx].is_connected = FALSE;
            usb_conn_status[idx].device_id = 0;
            usb_conn_status[idx].ts = 0;
            num_usb_connections--;
        } else if (usb_conn_status[idx].is_attached) {
            // Were are disconnecting before the device ID and connection is
            // fully established
            usb_conn_status[idx].is_attached = FALSE;
        }
    }
}

/**
 * List status for each defined USB device
 * @param cli_info Client context
 */
void
list_usb_stat(struct client_info *cli_info) {
    const int sockd = cli_info->cli_socket;

#ifdef __APPLE__    
    const size_t maxrows = defined_osx_connections;
#else    
    const size_t maxrows = MAX_USB_CONNECTIONS;
#endif        

    char device_name[128];
    int y, m, d, h, min, s;


    // Construct the array of data fields
    // '#', 'USB', 'Status', 'Dev ID'
    const size_t nCols = 4;
    char *tdata[(maxrows + 1) * nCols];
    memset(tdata, 0, sizeof (tdata));
    tdata[0 * nCols + 0] = strdup(" # ");
    tdata[0 * nCols + 1] = strdup("  USB  ");
    tdata[0 * nCols + 2] = strdup("  Conn. Time  ");
    tdata[0 * nCols + 3] = strdup("  Dev ID  ");

    char buff1[32], buff2[32], buff3[32], buff4[32];
    size_t rows = 1;
    for (size_t i = 0; i < maxrows; i++) {
        fromtimestamp(usb_conn_status[i].ts, &y, &m, &d, &h, &min, &s);
        strncpy(device_name, usb_conn_status[i].stty_device_name, sizeof (device_name) - 1);
        snprintf(buff1, sizeof (buff1), " %02zu ", i);
        if (usb_conn_status[i].is_connected) {
            if (0 == usb_conn_status[i].device_id) {
                // The device is physically connected but has not yet established USB connection
                snprintf(buff2, sizeof (buff2), " %c%s ",
                        cli_info->target_usb_idx == (int) i ? '*' : ' ', basename(device_name));
                snprintf(buff3, sizeof (buff3), " connecting ... ");
                *buff4 = '\0';
            } else {
                snprintf(buff2, sizeof (buff2), " %c%s ",
                        cli_info->target_usb_idx == (int) i ? '*' : ' ', basename(device_name));
                snprintf(buff3, sizeof (buff3), " %04d-%02d-%02d %02d:%02d ", y, m, d, h, min);
                snprintf(buff4, sizeof (buff4), " %u ", usb_conn_status[i].device_id);
            }
            tdata[rows * nCols + 0] = strdup(buff1);
            tdata[rows * nCols + 1] = strdup(buff2);
            tdata[rows * nCols + 2] = strdup(buff3);
            tdata[rows * nCols + 3] = strdup(buff4);
            ++rows;
        } else {
            if (usb_conn_status[i].is_attached) {
                snprintf(buff2, sizeof (buff2), " %s ", basename(device_name));
                tdata[rows * nCols + 0] = strdup(buff1);
                tdata[rows * nCols + 1] = strdup(buff2);
                snprintf(buff3, sizeof (buff3), " connecting ... ");
                tdata[rows * nCols + 2] = strdup(buff3);
                *buff4 = '\0';
                tdata[rows * nCols + 3] = strdup(buff4);
                ++rows;
            }
        }
    }

    if (rows == 1) {
        // Insert an empty row
        tdata[1 * nCols + 0] = strdup(" ");
        tdata[1 * nCols + 1] = strdup(" ");
        tdata[1 * nCols + 2] = strdup(" ");
        tdata[1 * nCols + 3] = strdup(" ");
        rows++;
    }

    table_t *t = utable_create_set(rows, nCols, tdata);
    //utable_set_title(t, "USB devices", TITLESTYLE_LINE);
    utable_set_row_halign(t, 0, CENTERALIGN);
    if (cli_info->use_unicode_table) {
        utable_set_interior(t,TRUE,FALSE);        
        utable_stroke(t, sockd, TSTYLE_DOUBLE_V2);
    } else {
        utable_stroke(t, sockd, TSTYLE_ASCII_V3);
    }
       
    utable_free(t);
    for (size_t i = 0; i < rows * nCols; i++) {
        free(tdata[i]);
    }
}

/**
 * Check if any USB with specified index connection is active
 * @param[in] idx Index into USB status array
 * @return TRUE if connection exist, FALSE otherwise
 */
_Bool
is_usb_connected(const size_t idx) {
    if (idx >= MAX_USB_CONNECTIONS)
        return FALSE;

    return num_usb_connections > 0 &&
            usb_conn_status[idx].is_connected && usb_conn_status[idx].device_id;
}

/**
 * 
 * @return 0 on success, aborts program on error
 */
void
init_usb_status(void) {
#ifndef __APPLE__    
    for (size_t i = 0; i < MAX_USB_CONNECTIONS; i++) {
        usb_conn_status[i].is_attached = FALSE;
        usb_conn_status[i].is_connected = FALSE;
        usb_conn_status[i].ts = 0;
        usb_conn_status[i].device_id = 0;
        usb_conn_status[i].stty_device_num = i + stty_device_startidx;
        const char *name = get_stty_device_name(i + stty_device_startidx);
        if (NULL == name) {
            logmsg(LOG_ERR, "Fatal error in init_usb_status() aborting program.");
            exit(EXIT_FAILURE); // There is no way to recover from this
        }
        strncpy(usb_conn_status[i].stty_device_name, name, sizeof (usb_conn_status[i].stty_device_name) - 1);
    }
#else
    size_t i = 0;
    while (cu_usbmodem_list[i] > -1) {
        usb_conn_status[i].is_attached = FALSE;
        usb_conn_status[i].is_connected = FALSE;
        usb_conn_status[i].ts = 0;
        usb_conn_status[i].device_id = 0;
        usb_conn_status[i].stty_device_num = i;
        const char *name = get_stty_device_name(cu_usbmodem_list[i]);
        if (NULL == name) {
            logmsg(LOG_ERR, "Fatal error in init_usb_status() aborting program.");
            exit(EXIT_FAILURE); // There is no way to recover from this
        }
        strncpy(usb_conn_status[i].stty_device_name, name, sizeof (usb_conn_status[i].stty_device_name) - 1);
        defined_osx_connections++;
        i++;
    }

#endif    
}

// Silent gcc about unused "arg"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/**
 * An instance of this thread is started for each connected USB device. Its
 * task is to handle the fact that it take somewhere between 15-20s for the
 * device to have a full USB connection established after physical connection.
 * It will check after 20s for a good connection by trying to retrieve the
 * device ID. If this succeeds the device is considered properly connected
 * and can be used in further command actions.
 * 
 * @param arg The USB index
 * @return 
 */
void *
establish_usb_thread(void *arg) {
    const size_t idx = *(size_t*) arg;
    pthread_detach(pthread_self());
    struct client_info cli_info;

    // Since this is sent as a server internal command we don't really have a client
    // context so we fake one with a -1 as client socket which means that nothing will
    // be written to that socket.
    // As the USB target we use the new USB connection indicated by the 'arg' provided
    // into this thread.
    CLEAR(cli_info);
    cli_info.cli_socket = -1; // No socket to write back to
    cli_info.target_socket = -1; // No target socket
    cli_info.target_usb_idx = idx;

    usb_conn_status[idx].is_attached = TRUE;

    // Wait 20s initially for USB connection to be established
    logmsg(LOG_DEBUG, "Waiting 20s for USB device connection to be established");
    sleep(20);

    unsigned device_id;
    if (-1 == get_devid(&cli_info, &device_id)) {
        // Wait another 4 seconds
        logmsg(LOG_DEBUG, "Waiting additional 5s for USB device connection to be established");
        sleep(5);

        if (-1 == get_devid(&cli_info, &device_id)) {
            // Wait another 4 seconds
            logmsg(LOG_DEBUG, "Waiting additional 5s for USB device connection to be established");
            sleep(5);


            if (-1 == get_devid(&cli_info, &device_id)) {
                // Give up
                logmsg(LOG_ERR, "Cannot establish USB connection. Please disconnect/reconnect.");
                pthread_exit(NULL);
                return (void *) 0;
            }

        }

    }
    usb_conn_status[idx].device_id = device_id;
    update_usb_stat(idx, TRUE);
    logmsg(LOG_INFO, "USB established with device. DEVID=%u", device_id);
    pthread_exit(NULL);
    return (void *) 0;
}


#ifndef __APPLE__

/**
 * Main thread that watches the "dev/" directory for appearance of USB device
 * file. This event in turns sets or clear a global volatile flag that communicates
 * the state to clients.
 * This thread has one big preprocessor ifdef to also work on OSX. On OSX
 * the inotify() structure does not exist. So in this case we pull and check
 * the file with 5s intervals. Not ideal and should eventually be rewritten
 * to use Apples File Event API-
 * @param arg Dummy parameter required for new thread. Not used.
 * @return Dummy return. This thread will only return on program shutdown.
 */
void *
connwatcher_thread(void *arg) {

    // To avoid reserving ~8MB after the thread terminates we
    // detach it. Without doing this the pthreads library would keep
    // the exit status until the thread is joined (or detached) which would mean
    // loosing 8MB for each created thread
    pthread_detach(pthread_self());

    // Init USB connection status 
    init_usb_status();

    // In case the device already exists it wont generate any event until it is removed
    // and we need to know that it is already connected. We do this by trying to open the
    // device.
    struct stat stat_buffer;
    int ret;
    pthread_t usb_conn_thread_id[MAX_USB_CONNECTIONS];
    size_t usb_idx[MAX_USB_CONNECTIONS];

    for (size_t j = 0; j < MAX_USB_CONNECTIONS; j++) {
        logmsg(LOG_DEBUG, "Checking if device %s is connected from start", usb_conn_status[j].stty_device_name);
        CLEAR(usb_conn_thread_id[j]);
        ret = stat(usb_conn_status[j].stty_device_name, &stat_buffer);
        if (0 == ret) {
            logmsg(LOG_INFO, "USB on \"%s\" connected.", usb_conn_status[j].stty_device_name);
            // The "usb_idx[j] = j" below might look silly but in order to avoid any kind of
            // nasty race condition when the thread is created we cannot us the address to 
            // a local auto variable as argument to the thread. It is possible that the auto
            // variable changes between the call to create the thread and when it is in
            // fact used when once the thread has started.
            usb_idx[j] = j;
            pthread_create(&usb_conn_thread_id[j], NULL, establish_usb_thread, &usb_idx[j]);
        }
    }

    int inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        logmsg(LOG_CRIT, "Cannot initialize inotify. Aborting. ( %d : %s )", errno, strerror(errno));
        pthread_exit(NULL);
        return (void *) 0;
    }

    fd_set ifd_set;
    struct timeval timeout;
    char *event_buffer = _chk_calloc_exit(BUF_LEN);
    char stty_device_file_copy[128];

    strcpy(stty_device_file_copy, usb_conn_status[0].stty_device_name);
    char *stty_device_dir = dirname(stty_device_file_copy);
    logmsg(LOG_DEBUG, "Watching directory dir=\"%s\" for changes", stty_device_dir);

    int wd = inotify_add_watch(inotify_fd, stty_device_dir, IN_CREATE | IN_DELETE);
    if (wd < 0) {
        logmsg(LOG_CRIT, "Cannot add watcher ( %d : %s )", errno, strerror(errno));
        pthread_exit(NULL);
        return (void *) 0;
    }

    char tmp_device_name[128];
    while (1) {
        FD_ZERO(&ifd_set);
        FD_SET(inotify_fd, &ifd_set);
        timeout.tv_sec = 4;
        timeout.tv_usec = 0;
        ret = select(inotify_fd + 1, &ifd_set, NULL, NULL, &timeout);

        if (0 == ret) {
            // logmsg(LOG_DEBUG,"USB watcher timeout.");
            // Timeout, so take the opportunity to check if we received a
            // signal telling us to quit. We don't care about what signal. Since the signal
            // handler is only allowing signals that we consider stopping signals)
            if (received_signal)
                break;
            else
                continue;
        }

        // Now read the event buffer to see what has happened
        const ssize_t length = read(inotify_fd, event_buffer, BUF_LEN);
        ssize_t i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &event_buffer[ i ];
            // Do we have a name in the event and if so check if the name matches one of our
            // device files
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    for (size_t j = 0; j < MAX_USB_CONNECTIONS; j++) {
                        strncpy(tmp_device_name, usb_conn_status[j].stty_device_name, sizeof (tmp_device_name) - 1);
                        if (0 == strcmp(basename(tmp_device_name), event->name)) {
                            logmsg(LOG_INFO, "USB on \"%s\" connected", event->name);
                            // The "usb_idx[j] = j" below might look silly but in order to avoid any kind of
                            // nasty race condition when the thread i created we cannot us the address to 
                            // a local auto variable as argument to the thread. It is possible that the auto
                            // variable changes between the call to create the thread and when it is in
                            // fact used when once the thread has started.
                            usb_idx[j] = j;
                            pthread_create(&usb_conn_thread_id[j], NULL, establish_usb_thread, &usb_idx[j]);
                        }
                    }
                } else if (event->mask & IN_DELETE) {
                    for (size_t j = 0; j < MAX_USB_CONNECTIONS; j++) {
                        strncpy(tmp_device_name, usb_conn_status[j].stty_device_name, sizeof (tmp_device_name) - 1);
                        if (0 == strcmp(basename(tmp_device_name), event->name)) {
                            // Check if we are still trying to establish a connection with this device indicating 
                            // that it was unplugged just after it had been connected. In that case we need to cancel
                            // the thread thta is trying to get the device ID
                            if (usb_conn_status[j].is_attached && !usb_conn_status[j].is_connected) {
                                // Cancel the thread
                                if (0 == pthread_cancel(usb_conn_thread_id[j])) {
                                    logmsg(LOG_DEBUG, "USB connection thread canceled.");
                                } else {
                                    logmsg(LOG_DEBUG, "USB connection thread could not be canceled");
                                }
                                CLEAR(usb_conn_thread_id[j]);
                            }
                            logmsg(LOG_INFO, "USB on (%s) disconnected.", event->name);
                            update_usb_stat(j, FALSE);
                        }
                    }
                }
            }

            i += EVENT_SIZE + event->len;
        }
    }

    (void) inotify_rm_watch(inotify_fd, wd);
    (void) close(inotify_fd);
    free(event_buffer);
    pthread_exit(NULL);
    return (void *) 0;
}

#else
// On OSX we fake inotify() by checking the file at regular interval
// TODO: Rewrite using OSX File system event API

void *
connwatcher_thread(void *arg) {

    // To avoid reserving ~8MB after the thread terminates we
    // detach it. Without doing this the pthreads library would keep
    // the exit status until the thread is joined (or detached) which would mean
    // loosing 8MB for each created thread
    pthread_detach(pthread_self());

    // Init USB connection status 
    init_usb_status();

    struct stat stat_buffer;
    pthread_t usb_conn_thread_id[MAX_USB_CONNECTIONS];
    size_t usb_idx[MAX_USB_CONNECTIONS];

    for (size_t j = 0; j < MAX_USB_CONNECTIONS; j++)
        CLEAR(usb_conn_thread_id[j]);

    while (TRUE) {

        if (received_signal)
            break;

        for (size_t j = 0; j < defined_osx_connections; j++) {
            //logmsg(LOG_DEBUG,"Checking if USB device on %s is connected",usb_conn_status[j].stty_device_name);
            if (0 == stat(usb_conn_status[j].stty_device_name, &stat_buffer)) {
                if (usb_conn_status[j].is_attached == FALSE) {
                    // Not previously connected
                    logmsg(LOG_INFO, "USB on \"%s\" connected.", usb_conn_status[j].stty_device_name);
                    // The "usb_idx[j] = j" below might look silly but in order to avoid any kind of
                    // nasty race condition when the thread i created we cannot us the address to 
                    // a local auto variable as argument to the thread. It is possible that the auto
                    // variable changes between the call to create the thread and when it is in
                    // fact used when once the thread has started.
                    usb_idx[j] = j;
                    pthread_create(&usb_conn_thread_id[j], NULL, establish_usb_thread, &usb_idx[j]);
                }
            } else {
                if (usb_conn_status[j].is_attached && !usb_conn_status[j].is_connected) {
                    // Cancel the thread
                    if (0 == pthread_cancel(usb_conn_thread_id[j])) {
                        logmsg(LOG_DEBUG, "USB connection thread canceled.");
                    } else {
                        logmsg(LOG_DEBUG, "USB connection thread could not be canceled");
                    }
                    CLEAR(usb_conn_thread_id[j]);
                }
                if (usb_conn_status[j].is_connected || usb_conn_status[j].is_attached) {
                    // It used to be connected
                    logmsg(LOG_INFO, "USB on \"%s\" disconnected.", usb_conn_status[j].stty_device_name);
                }
                update_usb_stat(j, FALSE);
            }
        }

        // Check every 30 seconds
        sleep(30);

    }

    pthread_exit(NULL);
    return (void *) 0;

}
#endif

#pragma GCC diagnostic pop
