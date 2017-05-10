
/* =========================================================================
 * File:        WREPLY.C
 * Description: Functions to handle writing back a reply to a client
 *
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

// We want the full POSIX and C99 standard
#define _GNU_SOURCE

// Standard defines
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/param.h>

#include "config.h"
#include "g7ctrl.h"
#include "g7config.h"
#include "utils.h"
#include "libxstr/xstr.h"
#include "logger.h"
#include "wreply.h"

/**
 * _writef_reply_interactive
 * Send back a regular reply to the client (no errors) when the client
 * is interactive. For programmatical interactions this will do nothing
 *
 * @param fd File/socket to write to
 * @param buf Format string
 * @param ... Number of args depends on the format string
 * @return  0 on success, -1 on failure
 */
int
_writef_reply_interactive(const int fd, const char *format, ...) {
    if (fd >= 0) {
        const int blen = MAX(SIZE_10KB, strlen(format) + 1);

        char *tmpbuff = _chk_calloc_exit(blen);
        va_list ap;
        va_start(ap, format);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        vsnprintf(tmpbuff, blen, format, ap);
#pragma clang diagnostic pop
#pragma GCC diagnostic pop

        tmpbuff[blen - 1] = 0;
        int ret = write(fd, tmpbuff, strnlen(tmpbuff, blen));
        free(tmpbuff);
        va_end(ap);
        return ret;
    }
    return -1;
}

/**
 * _writef_reply
 * Send back a regular reply to the client (no errors)
 *
 * @param fd File/socket to write to
 * @param buf Format string
 * @param ... Number of args depends on the format string
 * @return  0 on success, -1 on failure
 */
int
_writef_reply(const int fd, const char *format, ...) {
    if (fd >= 0) {
        const int blen = MAX(SIZE_10KB, strlen(format) + 1);

        char *tmpbuff = _chk_calloc_exit(blen);
        va_list ap;
        va_start(ap, format);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        vsnprintf(tmpbuff, blen, format, ap);
#pragma clang diagnostic pop
#pragma GCC diagnostic pop

        tmpbuff[blen - 1] = 0;
        int ret = write(fd, tmpbuff, strnlen(tmpbuff, blen));
        free(tmpbuff);
        va_end(ap);
        return ret;
    }
    return -1;
}

/**
 * _writef_reply_err
 * Send back a reply indicating failure to the client
 *
 * @param fd File/socket to write to
 * @param buf Format string
 * @param ... Number of args depends on the format string
 * @return  0 on success, -1 on failure
 */
int
_writef_reply_err(const int fd, int errnum, const char *format, ...) {
    if (fd >= 0) {
        const int blen = MAX(SIZE_10KB, strlen(format) + 1);

        char *tmpbuff = _chk_calloc_exit(blen);
        va_list ap;
        va_start(ap, format);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        vsnprintf(tmpbuff, blen, format, ap);
#pragma clang diagnostic pop
#pragma GCC diagnostic pop

        tmpbuff[blen - 1] = 0;
        int ret = _writef(fd, "[ERR=%d] %s", errnum, tmpbuff);
        logmsg(LOG_ERR, "%s", tmpbuff);
        free(tmpbuff);
        va_end(ap);
        return ret;
    }
    return -1;
}

