/* =========================================================================
 * File:        SERIAL.H
 * Description: Routines for read and write to serial port with timeout
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
#ifndef SERIAL_H
#define	SERIAL_H

#ifdef	__cplusplus
extern "C" {
#endif

const char *
get_stty_device_name(const size_t idx);

int
serial_open(const char *device, const int baudrate);

int
serial_close(const int idx);

int
serial_write(const int sfd, const char *data, size_t len);

int
serial_read_timeout(const int sfd, size_t maxlen, char *buffer, const int timeout_msec);

int
serial_read(const int sfd, size_t len,char *buff);

int
serial_read_line(const int sfd, char **reply);

/*
int
serial_chunkread(const int sfd, const size_t maxlen, char *buffer, const int timeout_msec,
                 const size_t chunkSize, void (*cb)(void *, int), void *arg);
*/

int
serial_set_read_timeout(const int msec);

#ifndef __APPLE__
int
usb_reset(const int sfd);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* SERIAL_H */

