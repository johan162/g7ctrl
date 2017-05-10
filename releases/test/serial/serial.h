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

/**
 *
 * Open the serial port using the index corresponding to the port which is mapped
 * to the real device name. The mapping is done with the serial_device_map[] structure
 * @param idx
 * @param baudrate baudrate
 * @return a handle to the serial port > 0, -1 on failure
 */
int
serial_open(int idx, int baudrate);

/**
 * Close the designated serial port
 * @param idx
 * @return
 */
int
serial_close(int idx);

/**
 * Write the designated number of characters to the specified serial port
 * @param sfd handle to serial port
 * @param len length of data
 * @param data pointer to data
 * @return 0 on success, -1 on failure
 */
int
serial_write(int sfd, size_t len,char *data);

/**
 * Read a maximum of len characters from the designated serial port and store
 * the read character in the buffer pointret to by buff
 * @param sfd handle to serial port
 * @param len length of data
 * @param buff pointer to buffer to store read data
 * @return number of characters read on success, -1 on failure, -2 on timeout
 */
int
serial_read(int sfd, size_t len,char *buff);

/**
 * Set timeout for reading operation
 * @param msec
 * @return
 */
int
serial_set_read_timeout(int msec);

// Clear variable section in memory
#define CLEAR(x) memset (&(x), 0, sizeof(x))


#ifdef	__cplusplus
}
#endif

#endif	/* SERIAL_H */

