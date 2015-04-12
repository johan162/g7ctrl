/* =========================================================================
 * File:        WREPLY.H
 * Description: Functions to handle writing back a reply to a client
 *
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id$
 *
 * Copyright (C) 2015  Johan Persson
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

#ifndef WREPLY_H
#define	WREPLY_H

#ifdef	__cplusplus
extern "C" {
#endif

int
_writef_reply(const int fd, const char *format, ...)
        __attribute__ ((format (printf, 2, 3)));

int
_writef_reply_interactive(const int fd, const char *format, ...)
        __attribute__ ((format (printf, 2, 3)));

int
_writef_reply_err(const int fd, int errnum, const char *buf, ...)
        __attribute__ ((format (printf, 3, 4)));



#ifdef	__cplusplus
}
#endif

#endif	/* WREPLY_H */

