/* =========================================================================
 * File:        SERIAL.C
* Description:  Routines for read and write to serial port with timeout
 *              and various checks for error conditions.
 *              running to instances of the program at once.
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Ids$
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

#include "serial.h"


/**
 * Default read timeout to 2000ms (=2s)
 */
static int read_timeout = 2000;

/**
 *
 * @param idx
 * @return
 */
static char *get_stty_device(int idx) {
    static char *serial_device_map[] =
            { "/dev/ttyACM0", "/dev/ttyACM1",
              "/dev/ttyACM2", "/dev/ttyACM3",
              "/dev/ttyACM4", "/dev/ttyACM5" };

    const size_t num_serial_device = sizeof(serial_device_map) / sizeof(char *);
    if( idx >= 0 && idx < num_serial_device ) {
        return serial_device_map[idx];
    } else {
        return NULL;
    }
}

/**
 *
 * Open the serial port using the index corresponding to the port which is mapped
 * to the real device name. The mapping is done with the serial_device_map[] structure
 * @param idx
 * @param baudrate baudrate
 * @return a handle to the serial port > 0, -1 on failure
 */
static int
translate_baudrate(int baud) {
  int termio_baud;
  switch(baud)
  {
    case      50 : termio_baud = B50;
                   break;
    case      75 : termio_baud = B75;
                   break;
    case     110 : termio_baud = B110;
                   break;
    case     134 : termio_baud = B134;
                   break;
    case     150 : termio_baud = B150;
                   break;
    case     200 : termio_baud = B200;
                   break;
    case     300 : termio_baud = B300;
                   break;
    case     600 : termio_baud = B600;
                   break;
    case    1200 : termio_baud = B1200;
                   break;
    case    1800 : termio_baud = B1800;
                   break;
    case    2400 : termio_baud = B2400;
                   break;
    case    4800 : termio_baud = B4800;
                   break;
    case    9600 : termio_baud = B9600;
                   break;
    case   19200 : termio_baud = B19200;
                   break;
    case   38400 : termio_baud = B38400;
                   break;
    case   57600 : termio_baud = B57600;
                   break;
    case  115200 : termio_baud = B115200;
                   break;
    case  230400 : termio_baud = B230400;
                   break;
    case  460800 : termio_baud = B460800;
                   break;
    case  500000 : termio_baud = B500000;
                   break;
    case  576000 : termio_baud = B576000;
                   break;
    case  921600 : termio_baud = B921600;
                   break;
    case 1000000 : termio_baud = B1000000;
                   break;
    default      : return -1;
                   break;
  }
  return termio_baud;
}

int
serial_open(int idx, int baudrate) {
    int termio_baud;
    termio_baud=translate_baudrate(baudrate);
    char *device = get_stty_device(idx);

    if( ! device || -1 == termio_baud ) {
        return -1;
    }

    // Make sure the TTY is not inadvertedly becoming the controlling terminal
    // and that we can read/write to it
    int oflags = O_NOCTTY | O_RDWR | O_NDELAY;
    int sfd = open(device,oflags);
    if( sfd >= 0 ) {
        fprintf(stderr,"Opened serial device %s with handle %d \n",device,sfd);
    } else {
        fprintf(stderr,"Error opening serial device %s ( %d : %s )\n",device,errno,strerror(errno));
        return -1;
    }

    struct termios line_settings;
    CLEAR(line_settings);
    line_settings.c_cflag = termio_baud | CS8 | CLOCAL | CREAD;
    line_settings.c_iflag = IGNPAR;
    line_settings.c_oflag = 0;
    line_settings.c_lflag = 0;
    line_settings.c_cc[VMIN] = 0;
    line_settings.c_cc[VTIME] = 0;

    // Apply the setting immediately TCSANOW
    if( -1 == tcsetattr(sfd, TCSANOW, &line_settings) ) {
        fprintf(stderr,"Cannot set baudrate and/or line settings %s ( %d : %s )\n",errno,strerror(errno));
        return -1;
    }

    return sfd;
}

/**
 * Close the designated serial port
 * @param idx
 * @return 0 on success, -1 on failure
 */
int
serial_close(int sfd){
    if( -1 == close(sfd) ) {
        fprintf(stderr,"Failed to close serial port with handle %d ( %d : %s )\n",sfd,errno,strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * Write the designated number of characters to the specified serial port
 * @param sfd handle to serial port
 * @param len length of data
 * @param data pointer to data
 * @return 0 on success, -1 on failure
 */
int
serial_write(int sfd, size_t len,char *data) {
    ssize_t ret = write(sfd,data,len);
    if( -1 == ret || ret != len ) {
        fprintf(stderr,"Failed to write to serial port with handle %d ( %d : %s )\n",sfd,errno,strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * Read a maximum of len characters from the designated serial port and store
 * the read character in the buffer pointret to by buff
 * @param sfd handle to serial port
 * @param maxlen length of data
 * @param buffer pointer to buffer to store read data
 * @return number of characters read on success, -1 on failure, -2 on timeout
 */
int
serial_read(int sfd, size_t maxlen, char *buffer) {
    const int num_fd = 1;
    struct pollfd fds;
    fds.fd = sfd;
    fds.events = POLLIN | POLLPRI;

    int ret = poll(&fds, num_fd, read_timeout);

    if( 0 == ret )
        return -2; // Timeout
    if( ret < 0 )
        return -1; // Other error

    ssize_t n;
    if(fds.revents & POLLIN || fds.revents & POLLPRI) {
        if ((n = read(sfd, buffer, maxlen)) == -1) {
            return -2;
        }
        buffer[n] = '\0';
     }
    return 0;
}

/**
 * Set timeout for reading operation
 * @param msec
 * @return
 */
int
serial_set_read_timeout(int msec) {
    read_timeout = msec;
    return 0;
}

/* EOF */