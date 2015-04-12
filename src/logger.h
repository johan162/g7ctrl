/* =========================================================================
 * File:        LOGGER.H
 * Description: Functions to manage logfile
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: logger.h 839 2015-03-16 01:23:49Z ljp $
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

#ifndef LOGGER_H
#define	LOGGER_H

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * Determine if several identical messages should be compressed
 * into one line.
 */
#define LOG_COMPRESS 0

/**
 * Maximum logfile size before it is rotated
 */
#define MAX_LOGFILE_SIZE (500*1024)

void
_vsyslogf(int priority, const char *msg, ...)
         __attribute__ ((format (printf, 2, 3)));

void
logmsg(int priority, const char *msg, ...)
        __attribute__ ((format (printf, 2, 3)));

int
setup_logger(char *packageName) ;

#ifdef	__cplusplus
}
#endif

#endif	/* LOGGER_H */

