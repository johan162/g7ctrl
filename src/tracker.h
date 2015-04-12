/* =========================================================================
 * File:        TRACKER.C
 * Description: Listening for incoming packages on the designated
 *              socket that the device is connecting on. All incomming
 *              trafic is stored in the tracker log file.
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: tracker.h 623 2015-01-08 07:25:36Z ljp $
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


#ifndef TRACKER_H
#define	TRACKER_H

#ifdef	__cplusplus
extern "C" {
#endif

void *
tracker_clientsrv(void *arg);

#ifdef	__cplusplus
}
#endif

#endif	/* TRACKER_H */

