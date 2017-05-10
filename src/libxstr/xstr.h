/* =========================================================================
 * File:        XSTR.H
 * Description: Some extra string utility functions
 * Author:      Johan Persson (johan162@gmail.com)
 *
 * Copyright (C) 2009,2010,2011,2012 Johan Persson
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


#ifndef XSTR_H
#define	XSTR_H

#ifdef	__cplusplus
extern "C" {
#endif

#define MAX_SPLIT_FIELDS 64
#define MAX_FIELD_SIZE 256

/**
 * Data structure used with xstrsplitfields() used to split a string
 * into fields using the specified field delimiter
 */
struct splitfields {
        /** Number of split fields */
	size_t nf;
        /** Each field as a string */
	char fld[MAX_SPLIT_FIELDS][MAX_FIELD_SIZE];
};

size_t
xvstrncat(char *dst, size_t size, const char *format, ...)
    __attribute__ ((format (printf, 3, 4)));

void
xstrtrim(char *string);

void
xstrtrim_crnl(char *string);

size_t
xstrlcpy(char *dst, const char *src, size_t maxlen);

size_t
xstrlcat(char *dst, const char *src, size_t size);

int
xatoi(const char * const str) __attribute__ ((pure));

long
xatol(const char * const str) __attribute__ ((pure));

double
xatof(const char * const str)  __attribute__ ((pure));

int
xstricmp(const char *s1, const char *s2);

int
xstrtolower(char *s);

void
xstrfilify(char *s, char replace);

int
xsubstr(char *to, size_t maxlen, char *from, size_t s, size_t e);

int
xstrsplitfields(const char *buffer, char delim, struct splitfields *sfields);

int
xstrfext(const char *fileName, char *ext);

/*
 * Multibyte (UTF) safe string handling routines
 */

size_t
xmb_strlen(const char *s) __attribute__ ((pure));

size_t 
xmb_offset(const char *s, int charnum);

size_t
xmb_charnum(const char *s, size_t offset);

char *
xmb_strncpy(char *s1,const char *s2,size_t len);

size_t
xmb_strlcpy(char *s1,const char *s2,size_t maxbuff);

char *
xmb_strncat(char *dest, const char *src, size_t maxlen);

int
xmb_rpad(char *s, size_t pad, const size_t maxlen, const char padc);

int
xmb_lpad(char *s, size_t pad, const size_t maxlen, const char padc);

int
xmb_fprintf(FILE *fp,char *fmt, ...)
        __attribute__ ((format (printf, 2, 3)));

int
xmb_printf(char *fmt, ...)
        __attribute__ ((format (printf, 1, 2)));

int
xmb_snprintf(char *dest, size_t maxlen, char *fmt, ...)
        __attribute__ ((format (printf, 3, 4)));


#ifdef	__cplusplus
}
#endif

#endif	/* XSTR_H */

