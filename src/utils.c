/* =========================================================================
 * File:        UTILS.C
 * Description: A collection of small utility functions used by the rest
 *              of the daemon.
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

// We want the full POSIX and C99 standard
#define _GNU_SOURCE

// Standard defines
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <math.h>
#include <syslog.h>
#include <math.h>

#include <errno.h>
#include <sys/param.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <locale.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <pcre.h>
#include <signal.h>

#include "config.h"
#include "g7ctrl.h"
#include "g7config.h"
#include "utils.h"
#include "libxstr/xstr.h"
#include "logger.h"


_Bool htmlencode_flag;

/**
 * Debug version of close()
 * @param fd
 * @return
 */
int
_x_dbg_close(int fd) {

    logmsg(LOG_NOTICE, "dbg_close() : fd=%d", fd);
    return close(fd);

}

/*
 * Allocate memory and also check for out of memory condition and in that
 * case terminate program vi exit(EXIT_FAILURE)
 * @param num Number of elements to allocate
 * @param size Size of each elment
 * @return Pointer to valid allocated memory

void *
_chk_calloc_OLD(const size_t num, const size_t size) {
    void *ptr = calloc(num, size);
    if( ptr == NULL ) {
        logmsg(LOG_ERR,"FATAL: Cannot allocate memory. Terminating.");
        exit(EXIT_FAILURE);
    } else {
        return ptr;
    }
}
 */

/**
 * Allocate a memory block of size "size" and return a pointer to it.
 * @param size Size in bytes to allocate
 * @return <> NULL pointer to newly allocated memory,
 * @note: This is a possible exit point in the program.
 */
void *
_chk_calloc_exit(size_t size) {
    char *buff = calloc(size, sizeof (char));
    if (NULL == buff) {
        // Unfortunately this is so critical we cannot even generate a message
        // since that requires heap memory - which we obviously don't have.
        // For practical reason this is very, very unlikely to happen on a
        // real world server.
#ifdef   SIGSEGV_HANDLER
        // Force a stackdump if program compiled with crashdump options
        abort();
#else
        exit(EXIT_FAILURE);
#endif
    }
    return buff;
}

/**
 * _writef
 * Utility function
 * Simplify a formatted write to a file descriptor
 *
 * @param fd File/socket to write to
 * @param buf Format string
 * @param ... Number of args depends on the format string
 * @return  0 on success, -1 on failure
 */
int
_writef(const int fd, const char *buf, ...) {
    if (fd >= 0) {
        const int blen = SIZE_1MB;
        
        char *tmpbuff = _chk_calloc_exit(blen);
        va_list ap;
        va_start(ap, buf);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-result"        
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic ignored "-Wunused-result"            
        int len = vsnprintf(tmpbuff, blen, buf, ap);
        if( len > blen ) {
            // Output was truncated
            logmsg(LOG_CRIT,"Internal error. Output truncated in _writef()");
            const char *errstr = "Error: Output truncated.";
            (void)write(fd, errstr, strlen(errstr));
            free(tmpbuff);
            va_end(ap);
            return -1;
        }
#pragma clang diagnostic pop
#pragma GCC diagnostic pop

        tmpbuff[blen - 1] = 0;
        int ret;
        if (htmlencode_flag) {
            char *htmlbuff = html_encode(tmpbuff);
            ret = write(fd, htmlbuff, strlen(htmlbuff));
            free(htmlbuff);
        } else {
            ret = write(fd, tmpbuff, strlen(tmpbuff));
        }
        free(tmpbuff);
        va_end(ap);
        return ret;
    }
    return -1;
}

/**
 * Utility function that uses Perl Regular Expression library to match
 * a string and return an array of the found subexpressions
 * NOTE: It is the calling routines obligation to free the returned
 * field with a call to
 * pcre_free_substring_list((const char **)field);
 *
 * @param regex Regular expression to match the command
 * @param cmd Command string to test
 * @param field The identified fields in the command string
 * @return -1 on failure, 0 on success
 */
int
matchcmd(const char *regex, const char *cmd, char ***field) {
    pcre *cregex;
    int ovector[100];
    const char *errptr;
    int erroff, ret;

    cregex = pcre_compile(regex, PCRE_CASELESS | PCRE_MULTILINE | PCRE_NEWLINE_CRLF | PCRE_UTF8,
            &errptr, &erroff, NULL);

    if (cregex) {
        ret = pcre_exec(cregex, NULL, cmd, strlen(cmd), 0, 0, ovector, 90);
        pcre_free(cregex);
        if (ret > 0) {
            (void) pcre_get_substring_list(cmd, ovector, ret, (const char ***) field);
            return ret;
        }
    }
    return -1;
}

/**
 * Utility function that uses Perl Regular Expression library to match
 * a string and return an array of the found subexpressions
 * NOTE: It is the calling routines obligation to free the returned
 * field with a call to
 * pcre_free_substring_list((const char **)field);
 * This function differs from the matchcmd() function in that it handles
 * a possible multi-line command string.
 * @param regex
 * @param cmd
 * @param field
 * @return -1 on failure, 0 on success
 */
int
matchcmd_ml(const char *regex, const char *cmd, char ***field) { //, const char *func, int line) {
    pcre *cregex;
    int ovector[100];
    const char *errptr;
    int erroff, ret;

    //logmsg(LOG_DEBUG, "matchcmd() called from '%s()' at line #%05d",func,line);
    cregex = pcre_compile(regex, PCRE_CASELESS | PCRE_UTF8 | PCRE_NEWLINE_CRLF | PCRE_MULTILINE, &errptr, &erroff, NULL);
    if (cregex) {
        ret = pcre_exec(cregex, NULL, cmd, strlen(cmd), 0, 0, ovector, 90);
        pcre_free(cregex);
        if (ret > 0) {
            (void) pcre_get_substring_list(cmd, ovector, ret, (const char ***) field);
            return ret;
        }
    }
    return -1;
}

/**
 * Free the allocated fields after a successful command match
 * @param field
 */
void
matchcmd_free(char ***field) { //,const char *func, int line) {
    //logmsg(LOG_DEBUG, "matchcmd_free() called from '%s()' at line #%05d",func,line);
    if (field != (void *) NULL) {
        pcre_free_substring_list((const char **) *field);
        *field = NULL;
    }
}

/**
 * Fill the supplied buffer with 'num' repeats of character 'c'
 * It is the calling routines responsibility that buf is large enough.
 * NOTE: The length is capped at 255 characters to avoid run-away
 * arguments.
 * @param c Character to fill
 * @param num Number of characters
 * @param buf Buffer to fill
 * @return pointer to buffer
 */
char *
rptchr_r(char c, const size_t num, char *buf) {
    size_t n = MIN(255, num);
    for (unsigned i = 0; i < n; i++)
        buf[i] = c;
    buf[n - 1] = '\0';
    return buf;
}

/**
 * Validate a given parameter against a min/max value. Abort the program
 * with exit(EXIT_FAILURE) if the test fails.
 * @param min
 * @param max
 * @param name
 * @param val
 * @return val if in bounds
 */
int
validate(const int min, const int max, const char *name, const int val) {
    if (val >= min && val <= max)
        return val;
    logmsg(LOG_ERR, "Value for '%s' is out of allowed range [%d,%d]. Aborting. \n", name, min, max);
    (void) exit(EXIT_FAILURE);
    return -1;
}

/**
 * Validate a given parameter against a min/max value. Abort the program
 * with exit(EXIT_FAILURE) if the test fails.
 * @param min
 * @param max
 * @param name
 * @param val
 * @return val if in bounds
 */
double
validate_double(const double min, const double max, const char *name, const double val) {
    if (val >= min && val <= max)
        return val;
    logmsg(LOG_ERR, "Value for '%s' is out of allowed range [%f,%f]. Aborting. \n", name, min, max);
    (void) exit(EXIT_FAILURE);
    return -1;
}


/**
 * Get system load averages
 * @param[out] avg1 1 min average
 * @param[out] avg5 5 min average
 * @param[out] avg15 15 min average
 * @return 0 on success, -1 on failure
 */
int
getsysload(double *avg1, double *avg5, double *avg15) {
    *avg1 = -1;
    *avg5 = -1;
    *avg15 = -1;

#ifdef __APPLE__
    char cmd[32];
    snprintf(cmd, sizeof (cmd), "sysctl -n vm.loadavg");
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char lbuff[64];
        char * ret = fgets(lbuff, sizeof (lbuff) - 1, fp);
        if (NULL == ret) {
            (void) pclose(fp);
            return -1;
        }
        lbuff[sizeof (lbuff) - 1] = '\0';
        lbuff[strlen(lbuff) - 1] = '\0'; // Get rid of newline

        if (0 == pclose(fp)) {
            // A typical returned string looks like
            // { 0.81 0.34 0.13 }
            if (3 != sscanf(lbuff, "{ %lf%lf%lf }", avg1, avg5, avg15)) {
                return -1;
            } else {
                return 0;
            }
        } else {
            return -1;
        }
    } else {
        return -1;
    }
#else
    int ld = open("/proc/loadavg", O_RDONLY);
    if (ld >= 0) {
        char lbuff[30];
        if (-1 == read(ld, lbuff, 30)) {
            close(ld);
            logmsg(LOG_ERR, "FATAL: Cannot read '/proc/loadavg' ( %d : %s )", errno, strerror(errno));
            return -1;
        }
        close(ld);
        lbuff[14] = '\0';
        sscanf(lbuff, "%lf%lf%lf", avg1, avg5, avg15);
        logmsg(LOG_DEBUG, "Load avg: %f %f %f", *avg1, *avg5, *avg15);
        return 0;
    }
    logmsg(LOG_DEBUG, "Failed to read /proc/loadavg ( %d : %s )", errno, strerror(errno));
    return -1;
#endif
}

/**
 * Get total system uptime
 * @param[out] totaltime
 * @param[out] idletime
 */
void
getuptime(int *totaltime, int *idletime) {
    *totaltime = -1;
    *idletime = -1;
    int ld = open("/proc/uptime", O_RDONLY);
    if (ld >= 0) {
        char lbuff[24];
        if (-1 == read(ld, lbuff, 24)) {
            logmsg(LOG_ERR, "FATAL: Cannot read '/proc/uptime' ( %d : %s )", errno, strerror(errno));
            *totaltime = 0;
            *idletime = 0;
        }
        close(ld);

        float tmp1, tmp2;
        sscanf(lbuff, "%f%f", &tmp1, &tmp2);
        *totaltime = round(tmp1);
        *idletime = round(tmp2);
    }
}

/**
 * Set the FD_CLOEXEC flag on the specified file. This will cause the
 * file descriptor to automatically close when we do a process replacement
 * via a exec() family call.
 * @param fd File descriptor
 * @param flag TRUE to set the flag, FALSE to clear it
 * @return -a on failure, 0 on success
 */
int
set_cloexec_flag(const int fd, const _Bool flag) {
    int oldflags = fcntl(fd, F_GETFD, 0);

    if (oldflags < 0)
        return oldflags;

    if (flag != 0) {
        oldflags |= FD_CLOEXEC;
    } else {
        oldflags &= ~FD_CLOEXEC;
    }

    return fcntl(fd, F_SETFD, oldflags);
}

/**
 * Find out the size of the working set for the specified process id
 * and the current number of running threads.
 * This corresponds to the allocated virtual memory for this process.
 * @param pid process id
 * @param[out] size store size
 * @param[out] unit string with the unit for the size (normally kB)
 * @param[out] threads Number of running threads
 * @return 0 on success -1 on failure
 */
int
getwsetsize(const int pid, int *size, char *unit, int *threads) {
    char buffer[256];
    char linebuffer[1024];

    *size = -1;
    *unit = '\0';
    *threads = -1;

    snprintf(buffer, sizeof(buffer),"/proc/%d/status", pid);

    FILE *fp = fopen(buffer, "r");
    if (fp == NULL) {
        logmsg(LOG_ERR, "Cannot open '%s' (%d : %s)\n", buffer, errno, strerror(errno));
        return -1;
    }

    char tmpbuff[64];
    while (NULL != fgets(linebuffer, 1023, fp)) {
        xmb_strncpy(tmpbuff, linebuffer, 6);
        tmpbuff[6] = '\0';
        if (strcmp(tmpbuff, "VmSize") == 0) {
            sscanf(linebuffer, "%53s %d %4s", tmpbuff, size, unit);
        }
        if (strcmp(tmpbuff, "Thread") == 0) {
            sscanf(linebuffer, "%63s %d", tmpbuff, threads);
            break;
        }
    }

    fclose(fp);

    if (-1 == *size || -1 == *threads || '\0' == *unit) {
        logmsg(LOG_ERR, "getwsetsize() : Failed to read process information.");
        return -1;
    }

    return 0;

}

/**
 * Make a call to 'df' system function in order to find out remaining disk space
 * space. It is the calling routines responsibility that the buffers are large
 * enough to store the resulting data.
 * @param dir Directory to find size of
 * @param[out] fs Store file system found (the mounted file system)
 * @param[out] size Store size of file system
 * @param[out] used Store used size
 * @param[out] avail Store available size
 * @param[out] fill Percent fill as integer 0-100
 * @return 0 success, -1 failure
 */
int
get_diskspace(const char *dir, char *fs, char *size, char *used, char *avail, int *fill) {
    char cmd[512];
    snprintf(cmd, sizeof (cmd), "df -hP %s 2>&1", dir);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buffer[512];
        char * ret1 = fgets(buffer, sizeof (buffer) - 1, fp); // Read and discard header
        char * ret2 = fgets(buffer, sizeof (buffer) - 1, fp);
        if (NULL == ret1 || NULL == ret2) {
            (void) pclose(fp);
            return -1;
        }
        buffer[sizeof (buffer) - 1] = '\0';
        buffer[strlen(buffer) - 1] = '\0'; // Get rid of newline

        if (0 == pclose(fp)) {
            // A typical returned string looks like
            // //192.168.0.199/media  4.1T  612G  3.5T  15% /mnt/omega/mm

            int ret = sscanf(buffer, "%64s %8s %8s %8s %d", fs, size, used, avail, fill);
            if (5 != ret) {
                return -1;
            } else {
                return 0;
            }
        } else {
            return -1;
        }
    } else {
        return -1;
    }
}

/**
 * Get filesize of named file
 * @param fname File name
 * @param[out] filesize Returned filesize in kB
 * @return 0 on success, -1 on failure
 */
int
get_filesize_kb(const char *fname, unsigned *filesize) {
    struct stat statbuf;
    if (stat(fname, &statbuf)) {
        logmsg(LOG_ERR, "Failed to call stat() ( %d : %s )", errno, strerror(errno));
        return -1;
    }
    *filesize = statbuf.st_size / 1024;
    logmsg(LOG_ERR, "Filesize %s = %u", fname, *filesize);
    return 0;
}

/**
 * Rotate a file name if the size is above the specified size
 * The file is then renamed to a new a file with the date time
 * added to the name.
 * @param fname Original name
 * @param max_size_kb Maximum size
 * @return 0 if the file has been rotated, -1 otherwise
 */
int
rotate_file(const char *fname, const unsigned max_size_kb) {
    struct stat statbuf;
    if (stat(fname, &statbuf)) {
        return -1;
    }
    unsigned size = statbuf.st_size / 1024;

    if (size > max_size_kb) {
        char tbuff[24];
        struct tm tm;
        time_t t = time(NULL);
        localtime_r(&t, &tm);
        strftime(tbuff, sizeof (tbuff), "%Y%m%d%H%M%S", &tm);
        char nfname[512];
        snprintf(nfname, sizeof (nfname) - 1, "%s-%s", fname, tbuff);
        if (rename(fname, nfname)) {
            return -1;
        }
        char cmdBuff[1024];
        snprintf(cmdBuff, sizeof (cmdBuff) - 1, "/usr/bin/gzip -f %s > /dev/null 2>&1", nfname);
        int rc = system(cmdBuff);
        if (rc) {
            return -1;
        }
        return 0;
    }
    return -1;
}

/**
 * Double escape percent sign in a buffer in preparation form formatted output
 * It is the calling routines responsibility to free the newly allocated
 * buffer. The original string is not harmed.
 * @param str String to escape
 * @return A pointer to an allocated buffer with escaped string
 */
char *
esc_percentsign(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    char *buff = _chk_calloc_exit(strlen(str)*3 + 1);
    char *pbuff = buff;
    while (*str) {
        if (*str == '%') {
            *pbuff++ = '%';
            *pbuff++ = '%';
        } else {
            *pbuff++ = *str;
        }
        str++;
    }
    *pbuff = '\0';
    return buff;
}

/**
 * Converts a hex character to its integer value
 * @param ch Hex character
 * @return the integer value corresponding to the hex char
 */

char from_hex(const char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/**
 * Convert integer (in raneg 0-15) to hex character. It is the calling
 * routines responsibility that the argument is in range
 * @param code
 * @return
 */
char to_hex(const char code) {
    static char hex[] = "0123456789ABCDEF";
    return hex[code & 0x0f];
}

/**
 * URL encode a buffer.
 * Note: Calling function is responsible to free returned string. The
 * original data is not touched.
 *
 * @param str Buffer to encode
 * @return A pointer to a newly allocated buffer with the encoded data
 */
char *
url_encode(const char *str) {
    const char *pstr = str;
    char *buf = _chk_calloc_exit(strlen(str) * 3 + 1);
    char *pbuf = buf;

    while (*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') {
            *pbuf++ = *pstr;
        } else if (*pstr == ' ') {
            *pbuf++ = '+';
        } else {
            *pbuf++ = '%';
            *pbuf++ = to_hex(*pstr >> 4);
            *pbuf++ = to_hex(*pstr);
        }
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

/**
 * URL decode a buffer.
 * Note: Calling function is responsible to free returned string

 * @param str Input string to decode
 * @return A newly allocated buffer with the decoded data
 */
char *
url_decode(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    const char *pstr = str;
    char *buf = _chk_calloc_exit(strlen(str) + 1);
    char *pbuf = buf;

    while (*pstr) {
        if (*pstr == '%') {
            if (pstr[1] && pstr[2]) {
                *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
                pstr += 2;
            }
        } else if (*pstr == '+') {
            *pbuf++ = ' ';
        } else {
            *pbuf++ = *pstr;
        }
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

/**
 * URL decode in the original string. This will modify the original buffer
 * @param str String to decode in place
 * @param maxlen
 */
void
url_decode_inplace(char *str, const size_t maxlen) {
    char *p = url_decode(str);
    xmb_strncpy(str, p, maxlen);
    free(p);
}

/**
 * URL decode a into a buffer.
 * @param decode
 * @param maxlen
 * @param str
 * @return 0 on success, -1 on failure
 */
int
url_decode_buff(char *decode, const size_t maxlen, const char *str) {
    size_t ml = maxlen;
    if (str == NULL) {
        return -1;
    }
    const char *pstr = str;
    while (ml > 1 && *pstr) {
        if (*pstr == '%') {
            if (pstr[1] && pstr[2]) {
                *decode++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
                pstr += 2;
            }
        } else if (*pstr == '+') {
            *decode++ = ' ';
        } else {
            *decode++ = *pstr;
        }
        pstr++;
        ml--;
    }
    *decode = '\0';
    if (1 == ml && *pstr) {
        return -1;
    } else {
        return 0;
    }
}

/**
 * HTML encode a buffer. Translate HTML entities into ampersand sequences
 * Note: Calling function is responsible to free returned string
 *
 * @param str
 * @return NULL if input is NULL, otherwise a newly allocated buffer
 * with the encoded data
 */
char *html_encode(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    const char *pstr = str;
    char *buf = _chk_calloc_exit(strlen(str) * 6 + 1);
    char *pbuf = buf;

    while (*pstr) {
        switch (*pstr) {
            case '<':
                *pbuf++ = '&';
                *pbuf++ = 'l';
                *pbuf++ = 't';
                *pbuf++ = ';';
                break;
            case '>':
                *pbuf++ = '&';
                *pbuf++ = 'g';
                *pbuf++ = 't';
                *pbuf++ = ';';
                break;
            case '&':
                *pbuf++ = '&';
                *pbuf++ = 'a';
                *pbuf++ = 'm';
                *pbuf++ = 'p';
                *pbuf++ = ';';
                break;
            case '"':
                *pbuf++ = '&';
                *pbuf++ = 'q';
                *pbuf++ = 'u';
                *pbuf++ = 'o';
                *pbuf++ = 't';
                *pbuf++ = ';';
                break;
            default:
                *pbuf++ = *pstr;
                break;
        }
        ++pstr;
    }
    *pbuf = '\0';
    return buf;
}

/**
 * Dump ascii values in string together with the string
 * @param buffer
 * @param maxlen
 * @param str
 * @return 0 on success, -1 on failure
 */
int
dump_string_chars(char *buffer, size_t maxlen, char const *str) {
    char tmpbuff[16];
    size_t len = strlen(str);
    if (buffer == NULL) {
        return -1;
    }
    snprintf(buffer, maxlen, "%s \n(", str);
    maxlen -= len + 3;
    for (size_t i = 0; i < len; ++i) {
        snprintf(tmpbuff, 15, "%02X,", (unsigned) str[i]);
        if (maxlen > 2)
            xstrlcat(buffer, tmpbuff, maxlen);
        else
            return -1;
        maxlen -= 3;
    }
    snprintf(tmpbuff, 15, ")\n");

    if (maxlen > 2)
        xstrlcat(buffer, tmpbuff, maxlen);
    else
        return -1;

    return 0;
}

/**
 * Create a timestamp from date and time
 * @param year
 * @param month
 * @param day
 * @param hour
 * @param min
 * @param sec
 * @return Timestamp created from the date and time supplied
 */
time_t
totimestamp(const int year, const int month, const int day,
        const int hour, const int min, const int sec) {

    time_t timestamp;
    struct tm time_struc;

    time_struc.tm_sec = sec;
    time_struc.tm_min = min;
    time_struc.tm_hour = hour;
    time_struc.tm_mday = day;
    time_struc.tm_mon = month - 1;
    time_struc.tm_year = year - 1900;
    time_struc.tm_isdst = -1;

    timestamp = mktime(&time_struc);

    if (timestamp == -1) {
        logmsg(LOG_ERR, "totimestamp() : Cannot convert tm to timestamp (%d : %s)",
                errno, strerror(errno));
        return -1;
    }

    return timestamp;
}

/**
 * Get date and time from a timestamp
 *
 * @param timestamp
 * @param[out] year
 * @param[out] month
 * @param[out] day
 * @param[out] hour
 * @param[out] min
 * @param[out] sec
 * @return 0 on success, -1 on failure
 */
int
fromtimestamp(const time_t timestamp, int* year, int* month, int* day,
        int* hour, int* min, int* sec) {

    struct tm time_struc;
    if (NULL == localtime_r(&timestamp, &time_struc)) {
        logmsg(LOG_ERR, "fromtimestamp() : Cannot convert timestamp (%d : %s)",
                errno, strerror(errno));
        return -1;
    }

    *year = time_struc.tm_year + 1900;
    *month = time_struc.tm_mon + 1;
    *day = time_struc.tm_mday;
    *hour = time_struc.tm_hour;
    *min = time_struc.tm_min;
    *sec = time_struc.tm_sec;

    return 0;
}

/**
 * Utility variant of the standard read() function which handles signal
 * interruption and connection reset in a graceful way. Basically it will
 * resume the read() operation when a temporary error has happened.
 *
 * @param sockd Socket descriptor
 * @param[out] buffer Buffer to hold the read string
 * @param buffLen Maximum length of the storage buffer
 * @return the number of bytes read, 0 on connection close/reset, otherwise
 *          < 0 indicates an error and errno is set.
 */
int
socket_read(const int sockd, void *buffer, const size_t buffLen) {
    int rc;
    while (TRUE) {
        rc = read(sockd, buffer, buffLen);
        if ((-1 == rc) && (EINTR == errno)) {
            continue;
        } else if ((-1 == rc) && (ECONNRESET == errno)) {
            rc = 0;
            break;
        } else {
            break;
        }
    }
    return rc;
}

/**
 * Convert a position in decimal representation to a position using
 * degrees and seconds. For example 57.80889 converts to 57 48' 32"
 * It is the responsibility of the calling routine that the out
 * buffer is at least 12 character long
 * @param[in] dec Position in decimal string format
 * @param[out] grad Position in deg, min and sec as dd mm' ss"
 * @return -1 on failure, 0 on success
 */
int
convert_decgrad(const char *dec, char *grad) {
    *grad = '\0';
    const double v = xatof(dec);
    if (0 == v) {
        return -1;
    }

    const long deg = floor(v);
    const long min = floor((v - deg) * 60.0);
    const long sec = lround((v - deg - min / 60.0)*3600.0);
    snprintf(grad, 12, "%ld %ld' %ld\"", deg, min, sec);
    return 0;
}

/**
 * Return a basic european date.
 *
 * Example is: "220618 13:23:19"
 * @return A pointer to an allocated string. It is the callers responsibility to free the
 * string after usage.
 */
char *
get_short_datetime(void) {
    const char* SHORT_DATE_FORMAT_STR = "%y%m%d %H:%M:%S";
    char tbuff[32];
    time_t t;
    struct tm *tm;

    t = time(NULL);
    tm = localtime(&t);
    if( tm == NULL ) {
        return NULL;
    }

    if (strftime(tbuff, 32, SHORT_DATE_FORMAT_STR, tm) == 0) {
        return NULL;
    }

    return strdup(tbuff);
}


/**
 * Get the current local date time in format YYYY-MM-DD HH:MM[:SS]
 * It is the calling routines responsibility that the result
 * buffer can hold 20 chars
 * @param res
 * @param sec Set to 1 to include seconds in result string
 * @return -1 on failure, 0 on success
 */
int
get_datetime(char *res, int sec) {
    time_t t;
    struct tm *tm;
    const int rsize = 20;

    t = time(NULL);
    tm = localtime(&t);
    if (tm == NULL) {
        return -1;
    }
    *res = '\0';
    if (sec) {
        if (0 == strftime(res, rsize, "%Y%m%d %H:%M:%S", tm)) {
            return -1;
        }
    } else {
        if (0 == strftime(res, rsize, "%Y%m%d %H:%M", tm)) {
            return -1;
        }
    }
    logmsg(LOG_DEBUG, "Current date-time: %s", res);
    return 0;
}


#ifdef __APPLE__
#ifndef HAVE_CLOCK_GETTIME
#include <sys/time.h>
// MacOS X has a monotonic clock, but it's not POSIX. 
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0x01 
#define CLOCK_MONOTONIC 0x02
#define CLOCK_MONOTONIC_RAW 0x02
static uint64_t monotonic_timebase_factor = 0;

int clock_gettime(int clock_id, struct timespec *ts) {
    struct timeval tval;
    int rc = 0;
    
    switch (clock_id) {
        case CLOCK_REALTIME:
            rc = gettimeofday(&tval, NULL);
            if (rc)
                return rc;
            ts->tv_nsec = tval.tv_usec * 1000;
            ts->tv_sec = tval.tv_sec;
            return rc;
            
        case CLOCK_MONOTONIC:
            if (monotonic_timebase_factor == 0) {
                mach_timebase_info_data_t timebase_info;
                (void) mach_timebase_info(&timebase_info);
                monotonic_timebase_factor = timebase_info.numer / timebase_info.denom;
            }
            uint64_t monotonic_nanoseconds = (mach_absolute_time() * monotonic_timebase_factor);
            ts->tv_sec = monotonic_nanoseconds / 1E9;
            ts->tv_nsec = monotonic_nanoseconds - ts->tv_nsec;
            return 0;
            
        default:
            return -1;
    }
} 
#endif
#endif


/**
 * Return a relative number of milliseconds since EPOC. Note that
 * this is only safe to use as a relative timer since it is not
 * adjusted for time zone of changes in system time so no absolute
 * time can be retrieved from this function.
 * @param t A pointer to a long unsigned that gets filled with the
 *          millisecond time 
 * @return  0 on success, -1 on failure
 */
int 
mtime(unsigned long *t) {
  struct timespec timespec;
  if( clock_gettime(CLOCK_MONOTONIC_RAW,&timespec) ) {
    return -1;
  }
  *t = (unsigned long)(timespec.tv_sec*1000+timespec.tv_nsec/1000000);
  return 0;
}


/**
 * Write the buffer to the named file. Existing file will be overwritten
 * For created files the permission will be set to 0644
 * @param fname File name
 * @param buff Buffer to write
 * @param buffsize Buffer size
 * @return 0 on success , -1 on failure
 */
int
_wbuff2file(const char *fname, const char *buff, const size_t buffsize) {
    int rc = 0;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, mode);
    if (fd >= 0) {
        ssize_t nwrite = write(fd, buff, buffsize);
        if (nwrite != (ssize_t) buffsize) {
            logmsg(LOG_DEBUG, "Error writing rec file to disk nwrite=%zd, strlen(rbuff)=%zd", nwrite, buffsize);
            rc = -1;
        }
        close(fd);
    } else {
        rc = -1;
    }
    return rc;
}

/**
 * Split compact datetime into a more readable variant. It is the calling
 * routines responsibility that the "buff" is at least 20 characters wide.
 * @param datetime String to be split
 * @param buff Buffer to write the split string to
 * @return Buffer pointer
 */
char *
splitdatetime(char *datetime, char *buff) {
    const char *p = datetime;
    strncpy(buff, p, 4);
    p += 4;
    buff[4] = '-';
    buff[5] = *p++;
    buff[6] = *p++;
    buff[7] = '-';
    buff[8] = *p++;
    buff[9] = *p++;
    buff[10] = ' ';
    buff[11] = *p++;
    buff[12] = *p++;
    buff[13] = ':';
    buff[14] = *p++;
    buff[15] = *p++;
    buff[16] = ':';
    buff[17] = *p++;
    buff[18] = *p++;
    buff[19] = '\0';
    return buff;
}


/* EOF */
