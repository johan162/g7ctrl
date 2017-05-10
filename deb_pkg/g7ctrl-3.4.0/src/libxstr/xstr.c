/* =========================================================================
 * File:        XSTR.C
 * Description: Some extra string utility functions
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>
#include <wchar.h>
#include <locale.h>
#include <stdarg.h>

#include "xstr.h"

#define TRUE 1
#define FALSE 0

#ifdef __APPLE__

void *
mempcpy(void *to, const void *from, size_t size) {
    memcpy(to, from, size);
    return ((void *) ((char *) to + size));
}
#endif

/**
 * Copy the string src to dst, but no more than size - 1 bytes, and
 * null-terminate dst. NOTE that this is not UTF8 safe. Please see
 * xmbstrncpy() for a UTF8 safe version.
 *
 * This function is the same as BSD strlcpy().
 *
 * @param dst destination buffer
 * @param src source string
 * @param size copy at maximum this number of bytes
 * @return the length of src
 */
size_t
xstrlcpy(char *dst, const char *src, size_t size) {
    *dst = '\0';
    if (size == 0)
        return 0;
    const size_t size2 = strnlen(src,size)+1; // Must include terminating 0   
    if (size2 == 1)
        return 0;
    *((char *) mempcpy(dst, src, (size < size2 ? size : size2) - 1)) = '\0';
    return strnlen(dst, size);
}

/**
 * String concatenation with extra safety. Note that this is not
 * not UTF8 safe
 * @param dst Target string buffer
 * @param src Source string buffer
 * @param size Maximum new total size (in bytes)
 * @return final length of string
 */
size_t
xstrlcat(char *dst, const char *src, size_t size) {

    if (size == 0)
        return 0;

    if (strnlen(dst, size - 1) == size - 1)
        return size - 1;
    if (strlen(src) + strlen(dst) < size) {
        strncat(dst, src, size - 1 - strlen(dst));
        dst[size - 1] = '\0';
    }
    return strnlen(dst, size);
}

/**
 * Concat a string with a dynamically formatted string as snprintf()
 * @param dst Destination string buffer
 * @param size Maximum size for destination string
 * @param format Format string
 * @param ...
 * @return final length of string, (size_t)-1 in case of memory error
 */
size_t
xvstrncat(char *dst, size_t size, const char *format, ...) {
    const int blen = 1024*50;

    char *tmpbuff = calloc(1, blen);
    if (NULL == tmpbuff) {
        return (size_t)(-1);
    }
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
    int ret = xstrlcat(dst, tmpbuff, size);

    free(tmpbuff);
    va_end(ap);
    return ret;
}

/**
 * Trim a string in-place by removing beginning and ending spaces
 * @param str String to trim
 */
void
xstrtrim(char *str) {
    char *tmp = strdup(str), *startptr = tmp;
    int n = strlen(str);
    char *endptr = tmp + n - 1;

    while (*startptr == ' ') {
        startptr++;
    }

    while (n > 0 && *endptr == ' ') {
        --n;
        --endptr;
    }

    while (startptr <= endptr) {
        *str++ = *startptr++;
    }

    *str = '\0';

    free(tmp);
}

/**
 * Trim a string in-place by removing beginning and ending spaces
 * @param str String to trim
 */
void
xstrtrim_crnl(char *str) {
    char *tmp = strdup(str), *startptr = tmp;
    int n = strlen(str);
    char *endptr = tmp + n - 1;

    while (*startptr == ' ') {
        startptr++;
    }

    while (n > 0 && (*endptr == ' ' || *endptr == '\n' || *endptr == '\r')) {
        --n;
        --endptr;
    }

    while (startptr <= endptr) {
        *str++ = *startptr++;
    }

    *str = '\0';

    free(tmp);
}

/**
 * A safer version of atoi using strtol with error checking
 * @param str String to convert to integer
 * @return The value of str as integer. If the value is out of range
 *         the returned value is 0 and errno is set to the corresponding
 *         error.
 */
int
xatoi(const char * const str) {
    char *endptr;
    errno = 0;
    size_t len = strlen(str);
    _Bool isDigit = TRUE;
    for (size_t i = 0; i < len && isDigit; ++i) {
        isDigit = str[i] >= '0' && str[i] <= '9';
    }
    if (!isDigit) {
        errno = ERANGE;
        return 0;
    }
    long val = strtol(str, &endptr, 10);
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
        val = 0;
    }

    if (endptr == str) {
        val = 0;
    }

    return (int) val;
}

/**
 * A safer version of atol using strtol with error checking
 * @param str String to convert to long
 * @return The value of str as long. If the value is out of range
 *         the returned value is 0 and errno is set to the corresponding
 *         error.
 */
long
xatol(const char * const str) {
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
        val = 0;
    }

    if (endptr == str) {
        val = 0;
    }

    return val;
}

/**
 * A safer version of atof using strtod with error checking
 * @param str String to convert to double
 * @return The value of str as double. If the value is out of range
 *         the returned value is 0 and errno is set to the corresponding
 *         error.
 */
double
xatof(const char * const str) {
    char *endptr;
    errno = 0;
    double val = strtod(str, &endptr);
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
        val = 0;
    }

    if (endptr == str) {
        val = 0;
    }

    return val;
}

/**
 * Utility function. Convert string of maximum 4095 characters to lower case
 * in-place.
 * @param s String to transform
 * @return 0 on success, -1 if safetylimit is reached
 */
int
xstrtolower(char *s) {
    int safetylimit = 4096;
    while (--safetylimit > 0 && (*s = tolower(*s))) {
        s++;
    }
    if (safetylimit <= 0) {
        return -1;
    }
    return 0;
}

/**
 * Replace all non filename safe character in a string with the specified
 * character in-place.
 * @param str Input string
 * @param[in] replace Character to use as replacement for illegal file
 * name characters.
 */
void
xstrfilify(char *str, char replace) {
    const size_t n = strlen(str);
    for (unsigned i = 0; i < n; i++) {
        if (str[i] == ' ' || str[i] == '&' ||
                str[i] == ':' || str[i] == '!' ||
                str[i] == '#' || str[i] == '?' ||
                str[i] == '/' || str[i] == '\\' ||
                str[i] == '@' || str[i] == ',') {

            str[i] = replace;
        }
    }
}

/**
 * Cases insensitive string comparison
 * @param s1 First string to compare
 * @param s2 Second string to compare
 * @return 0 if equal, -1 if s1 &lt; s2 otherwise 1
 */
int
xstricmp(const char *s1, const char *s2) {
    const int safetylimit = 4096;
    int len1 = strnlen(s1, safetylimit);
    int len2 = strnlen(s2, safetylimit);
    if (len1 >= safetylimit || len2 >= safetylimit) {
        return -1;
    }
    char *b1 = strdup(s1);
    char *b2 = strdup(s2);
    if (b1 == NULL || b2 == NULL) {
        if (b1) free(b1);
        if (b2) free(b2);
        return -1;
    }
    xstrtolower(b1);
    xstrtolower(b2);
    int ret = strncmp(b1, b2, safetylimit);
    free(b1);
    free(b2);
    return ret;
}

/**
 * Extract substring to buffer
 * @param to     Buffer to write the extracted string to
 * @param maxlen Maximum length of buffer
 * @param from   String to extract from
 * @param s      Start position
 * @param e      End position
 * @return 0 on success, -1 on failure
 */
int
xsubstr(char *to, size_t maxlen, char *from, size_t s, size_t e) {
    *to = '\0';

    if ((size_t) - 1 == e)
        e = strlen(from) - 1;

    if (e < s)
        return -1;

    const size_t len = e - s + 1;
    if (len >= maxlen)
        return -1;

    size_t i = 0;
    while (i < len) {
        to[i++] = from[s++];
    }
    to[i] = '\0';
    return 0;
}


/********************************************************************
 * All _xmb_XXX functions are used for UTF8 (multibyte characters)
 ********************************************************************
 */

/**
 * MB (Multibyte) string length. Note this requires that
 * locale = setlocale(LC_ALL, "") is called in the code.
 * @param s Input string
 * @return Character length (usually different from byte length)
 */

size_t
xmb_strlen(const char *s) {
    mbstate_t t;
    const char *scopy = s;
    memset(&t, '\0', sizeof (t));
    size_t n = mbsrtowcs(NULL, &scopy, strlen(scopy), &t);

    return n;
}

// The start of a UTF8 sequence always have the top two bits set
// This macro tests if we are at the start of a UTF character
#define isutf(c) (((c)&0xc0)!=0x80)

/**
 * Find the byte offset in buffer for the charnum:th multibyte character
 * @param s Input multi-byte string to search
 * @param charnum The character unmber we need to convert to byte offset
 * @return Offset Byte offset for character "charnum"
 */
size_t
xmb_offset(const char *s, int charnum) {
    size_t offs = 0;
    while (charnum > 0 && s[offs]) {
        (void) (isutf(s[++offs]) || isutf(s[++offs]) || isutf(s[++offs]) || ++offs);
        charnum--;
    }
    return offs;
}

/**
 * Find the character (code-point) that corresponds to offset byte offset in buffer
 * @param s Input multi-byte string to search
 * @param offset The byte offset in the input string
 * @return The character number corresponding to the specified byte offset 
 */
size_t
xmb_charnum(const char *s, size_t offset) {
    size_t charnum = 0, offs = 0;
    while (offs < offset && s[offs]) {
        (void) (isutf(s[++offs]) || isutf(s[++offs]) || isutf(s[++offs]) || ++offs);
        charnum++;
    }
    return charnum;
}

/**
 * Increase byte index supplied as the second in string to next mb code-point
 * @param s Input multi-byte string
 * @param i Current byte index
 */
void
xmb_inc(const char *s, size_t *i) {
    (void) (isutf(s[++(*i)]) || isutf(s[++(*i)]) || isutf(s[++(*i)]) || ++(*i));
}

/**
 * Decrease byte index supplied as the second argument to previous mb code-point
 * @param s Input multi-byte string
 * @param i Current byte index
 */
void
xmb_dec(const char *s, size_t *i) {
    (void) (isutf(s[--(*i)]) || isutf(s[--(*i)]) || isutf(s[--(*i)]) || --(*i));
}

/**
 * Multibyte safe of strncpy. Copies at most the number of
 * multibyte characters that can safely fit within (maxbuff-1) bytes.
 * The string s1 is always terminated by 0 after a complete mb character.
 * @param s1 Destination
 * @param s2 Source
 * @param maxbuff Maximum buffer size of s1 (includes the terminating 0)
 * @return s1 The destination string
 */
char *
xmb_strncpy(char *s1, const char *s2, size_t maxbuff) {
    // Find the place to put the terminating 0 just
    // after the unicode character that fits into the
    // maxbuff-1 size. 
    const int charnum = xmb_charnum(s2, maxbuff);
    size_t byteidx = xmb_offset(s2, charnum);
    if (byteidx >= maxbuff)
        xmb_dec(s2, &byteidx);
    *((char *) mempcpy(s1, s2, byteidx)) = '\0';
    return s1;
}

/**
 * Multibyte safe string concatenation. Concatenate the source to the destination
 * string. The final length of the copied bytes including terminating 0 is always
 * less or equal to the maxlen parameter.
 * @param dest Destination string
 * @param src Source string
 * @param maxlen The total maximum length (in bytes) for the destination string.
 * If the maxlen would cause a multibyte character to be split the ending will be
 * adjusted so that the previous full multibyte character will be included.
 * @return dest string
 */
char *
xmb_strncat(char *dest, const char *src, size_t maxlen) {
    const size_t dlen = strlen(dest); // Find the terminating '\0'
    if (maxlen == 0 || dlen >= maxlen - 1)
        return dest;
    return xmb_strncpy(dest + dlen, src, maxlen - dlen - 1);
}

/**
 * Copy string s2 to s1 and NULL teminate s1. The maximum size of s1 (inclusing '\0'
 * termination is specified as the maxbuff argument). This is multibyte character
 * safe
 * @param s1 Destination
 * @param s2 Source
 * @param maxbuff Maximum byte size of destintaion
 * @return The length of s1 (in characters) after copying.
 */
size_t
xmb_strlcpy(char *s1, const char *s2, size_t maxbuff) {
    char *s = xmb_strncpy(s1, s2, maxbuff);
    return xmb_strlen(s);
}

/**
 * MB (Multibyte) String right padding to length
 * @param s Input string
 * @param pad The new total string length (including padding)
 * @param maxlen The maximum length allowed for the string
 * @param padc Character to use for padding
 * @return 0 on success, -1 on failure
 */
int
xmb_rpad(char *s, size_t pad, const size_t maxlen, const char padc) {
    size_t mbn = xmb_strlen(s);
    size_t n = strlen(s);
    if ((size_t) - 1 == n) {
        return -1;
    }
    if ((size_t) - 1 == mbn) {
        return -1;
    }
    if (n + pad >= maxlen || mbn > pad) {
        return -1;
    }
    for (size_t i = 0; i < pad - mbn; ++i) {
        s[n + i] = padc;
    }
    s[n + pad - mbn] = '\0';
    return 0;
}

/**
 * MB (Multibyte) String left padding to length
 * @param s Input string
 * @param pad The new total string length (including padding)
 * @param maxlen The maximum length allowed for the string
 * @param padc Character to use for padding
 * @return 0 on success, -1 on failure
 */
int
xmb_lpad(char *s, size_t pad, const size_t maxlen, const char padc) {
    size_t mbn = xmb_strlen(s);
    size_t n = strlen(s);
    if ((size_t) - 1 == n) {
        return -1;
    }
    if ((size_t) - 1 == mbn) {
        return -1;
    }
    if (n + pad >= maxlen || mbn > pad) {
        return -1;
    }

    s[pad] = '\0';
    for (size_t i = 0; i < n; ++i) {
        s[pad - mbn + i] = s[i];
    }
    for (size_t i = 0; i < pad - mbn; ++i) {
        s[i] = padc;
    }
    return 0;
}

// Size of the internal buffer needed for XMB_printf() family of functions
#define MAX_MBPRINTFBUFF 20000

/**
 * Multibyte safe version of printf()
 * @param fmt Format string
 * @param ... Variable argument list
 * @return -1 on conversion failure standard printf() return value otherwise
 */
int
xmb_printf(char *fmt, ...) {
    wchar_t *wbuff = calloc(MAX_MBPRINTFBUFF, sizeof (wchar_t));
    va_list ap;
    va_start(ap, fmt);

    const size_t wflen = mbstowcs(NULL, fmt, 0) + 1;
    wchar_t wf[wflen];
    mbstowcs(wf, fmt, wflen);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"

    if (MAX_MBPRINTFBUFF == vswprintf(wbuff, MAX_MBPRINTFBUFF, wf, ap)) {
        va_end(ap);
        free(wbuff);
        return -1;
    }

#pragma clang diagnostic pop
#pragma GCC diagnostic pop

    va_end(ap);
    const int rc = printf("%ls", wbuff);
    free(wbuff);
    return rc;
}

/**
 * Multibyte safe version of fprintf()
 * @param fp File pointer
 * @param fmt Format string
 * @param ... Variable argument list
 * @return -1 on conversion failure standard fprintf() return value otherwise
 */
int
xmb_fprintf(FILE *fp, char *fmt, ...) {
    wchar_t *wbuff = calloc(MAX_MBPRINTFBUFF, sizeof (wchar_t));
    va_list ap;
    va_start(ap, fmt);

    const size_t wflen = mbstowcs(NULL, fmt, 0) + 1;
    wchar_t wf[wflen];
    mbstowcs(wf, fmt, wflen);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"

    if (MAX_MBPRINTFBUFF == vswprintf(wbuff, MAX_MBPRINTFBUFF, wf, ap)) {
        va_end(ap);
        free(wbuff);
        return -1;
    }

#pragma clang diagnostic pop
#pragma GCC diagnostic pop

    va_end(ap);
    const int rc = fprintf(fp, "%ls", wbuff);
    free(wbuff);
    return rc;

}

/**
 * Multibyte safe version of snprintf()
 * @param dest Destination string
 * @param maxlen Maximum length of destination string
 * @param fmt Format string
 * @param ... Variable argument list
 * @return -1 on conversion failure standard snprintf() return value otherwise
 */
int
xmb_snprintf(char *dest, size_t maxlen, char *fmt, ...) {
    wchar_t *wbuff = calloc(MAX_MBPRINTFBUFF, sizeof (wchar_t));
    va_list ap;
    va_start(ap, fmt);

    const size_t wflen = mbstowcs(NULL, fmt, 0) + 1;
    wchar_t wf[wflen];
    mbstowcs(wf, fmt, wflen);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"

    if (MAX_MBPRINTFBUFF == vswprintf(wbuff, MAX_MBPRINTFBUFF, wf, ap)) {
        va_end(ap);
        free(wbuff);
        return -1;
    }

#pragma clang diagnostic pop
#pragma GCC diagnostic pop

    va_end(ap);

    // Make sure the destination buffer is large enough
    if (maxlen < wcstombs(NULL, wbuff, 0) + 1) {
        *dest = '\0';
        free(wbuff);
        return -1;
    }

    const int rc = snprintf(dest, maxlen, "%ls", wbuff);
    free(wbuff);
    return rc;

}

/**
 * Split a string with the given delimiter into fields
 * @param buffer input string to split
 * @param delim delimiter to use to split fields
 * @param sfields return structure
 * @return -1 on failure, 0 on on success
 */
int
xstrsplitfields(const char *buffer, char delim, struct splitfields *sfields) {

    sfields->nf = 0;
    size_t len = strlen(buffer);
    if (len >= 1024 || 0 == len) return -1;

    const char *bptr = buffer;
    char *ptr = NULL;
    while (len > 0) {
        ptr = sfields->fld[sfields->nf];
        size_t flen = MAX_FIELD_SIZE;
        while (*bptr != delim && len > 0 && flen > 0) {
            //if (' ' == *bptr) bptr++;
            //else 
            *ptr++ = *bptr++;
            --len;
            --flen;
        }

        if (flen <= 0)
            return -1;

        if (*bptr == delim) {
            *ptr = '\0';
            bptr++;
            --len;
        }
        ++sfields->nf;

        if (sfields->nf >= MAX_SPLIT_FIELDS)
            return -1;
    }
    *ptr = '\0';
    // Special case. If the last character in the string is the delim
    // it means that len will be zero and the (empty) last field will not be accounted
    // for so take care of this corner case manually
    if (delim == buffer[strlen(buffer) - 1]) {
        sfields->fld[sfields->nf][0] = '\0';
        ++sfields->nf;
    }
    return 0;
}

/**
 * Check a string for a fileName extension.
 * @param[in] fileName The filename to check
 * @param[out] ext If <> NULL then the found extension is written to this buffer
 * @return 0 if the filename has an extension (after the '.') which is 3 or 4
 * characters long, -1 otherwise
 */
int
xstrfext(const char *fileName, char *ext) {
    const size_t n = strlen(fileName);
    size_t i = 0;
    _Bool foundDot = FALSE;
    size_t extLen = 0;
    if (ext != NULL) {
        *ext = '\0';
    }
    while (i < n) {
        if ('.' == fileName[i]) {
            if (foundDot)
                return -1;
            else
                foundDot = TRUE;
        } else {
            if (foundDot) {
                if (ext != NULL) {
                    *ext++ = fileName[i];
                }
                extLen++;
                if (extLen > 4)
                    return -1;
            }
        }
        i++;
    }
    if (ext != NULL)
        *ext = '\0';
    return extLen > 2 ? 0 : -1;
}


/* EOF */
