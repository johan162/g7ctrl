/* =========================================================================
 * File:        json_util.c
 * Description: Utility functions to support working with JSON format
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: json_util.c 696 2015-02-02 06:42:40Z ljp $
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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <sys/param.h>

#include "config.h"
#include "build.h"
#include "utils.h"
#include "futils.h"
#include "logger.h"
#include "g7config.h"
#include "xstr.h"
#include "json_util.h"



/**
 * Format a JSON reply that is either a fail or success. This will format the
 * variable length message argument and wrap it into a JSON object if use_jsaon argument
 * is true. Otherwise it will just print out the string. If the err is true the string
 * will be prepended by a "[ERR]" indicator.
 * @param[in] err True if this should be an error message
 * @param[in] errno Error number
 * @param[in] use_json True if JSON format should be used
 * @param[in] fd File descripor to write to
 * @param[in] buf Variable arg start buffer
 * @return -1 on failure, 0 on success
 */
int
_writef_json(const _Bool err, const int errnum, const _Bool use_json, const int fd, const char *buf, ...)  {
    if( fd >= 0 ) {
        const int blen = MAX(1024*20,strlen(buf)+1);

        char *tmpbuff = _chk_calloc_exit(-1,blen);
        va_list ap;
        va_start(ap, buf);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        vsnprintf(tmpbuff, blen, buf, ap);
#pragma clang diagnostic pop
#pragma GCC diagnostic pop

        tmpbuff[blen-1] = 0;
        int ret;
        if( use_json ) {
            if( err ) {
                ret = _writef(fd,"{\"status\":\"ERR\",\"errno\":%d,\"msg\":\"%s\"}",errnum,tmpbuff);
                logmsg(LOG_ERR,tmpbuff);
            } else {
                ret = _writef(fd,"{\"status\":\"OK\",\"msg\":\"%s\"}",tmpbuff);
                logmsg(LOG_INFO,tmpbuff);
            }
        } else {
            if( err ) {
                ret = _writef(fd, "[ERR] %s", tmpbuff);
            } else {
                ret = write(fd, tmpbuff, strnlen(tmpbuff,blen));
            }
            logmsg(LOG_DEBUG,tmpbuff);
        }
        free(tmpbuff);
        va_end(ap);
        return ret;
    }
    return -1;
}

/**
 * Create a response indicating success with a message ifJSON
 * format is enabled. Plain writef() if not enabled.
 * This is a simplified version of _writef_json()
 * @param use_json
 * @param sockd
 * @param msg
 */
int
_writef_json_msg_ok(const _Bool use_json, int sockd, const char *msg) {
    _writef_json(FALSE, use_json, sockd, msg);
    if( use_json ) {
       return _writef(sockd,"{\"status\":\"OK\",\"msg\":\"%s\"}",msg);
    } else {
       return _writef(sockd,"%s",msg);
    }
}

/**
 * Create a response indicating failure with a message if JSON
 * format is enabled. Use plain writef() if not enabled.
 * This is a simplified version of _writef_json()
 * @param use_json
 * @param sockd
 * @param __errno_location
 * @param msg
 */
int
_writef_json_msg_err(const _Bool use_json, int sockd, const int errnum, static char *msg) {
    if( use_json ) {
       return _writef(sockd,"{\"status\":\"ERR\",\"errno\":%d,\"msg\":\"%s\"}",errnum,msg);
    } else {
       return _writef(sockd,"[ERR] %s",msg);
    }
}






