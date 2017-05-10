/* =========================================================================
 * File:        UTILS.H
 * Description: A collection of small utility functions used by the rest
 *              of the server.
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

#ifndef _UTILS_H
#define	_UTILS_H

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef DEBUG_SIMULATE
#define _dbg_close(fd) _x_dbg_close(fd)
#else
#define _dbg_close(fd) close(fd)
#endif

/* Some useful size constants */
#define SIZE_10KB (10*1024)
#define SIZE_20KB (20*1024)
#define SIZE_50KB (50*1024)
#define SIZE_100KB (100*1024)
#define SIZE_1MB (1000*1024)

#define TMPBUFF_10K(name) char *name=_chk_calloc_exit(10*1024);
#define TMPBUFF_20K(name) char *name=_chk_calloc_exit(20*1024);

/**
 * TEMP_FAILURE_RETRY is a GNU lib:ism that comes in quite handy to
 * rerun system calls that was interrupted and has had a temporary
 * error and should be re-tried. For non-GNU lib systems we have to
 * provide a version of this macro ourself.
 */
#ifdef __APPLE__
# define TEMP_FAILURE_RETRY(expression) \
  (__extension__                                                              \
    ({ int __TF_result;                                                     \
       do __TF_result = (int) (expression);                                 \
       while (__TF_result == -1 && errno == EINTR);                             \
       __TF_result; }))
#endif


/**
 * Special usage with the _writef() function. If this flag is true then the
 * output will be HTML encoded. This will for example replace "<" with "&lt;"
 * and so on.
 */
extern _Bool htmlencode_flag;

int
_x_dbg_close(int fd);

/**
 * Pointer to a suitable pre-format function used with the _writef()
 * function. The primary intention is to handle HTML formatting of
 * returned command outputs transparently.
 */
extern char * (*_write_preformat_func)(char *);

int
_writef(const int fd, const char *buf, ...)
        __attribute__ ((format (printf, 2, 3)));

/*
#define matchcmd_free(field) _matchcmd_free((field),__FUNCTION__,__LINE__)
#define matchcmd(regex,cmd,field) _matchcmd((regex), (cmd),(field),__FUNCTION__,__LINE__)
*/

int
matchcmd(const char *regex, const char *cmd, char ***field);

int
matchcmd_ml(const char *regex, const char *cmd, char ***field);

void
matchcmd_free(char ***field);

char *
rptchr_r(char c, const size_t num, char *buf);

int
validate(const int min, const int max, const char *name, const int val) __attribute__ ((pure));

double
validate_double(const double min, const double max, const char *name, const double val) __attribute__ ((pure));

int
getsysload(double *avg1, double *avg5, double *avg15);

void
getuptime(int *totaltime, int *idletime);

int
get_filesize_kb(const char *fname, unsigned *filesize);

int
get_diskspace(const char *dir, char *fs, char *size, char *used, char *avail, int *use) __attribute__ ((pure));

int
rotate_file(const char *fname, const unsigned max_size_kb);

int
set_cloexec_flag(const int fd, const _Bool flag);

int
getwsetsize(const int pid, int *size, char *unit, int *threads);

char *
url_encode(const char *str);

char *
url_decode(const char *str);

void
url_decode_inplace(char *str,const size_t maxlen);

int
url_decode_buff(char *decode, const size_t maxlen, const char *str);

char *
html_encode(const char *str);

char *
esc_percentsign(const char *str);

void
escape_quotes(char *tostr, const char *fromstr, const size_t maxlen, unsigned remove_n);

int
dump_string_chars(char *buffer, size_t maxlen, char const *str);


time_t
totimestamp(const int year, const int month, const int day,
            const int hour, const int min, const int sec) __attribute__ ((pure));
int
fromtimestamp(const time_t timestamp, int* year, int* month, int* day,
              int* hour, int* min, int* sec);

int
socket_read(const int sockd, void *buffer, const size_t buffLen);

int
convert_decgrad(const char *dec, char *grad) __attribute__ ((pure));

int
get_datetime(char *res,int sec);

int 
mtime(unsigned long *t);

int
_wbuff2file(const char *fname,const char *buff,const size_t buffsize);

void *
_chk_calloc_exit(size_t size);

char *
splitdatetime(char *datetime, char *buff);

// Clear variable section in memory
#define CLEAR(x) memset (&(x), 0, sizeof(x))

//-----------------------------------------------------------------------------
// Various defines for Regular expression matching of commands
// This is normally used with the matchcmd() function
//-----------------------------------------------------------------------------

/*
 * First a number of generic unicode regex defines
 */

// Required space(s)
#define _PR_S "[\\p{Z}]+"

// Optional space(s)
#define _PR_SO "[\\p{Z}]*"

// Required alphanumeric sequence
#define _PR_AN "([\\p{L}\\p{N}\\_\\-]+)"

// Required alphanumeric sequence for function call  
#define _PR_ANF "([\\p{L}]+)\\(([\\p{N}\\p{L}\\\"\\.\\+\\,]*)\\)"

// Required alphanumeric starting with a letter
#define _PR_AAN "([\\p{L}][\\p{L}\\p{N}\\_\\-]*)"

// Optional alphanumeric sequence
#define _PR_ANO "([\\p{L}\\p{N}\\_\\-]*)"

// Optional alphanumeric sequence including space, dash and dot
#define _PR_ANSO "([\\p{L}\\p{N}\\_\\- ]*)"

// Required alphanumeric sequence including space, dash and dot
#define _PR_ANS "([\\p{L}\\p{N}\\_\\-\\. ]+)"

// Required alpha sequence
#define _PR_A "([\\p{L}\\_]+)"

// Required numeric sequence
#define _PR_N "([\\p{N}]+)"

// Single numeric digit
#define _PR_N1 "([\\p{N}])"


// Optional numeric sequence
#define _PR_NO "([\\p{N}]*)"

// Optional numeric with dash and spaces sequence
#define _PR_NDS "([ \\p{N}]{1,3}[\\-]?[\\p{N} ]+)"

// Required file name with extension (including path)
#define _PR_FNAMEEXT "([\\p{L}\\p{N}\\_\\-\\/]+\\.[\\p{L}]{3,4})"

// Required file name  (including path)
#define _PR_FNAMEOEXT "([\\p{L}\\p{N}\\_\\-\\/\\.]+)"

// Required filepath
#define _PR_FILEPATH "([\\p{L}\\p{N}\\/\\.\\_\\-]+)"

// Required alphanumeric and punctuation sequence
#define _PR_ANP "([\\p{L}\\p{N}\\p{P}\\.\\>\\<\\+\\;\\:\\$\\,\\'\\`\\'\\-\\&\\#\\=]+)"

// Optional alphanumeric and punctuation sequence
#define _PR_ANPO "([\\p{L}\\p{N}\\p{P}\\.\\>\\<\\+\\;\\:\\$\\,\\'\\`\\'\\-\\&\\#\\=]*)"

// Required alphanumeric, punctuation and space sequence
#define _PR_ANPS "([\\p{L}\\p{N}\\p{P} \\.\\>\\<\\+\\;\\:\\$\\,\\'\\`\\'\\-\\&\\#\\=]+)"

// Optional alphanumeric, punctuation and space sequence
#define _PR_ANPSO "([\\p{L}\\p{N}\\p{P} \\.\\>\\<\\+\\;\\:\\$\\,\\'\\`\\'\\-\\&\\#\\=]*)"

// Any sequence of symbols
#define _PR_ANY "(\\X+)"

// End of string
#define _PR_E "$"

// Handy macro for debugging regex
#define _DBG_REGFLD(N,F) logmsg(LOG_DEBUG,"nf=%d, %s, %s, %s, %s, %s\n",N,F[0],N > 1 ? F[1] : "NULL",N >= 3 ? F[2]:"NULL",N >= 4 ? F[3]:"NULL",N >= 5 ? F[4]: "NULL")

/*
 * Symbolic names for entities in the command strings
 */

#define _PR_ARGL _PR_AN "(" _PR_SO "\\," _PR_SO _PR_AN ")*"

// 4 digit pin
#define _PR_ONOFF "(on|off)"

// 4 digit pin
#define _PR_PIN "([\\p{N}]{1,4})"

// Required full time (h:m)
#define _PR_TIME "([0-1][0-9]|2[0-3]):([0-5][0-9])"

// Optional time. Only hour required
#define _PR_OPTIME "([0-1][0-9]|2[0-3])(:([0-5][0-9]))?(:([0-5][0-9]))?"

// required full date with century
#define _PR_FULLDATE "(201[0-9])-(0[1-9]|1[0-2])-([0-2][0-9]|3[0-2])"

// optional full date
#define _PR_OPFULLDATE "((201[0-9])-(0[1-9]|1[0-2])-([0-2][0-9]|3[0-2]))?"

#define _PR_OPTOFROMDATE "(" _PR_FULLDATE _PR_SO "(" _PR_TIME  ")? " _PR_SO _PR_FULLDATE _PR_SO "(" _PR_TIME  ")?" ")?"

#define _PR_OPDEVID "(dev=" "(3[\\p{N}]{2,9})" _PR_SO ")?"
#define _PR_OPEVENTID "(ev=" _PR_N _PR_SO ")?"


#define _PR_VIDEO "([0-5])"
#define _PR_50_VAL "(-?[0-4]?[0-9]|-?50)" // [-50,50]
#define _PR_100_VAL "(100|[0-9]?[0-9])" // [0,100]

#define _PR_TITLE "(\\p{L}[\\p{L}\\p{N} _-]+)"
#define _PR_OPTITLE "(" _PR_S "(\\p{L}[\\p{L}\\p{N} _-]*))?"

#define TRUE 1
#define FALSE 0

#ifdef	__cplusplus
}
#endif

#endif	/* _UTILS_H */

