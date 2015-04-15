/* =========================================================================
 * File:        json_util.h
 * Description: Utility functions to support working with JSON format
 * Author:      Johan Persson (johan162@gmail.com)
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

#ifndef JSON_UTIL_H
#define	JSON_UTIL_H

#ifdef	__cplusplus
extern "C" {
#endif

int
_writef_json(const _Bool err, const int errnum, const _Bool use_json, const int fd, const char *buf, ...)
        __attribute__ ((format (printf, 5, 6)));

int
_writef_json_msg_ok(const _Bool use_json, int sockd,  const char *msg);

int
_writef_json_msg_err(const _Bool use_json, int sockd, const int errnum, const char *msg);

#ifdef	__cplusplus
}
#endif

#endif	/* JSON_UTIL_H */

