/* =========================================================================
 * File:        SERIAL.C
 * Description:  Routines for read and write to serial port with timeout
 *              and various checks for error conditions.
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
#include <termios.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#ifndef __APPLE__
#include <linux/usbdevice_fs.h>
#endif

#include "config.h"
#include "g7ctrl.h"
#include "utils.h"
#include "logger.h"
#include "g7config.h"
#include "lockfile.h"
#include "serial.h"
#include "libxstr/xstr.h"


// Clear variable section in memory
#define CLEAR(x) memset (&(x), 0, sizeof(x))

/**
 * Default read timeout to 3000ms (=3s)
 */
static int read_timeout = 3000;

/**
 * Get the device file name from index
 * @param idx The number for the device file
 * @return A pointer to a static string with the name of the device to use
 */
const char *
get_stty_device_name(const size_t idx) {
#ifdef DEBUG_SIMULATE
    static char dummy_device[256];
    snprintf(dummy_device, sizeof (dummy_device), "/tmp/sim_ttyACM%d", (int) idx);
    return dummy_device;
#else

#ifdef __APPLE__

    static char *serial_udb_modem_basename = "/dev/cu.usbmodem";
    static char constructed_name[32];
    if (idx > 1000 && idx < 3000) {
        snprintf(constructed_name, sizeof (constructed_name), "%s%04zu", serial_udb_modem_basename, idx);
        logmsg(LOG_DEBUG, "Constructed  serial device name \"%s\"", constructed_name);
        return constructed_name;
    } else {
        logmsg(LOG_ERR, "Unknown device \"/dev/cu.usmodem%04zu\"", idx);
        return NULL;
    }

#else
    static char *serial_device_map[] ={"/dev/ttyACM0", "/dev/ttyACM1",
        "/dev/ttyACM2", "/dev/ttyACM3",
        "/dev/ttyACM4", "/dev/ttyACM5"};

    const size_t num_serial_device = 6;
    if (idx < num_serial_device) {
        return serial_device_map[idx];
    } else {
        logmsg(LOG_ERR, "FATAL: Unknown stty device index=%zd", idx);
        return NULL;
    }
#endif

#endif
}

/**
 * Translate integer baudrate to internal symbolic representation
 * @param baud baudrate. Must be one a standard baudrate
 * such as 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200
 * @return Internal constant for baudrate, -1 unknown baudrate
 */
static int
translate_baudrate(const int baud) {
    int termio_baud;
    switch (baud) {
        case 50: termio_baud = B50;
            break;
        case 75: termio_baud = B75;
            break;
        case 110: termio_baud = B110;
            break;
        case 134: termio_baud = B134;
            break;
        case 150: termio_baud = B150;
            break;
        case 200: termio_baud = B200;
            break;
        case 300: termio_baud = B300;
            break;
        case 600: termio_baud = B600;
            break;
        case 1200: termio_baud = B1200;
            break;
        case 1800: termio_baud = B1800;
            break;
        case 2400: termio_baud = B2400;
            break;
        case 4800: termio_baud = B4800;
            break;
        case 9600: termio_baud = B9600;
            break;
        case 19200: termio_baud = B19200;
            break;
        case 38400: termio_baud = B38400;
            break;
        case 57600: termio_baud = B57600;
            break;
        case 115200: termio_baud = B115200;
            break;
        case 230400: termio_baud = B230400;
            break;
        default: return -1;
            break;
    }
    return termio_baud;
}

/**
 * Open the serial device specified by index with the selected baudrate
 * @param idx Device index. 
 * @param baudrate Which baudrate to use. Must be one a standard baudrate
 * such as 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200
 * @return  -1 on failure, >= 0 the opened descriptor
 */
int
serial_open(const char *device, const int baudrate) {

#ifdef DEBUG_SIMULATE
    static int sim_fd = 500;
    logmsg(LOG_DEBUG, "Simulating opening device \"%s\" at %d (%d)", get_stty_device_name(idx), baudrate, translate_baudrate(baudrate));
    return sim_fd++;
#else
    int termio_baud;
    termio_baud = translate_baudrate(baudrate);
    //const char *device = get_stty_device_name(idx);

    if (!device || -1 == termio_baud) {
        return -1;
    }

    // Make sure the TTY is not inadvertently becoming the controlling terminal
    // and that we can read/write to it
    int oflags = O_NOCTTY | O_RDWR | O_NDELAY;
    int sfd = open(device, oflags);
    if (sfd >= 0) {
        logmsg(LOG_DEBUG, "Opened serial device \"%s\" fd=%d ", device, sfd);
    } else {
        logmsg(LOG_ERR, "Error opening serial device \"%s\" ( %d : %s )", device, errno, strerror(errno));
        struct passwd *pwe = getpwuid(getuid());
        logmsg(LOG_DEBUG, "   HINT: Make sure user \"%s\" belongs to the group necessary to access a serial device (usually \"dialout\")", pwe->pw_name);
        return -1;
    }
    
    // Set blocking mode to enable c_cc intercharacter timing / and counting
    if( -1 == fcntl(sfd, F_SETFL, 0 ) ) {
        logmsg(LOG_ERR,"Error setting F_SETFL on \"%s\" ( %d : %s )",device,errno,strerror(errno));
        (void)serial_close(sfd);
        return -1;
    }

    // Set exclusive right to this port
    if( -1 == ioctl(sfd, TIOCEXCL ) ) {
        logmsg(LOG_ERR,"Error setting TIOCEXCL on \"%s\" ( %d : %s )",device,errno,strerror(errno));
        (void)serial_close(sfd);
        return -1;
    }

    // Read existing setings
    // TODO: We should save the read settings and restore thme when we close the port
    struct termios line_settings;
    if (tcgetattr(sfd, &line_settings) < 0) {
        logmsg(LOG_ERR, "Cannot read line settings on device \"%s\" ( %d : %s )", device, errno, strerror(errno));
        (void) serial_close(sfd);
        return -1;
    }

    // Turn off character processing
    // clear current char size mask, no parity checking,
    // no output processing, force 8 bit input
    // Calling cfmakeraw is equivalent of:
    // line_settings.c_cflag &= ~(CSIZE | PARENB);
    // line_settings.c_cflag |= CS8 | CLOCAL | CREAD;    
    // Set RAW (character by character reading)
    cfmakeraw(&line_settings);
    
    // Ok to return read after only 1 character
    line_settings.c_cc[VMIN] = 1;
    line_settings.c_cc[VTIME] = 0;


    if (cfsetispeed(&line_settings, termio_baud) < 0 || cfsetospeed(&line_settings, termio_baud) < 0) {
        logmsg(LOG_ERR, "Cannot set baudrate ( %d : %s )", errno, strerror(errno));
        (void) serial_close(sfd);
        return -1;
    }

    if (-1 == tcsetattr(sfd, TCSAFLUSH, &line_settings)) {
        logmsg(LOG_ERR, "Cannot apply line settings on device \"%s\" ( %d : %s )", device, errno, strerror(errno));
        (void) serial_close(sfd);
        return -1;
    }

    return sfd;
#endif

}

#ifdef __APPLE__
// This define is missing in OSX headers
#define USBDEVFS_RESET             _IO('U', 20)
#endif

/**
 * Do a complete reset of the connected USB device. This is the same
 * as connecting/disconnecting the USB cable
 * @param sfd
 * @return -1 on failure, 0 on success
 */
int
usb_reset(const int sfd) {
#ifdef DEBUG_SIMULATE
    logmsg(LOG_DEBUG, "Resetting USB serial device fd=%d", sfd);    
#else    
    int rc = ioctl(sfd, USBDEVFS_RESET, 0);
    if (rc < 0) {
        logmsg(LOG_ERR, "Cannot reset USB on fd=%d ( %d : %s )", sfd, errno, strerror(errno));
        return -1;
    }
    return 0;
#endif
}


/**
 * Close the designated serial port
 * @param sfd
 * @return 0 on success, -1 on failure
 */
int
serial_close(const int sfd) {
#ifdef DEBUG_SIMULATE
    logmsg(LOG_DEBUG, "Closing serial device fd=%d", sfd);    
#else
    if( -1 == tcflush(sfd,TCIOFLUSH) ) {
        logmsg(LOG_ERR, "Failed to flush serial port fd=%d, ( %d : %s )", sfd, errno, strerror(errno));
    }
    if (-1 == close(sfd)) {
        logmsg(LOG_ERR, "Failed to close serial port fd=%d, ( %d : %s )", sfd, errno, strerror(errno));
        return -1;
    }
    logmsg(LOG_DEBUG, "Flushed and closed serial device fd=%d", sfd);
#endif
    return 0;
}

/**
 * Write the designated number of characters to the specified serial port. The data is
 * assumed to be binary and no end of string interpretation is made.
 * @param sfd handle to serial port
 * @param len Length of data
 * @param data pointer to data
 * @return 0 on success, -1 on failure
 */
int
serial_write(const int sfd, const char *data, const size_t len) {
#ifdef DEBUG_SIMULATE
    logmsg(LOG_DEBUG, "Simulating serial_write() on fd=%d, \"%s\" (len=%d)", sfd, data, (int) len);
#else
    // First clear the buffer from any potential previous crap
    tcflush(sfd, TCIOFLUSH);
    ssize_t ret = write(sfd, data, len);
    if (-1 == ret || ret != (ssize_t) len) {
        logmsg(LOG_ERR, "Failed to write to serial port fd=%d, ( %d : %s )", sfd, errno, strerror(errno));
        return -1;
    }
    // Make sure we wait until all chars have been cleared from the virtual UART buffer
    tcdrain(sfd);
#endif
    return 0;
}

#define SERIALRD_TIMEOUT -2

/**
 * Read a maximum of maxlen characters from the designated serial port and store
 * the read character in the buffer pointer to by buffer
 * @param sfd handle to serial port
 * @param maxlen length of data
 * @param buffer pointer to buffer to store read data
 * @param timeout_msec Timeout in ms
 * @return number of characters read on success, -1 on failure, SERIALRD_TIMEOUT on timeout
 */
int
serial_read_timeout(const int sfd, const size_t maxlen, char *buffer, const int timeout_msec) {
#ifdef DEBUG_SIMULATE
    logmsg(LOG_DEBUG, "Simulating serial_read_timeout() on fd=%d with timeout=%d", sfd, timeout_msec);
    snprintf(buffer, maxlen, "$ERR:SIMULATED=0\r\n");
    return strlen(buffer);
#else
    const int num_fd = 1;
    struct pollfd fds;
    CLEAR(fds);
    fds.fd = sfd;
    fds.events = POLLIN | POLLPRI;
    int ret = poll(&fds, num_fd, timeout_msec);

    if (0 == ret) {
        logmsg(LOG_ERR, "Serial read timeout");
        return SERIALRD_TIMEOUT; // Timeout
    }
    if (ret < 0) {
        logmsg(LOG_ERR, "Serial poll() error. ( %d : %s )", errno, strerror(errno));
        return -1; // Other error
    }

    ssize_t n = 0;
    if (fds.revents & POLLIN || fds.revents & POLLPRI) {
        if ((n = read(sfd, buffer, maxlen)) == -1) {
            logmsg(LOG_ERR, "Cannot read serial port after poll() returned. ( %d : %s )", errno, strerror(errno));
            return -1;
        }
    }
    return n;
#endif
}

/**
 * Read data from the serial port and store the data in the specified
 * buffer. This is a short form of serial_read_timeout() since it would
 * never make any point of trying a read without guarded by a timeout.
 * @param sfd handle to serial port
 * @param maxlen length of data
 * @param buffer pointer to buffer to store read data
 * @return number of characters read on success, -1 on failure, -2 on timeout
 */
int
serial_read(const int sfd, const size_t maxlen, char *buffer) {
    return serial_read_timeout(sfd, maxlen, buffer, read_timeout);
}


/*
 * Same as serial_read_timeout() but read chunkSize bytes at a time and
 * then call the callback function void cb(void *,int)  and continue to read until
 * no more data (or timeout). This can be used to intercept long reads
 * with progress information. The first argument to the callback is the last
 * argument of this function and the second argument in the callback is the total
 * number of bytes read sofar.
 * @param sfd handle to serial port
 * @param maxlen length of data
 * @param buffer pointer to buffer to store read data
 * @param timeout_msec Timeout in ms
 * @param chunkSize The size of each chunk to read
 * @param cb Callback function void cb(coid *, int)
 * @param arg The first argument to the callback.
 * @return number of characters read on success ( >= 0), -1 on failure, -2 on timeout
 */
/*
int
serial_chunkread(const int sfd, const size_t maxlen, char *buffer, const int timeout_msec,
                 const size_t chunkSize, void (*cb)(void *, int), void *arg) {
#ifdef DEBUG_SIMULATE
        logmsg(LOG_DEBUG,"Simulating serial_read() on fd=%d with timeout=%d",sfd,timeout_msec);
        snprintf(buffer,maxlen,"$ERR:SIMULATED=0\r\n");
    return strlen(buffer);
#else

    char *chunk = calloc(chunkSize,sizeof(char));
    if( NULL == chunk ) {
        return -1;
    }
    char *bptr = buffer, *cptr;;
    logmsg(LOG_DEBUG,"Chunk read");
    const int num_fd = 1;
    ssize_t tot_read=0;
    struct pollfd fds;
    ssize_t nread = 0;
    int chunkNbr=0;
    do {
        fds.fd = sfd;
        fds.events = POLLIN | POLLPRI;
        int ret = poll(&fds, num_fd, timeout_msec);

        if (0 == ret) {
            free(chunk);
            // If timeout before we have read any characters then return error
            // otherwise we assume that the read bytes are all there is and
            // return as success
            if( tot_read > 0 ) {
                return tot_read;
            } else {
                return -2; // Timeout
            }
        }
        if (ret < 0) {
            logmsg(LOG_ERR,"Serial poll() error. ( %d : %s )",errno,strerror(errno));
            free(chunk);
            return -1; // Other error
        }
        nread = 0;
        if (fds.revents & POLLIN || fds.revents & POLLPRI) {
            if ((nread = read(sfd, chunk, chunkSize)) == -1) {
                logmsg(LOG_ERR,"Cannot read serial port. ( %d : %s )",errno,strerror(errno));
                free(chunk);
                return -1;
            }
            cptr = chunk;
            if( tot_read+nread > (ssize_t)maxlen ) {
                // Data is larger than buffer so quit with an error
                free(chunk);
                logmsg(LOG_ERR,"serial_chunkread() : Buffer too small for data");
                return -1;
            } else {
                for( ssize_t i=0; i < nread; i++) {
 *bptr++ = *cptr++;
                }
                tot_read += nread;
                if( NULL != cb ) {
                    cb(arg,(int)tot_read);
                    chunkNbr++;
                }
            }
        } else {
            logmsg(LOG_ERR,"Unexpected poll() event");
            free(chunk);
            return -1;
        }
    } while( nread == (ssize_t)chunkSize );
    free(chunk);
    return tot_read;
#endif
}
 */

/**
 * Set timeout for rserial eading operation
 * @param msec Timeout in milliseconds
 * @return 0 on sucess, -1 on failure
 */
int
serial_set_read_timeout(const int msec) {
    read_timeout = msec;
    return 0;
}

/**
 * Read one line of reply from device on the previous opened serial
 * socket identified by the first argument.
 * The reply is returned in the newly allocated
 * buffer pointed to by the second argument. The reply will be read up to
 * the terminating "\r\n" character sequence. On timeout the routine will
 * try up to three times before giving up.
 * It is the calling routines responsibility to free the allocated memory
 * when the reply is no longer needed.
 * @param sfd Serial device descriptor
 * @param[out] reply Is set to a pointer to the reply.
 * @return 0 on success, -1 on failure in which case no memory is allocated
 * for the reply buffer.
 */

int
serial_read_line(const int sfd, char **reply) {
    size_t max_read = 100 * 1024;   // 100K max size to read from device
    const size_t MAXLOG = 2 * 1024; //   2K max size when logging the received data
    const size_t TIMEOUT = 3000;    //   3s timeout when reading from device

    int tries = 0;
    const int MAX_TRIES = 5;
    _Bool finished = FALSE;

    *reply = NULL;
    char *rbuff = _chk_calloc_exit(max_read);
    char *logbuff = _chk_calloc_exit(MAXLOG);
    char *bptr = rbuff;

    do {
        logmsg(LOG_DEBUG, "Reading reply from device. Try %d", tries + 1);
        int rc = serial_read_timeout(sfd, max_read, bptr, TIMEOUT);
        if (rc < 0 && rc != SERIALRD_TIMEOUT) {
            logmsg(LOG_ERR, "Cannot read response from device (rc=%d)", rc);
            free(rbuff);
            free(logbuff);
            return -1;
        }
        if( rc==SERIALRD_TIMEOUT )
            rc=0;
        else
            strncpy(logbuff, bptr, MAXLOG);
        if (rc < (int) MAXLOG)
            logbuff[rc] = '\0';
        else
            logbuff[MAXLOG - 1] = '\0';
        xstrtrim_crnl(logbuff);
        logmsg(LOG_DEBUG, "Read %d chars:\"%s\"", rc, logbuff);
        bptr += rc;
        max_read -= rc;
        tries++;

        // Check for end "\r\n" which indicates that the device is
        // done sending information. If not we keep reading information
        // until we get the ending or we have tried MAX_TRIES times
        finished = *(bptr - 1) == '\n' && *(bptr - 2) == '\r';
        
        if( finished ) {
            logmsg(LOG_DEBUG, "Found \\r\\n sequence at end of reply indicating the end.");
        }
                

    } while (!finished && tries < MAX_TRIES && max_read > 1);
    *bptr = '\0';
    free(logbuff);

    if (!finished) {
        logmsg(LOG_ERR, "Did not receive the ending \"\\r\\n\" at end of reply: \"%s\"", rbuff);
        free(rbuff);
        return -1;
    }

    *reply = rbuff;
    return 0;
}


/* EOF */
