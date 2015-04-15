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



#ifndef CONNWATCHER_H
#define	CONNWATCHER_H

#ifdef	__cplusplus
extern "C" {
#endif

//extern volatile int watcher_usb_connected;

void *
connwatcher_thread(void *arg);

_Bool
is_usb_connected(const size_t idx);

void
list_usb_stat(struct client_info *cli_info);

int
get_usb_devicename(const size_t idx, char **device);

int
set_usb_device_target_by_index(struct client_info *cli_info, const size_t usb_idx);

#ifdef	__cplusplus
}
#endif

#endif	/* CONNWATCHER_H */

