/* =========================================================================
 * File:        ASSOCARRAY.C
 * Description: A basic associate array of strings that grows dynamically
 *              with the usual manipulation functions
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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "assocarray.h"
#include "xstr.h"

/**
 * Create and return a new associative array
 * @param initial_size Initial size of array
 * @return A pointer to a new associative array, NULL on error
 */
assoc_array_t
assoc_new(size_t initial_size) {
    assoc_array_t pa = calloc(1, sizeof (struct assoc_array));
    if (pa == NULL || initial_size < 8) {
        return NULL;
    }
    pa->a = calloc(initial_size, sizeof (struct assoc_pair));
    if (pa->a == NULL) {
        free(pa);
        return NULL;
    }
    pa->max_values = initial_size;
    return pa;
}

/**
 * Get the value corresponding to the given name
 * @param a Associative array as returned by assoc_new()
 * @param name The name of the value
 * @return A pointer to the value, NULL of not found
 */
char *
assoc_get(assoc_array_t a, char *name) {
    if (a == NULL || name == NULL || *name == '\0' || a->num_values == 0)
        return NULL;

    size_t i = 0;
    while (i < a->num_values && strcmp(name, a->a[i].name)) {
        i++;
    }
    if (i >= a->num_values)
        return NULL;
    else
        return a->a[i].value;
}

/**
 * Return a value given a name and fallback to the value in "notfound"
 * if the named pair does not exists rather tha returning NULL
 * @param a Associative array as returned by assoc_new()
 * @param name The name of the value
 * @param notfound String to return if name is not found
 * @return Value of string in "Notfound"
 */
char *
assoc_get2(assoc_array_t a, char *name, char *notfound) {
    char *ret = assoc_get(a, name);
    if (NULL == ret)
        return notfound;
    else
        return ret;
}

/**
 * Deep copy of source to destination array. Existing elements in dest will be removed
 * @param dest Destination
 * @param src Source
 * @return 0 on success, -1 if the destination array does not have enough space
 */
int
assoc_copy(assoc_array_t dest, assoc_array_t src) {
    assoc_clear(dest);
    return assoc_add(dest, src);
}

/**
 * Add the values in source to existing values in destination. All existing
 * values in destination will be preserved
 * @param dest Destination array
 * @param src Source array
 * @return 0 on success, -1 on failure (indicates out of memory error)
 */
int
assoc_add(assoc_array_t dest, assoc_array_t src) {
    for (size_t i = 0; i < src->num_values; i++) {
        if (-1 == assoc_put(dest, src->a[i].name, src->a[i].value))
            return -1;
    }
    return 0;
}

/**
 * Create the intersection of two array and store in destinateion. The
 * intersection consists of all values existing in both arrays
 * @param dest Destination
 * @param a1 First input array
 * @param a2 Second input array
 * @return 0 on success, -1 on failure
 */
int
assoc_intersection(assoc_array_t dest, assoc_array_t a1, assoc_array_t a2) {
    assoc_clear(dest);
    // Loop over the shorter array
    assoc_array_t tmp1 = a1->num_values < a2->num_values ? a1 : a2;
    assoc_array_t tmp2 = a1->num_values < a2->num_values ? a2 : a1;
    for (size_t i = 0; i < tmp1->num_values; i++) {
        if (assoc_get(tmp2, tmp1->a[i].name)) {
            if (-1 == assoc_put(dest, tmp1->a[i].name, tmp1->a[i].value))
                return -1;
        }
    }
    return 0;
}

/**
 * Add a new name/value pair to the array
 * @param a Associative array as returned by assoc_new()
 * @param name Name
 * @param value String
 * @return 0 on success, -1 on failure, -2 if name exists
 */
int
assoc_put(assoc_array_t a, char *name, char *value) {
    if (a == NULL || name == NULL || value == NULL || *name == '\0')
        return -1;

    // Check that the name doesn't exist before
    if (assoc_get(a, name)) {
        return -2;
    }

    if (a->num_values >= a->max_values) {
        // We need to dynamically increase the size
        // Keep it simple and just double the size each time we hit the limit
        assoc_array_t tmp = assoc_new(a->max_values * 2);
        if (NULL == tmp)
            return -1;
        assoc_copy(tmp, a);
        for (size_t i = 0; i < a->num_values; i++) {
            free(a->a[i].name);
            free(a->a[i].value);
        }
        free(a->a);
        a->a = tmp->a;
        a->max_values = tmp->max_values;
        free(tmp);
    }
    a->a[a->num_values].name = strdup(name);
    a->a[a->num_values].value = strdup(value);
    if (a->a[a->num_values].name == NULL || a->a[a->num_values].value == NULL) {
        return -1;
    }
    a->num_values++;
    return 0;
}

/**
 * Update an existing name with new value
 * @param a Associative array as returned by assoc_new()
 * @param name Existing name
 * @param value New value
 * @return 0 on success, -1 if value does not exists
 */
int
assoc_update(assoc_array_t a, char *name, char *value) {
    if (NULL == assoc_get(a, name)) {
        return -1;
    }
    size_t i = 0;
    while (i < a->num_values && strcmp(a->a[i].name, name)) {
        i++;
    }
    free(a->a[i].value);
    a->a[i].value = strdup(value);
    return 0;
}

/**
 * Delete the named value
 * @param a Associative array as returned by assoc_new()
 * @param name Name
 * @return 0 on success, -1 on failure. -2 if name doesn't exists
 */
int
assoc_del(assoc_array_t a, char *name) {
    if (a == NULL || name == NULL || *name == '\0')
        return -1;

    // Check that the name exists before
    if (a->num_values == 0 || NULL == assoc_get(a, name)) {
        return -2;
    }

    size_t i = 0;
    while (i < a->num_values && strcmp(a->a[i].name, name)) {
        i++;
    }
    if (i >= a->num_values) {
        return -1;
    }
    free(a->a[i].name);
    free(a->a[i].value);
    // Copy down all values
    for (size_t j = i; j < a->num_values - 1; j++) {
        a->a[j].name = a->a[j + 1].name;
        a->a[j].value = a->a[j + 1].value;
    }
    a->a[a->num_values - 1].name = NULL;
    a->a[a->num_values - 1].value = NULL;
    a->num_values--;
    return 0;
}

/**
 * Return the current length of the associative array
 * @param a Associative array as returned by assoc_new()
 * @return The number of values in the array
 */
size_t
assoc_len(assoc_array_t a) {
    return a->num_values;
}

/**
 * Return the maximum size of the array
 * @param a Associative array as returned by assoc_new()
 * @return The current maximum size of array
 */
size_t
assoc_size(assoc_array_t a) {
    return a->max_values;
}

/**
 * Destroy the array (all memory is freed)
 * @param a
 * @return 0 on success, -1 on failure
 */
int
assoc_destroy(assoc_array_t a) {
    if (a == NULL)
        return -1;
    if (a->a) {
        for (size_t i = 0; i < a->num_values; i++) {
            if (a->a[i].name) free(a->a[i].name);
            if (a->a[i].value)free(a->a[i].value);
        }
        free(a->a);
    }
    free(a);
    return 0;
}

/**
 * Clear all values and return the memory occupied by the values.
 * This differs from assoc_destroy() in that the array itself will not
 * be freed. Only the values in the array.
 * @param a a Associative array as returned by assoc_new()
 * @return 0 on success, -1 on failure
 */
int
assoc_clear(assoc_array_t a) {
    if (a == NULL)
        return -1;
    if (a->a) {
        for (size_t i = 0; i < a->num_values; i++) {
            if (a->a[i].name) free(a->a[i].name);
            if (a->a[i].value)free(a->a[i].value);
        }
    }
    a->num_values = 0;
    return 0;
}

static int
qsort_compare(const void *e1, const void *e2) {
    return strcmp(((assoc_pair_t) e1)->name, ((assoc_pair_t) e2)->name);
}

/**
 * Sort the array by name
 * @param a Associative array as returned by assoc_new()
 * @return 0 on success, -1 on failure
 */
int
assoc_sort(assoc_array_t a) {
    if (a == NULL || a->num_values == 0)
        return -1;
    qsort(a->a, a->num_values, sizeof (struct assoc_pair), qsort_compare);
    return 0;
}

#define EXPORT_ID_NAME "_id"
#define EXPORT_ID_NAME_VAL "assoc_array"
#define EXPORT_PAIRS_KEY "pairs"

/**
 * Dump the array in a human readable format to the given string buffer
 * @param a Associative array as returned by assoc_new()
 * @param buff Output buffer
 * @param maxlen Maximum length
 * @return 0 on success, -1 on error
 */
int
assoc_export_to_json(assoc_array_t a, char *buff, size_t maxlen) {
    if (a == NULL)
        return -1;

    snprintf(buff, maxlen, "{\n  \"%s\":\"%s\",\"num_values\":%zu,\"max_values\":%zu,\n\"%s\":[\n",
            EXPORT_ID_NAME, EXPORT_ID_NAME_VAL, a->num_values, a->max_values, EXPORT_PAIRS_KEY);
    if (a->num_values > 0) {
        for (size_t i = 0; i < a->num_values - 1; i++) {
            xvstrncat(buff, maxlen, "    { \"%s\" : \"%s\" },\n", a->a[i].name, a->a[i].value);
        }
        xvstrncat(buff, maxlen, "    { \"%s\" : \"%s\" }\n", a->a[a->num_values - 1].name, a->a[a->num_values - 1].value);
    }
    xvstrncat(buff, maxlen, "  ]\n}\n");
    return 0;
}

/**
 * Skip white space in parse buffer
 * @param cptr Parse buffer pointer
 */
static void
_parse_ss(char **cptr) {
    while (**cptr && (**cptr == ' ' || **cptr == '\t' || **cptr == '\n'))
        (*cptr)++;
}

/**
 * Read the next string from the parse buffer. A string is enclosed in '"'
 * @param cptr
 * @param val
 * @param maxlen
 * @return 0 on success, -1 on failure
 */
static int
_parse_str(char **cptr, char *val, size_t maxlen) {
    _parse_ss(cptr);
    *val = '\0';
    if (cptr == NULL || **cptr == '\0' || **cptr != '"')
        return -1;
    (*cptr)++;
    while (maxlen > 1 && **cptr) {
        if (**cptr == '\\' && *((*cptr) + 1) == '"') {
            if (maxlen < 3) {
                return -1;
            }
            *val++ = **cptr;
            (*cptr)++;
            *val++ = **cptr;
            (*cptr)++;
            maxlen -= 2;
        } else if (**cptr == '"') {
            (*cptr)++;
            break;
        } else {
            *val++ = **cptr;
            (*cptr)++;
            maxlen--;
        }
    }

    *val = '\0';
    return 0;
}

/**
 * Parse the next number
 * @param cptr Parse buffer pointer
 * @param val The number read
 * @param maxlen Maximum length of buffer
 * @return 0 on success, -1 on failure
 */
static int
_parse_number(char **cptr, char *val, size_t maxlen) {
    _parse_ss(cptr);
    *val = '\0';
    if (cptr == NULL || **cptr == '\0' || **cptr < '0' || **cptr > '9')
        return -1;
    while (maxlen > 1 && **cptr && ((**cptr >= '0' && **cptr <= '9') || **cptr == '.')) {
        *val++ = **cptr;
        (*cptr)++;
        maxlen--;
    }
    *val = '\0';
    return 0;
}

/**
 * Check and advance pointer if chracter is the expected
 * @param cptr Parse buffer pointer
 * @param c
 * @return TRUE on match, false otherwise
 */
static _Bool
_parse_chk(char **cptr, char c) {
    _parse_ss(cptr);
    if (**cptr == c) {
        (*cptr)++;
        return 1;
    } else {
        return 0;
    }
}

/**
 * Return the net non-white space character and advance the
 * parse buffer pointer to that character
 * @param cptr Parse buffer pointer
 * @param c
 * @return TRUE on match, false otherwise
 */
static char
_parse_peek(char **cptr) {
    _parse_ss(cptr);
    return **cptr;
}

/**
 * Parse a JSON (name:number) pair
 * @param cptr Parse buffer pointer
 * @param name
 * @param val
 * @param maxlen
 * @return 0 on success, -1 on failure
 */
static int
_parse_pair_number(char **cptr, char *name, char *val, size_t maxlen) {
    if (0 != _parse_str(cptr, name, maxlen))
        return -1;
    if (!_parse_chk(cptr, ':') || 0 != _parse_number(cptr, val, maxlen))
        return -1;
    return 0;
}

/**
 * Parse a JSON (name:string) pair
 * @param cptr Parse buffer pointer
 * @param name
 * @param val
 * @param maxlen
 * @return 0 on success, -1 on failure
 */
static int
_parse_pair_str(char **cptr, char *name, char *val, size_t maxlen) {
    if (0 != _parse_str(cptr, name, maxlen))
        return -1;
    if (!_parse_chk(cptr, ':') || 0 != _parse_str(cptr, val, maxlen))
        return -1;
    return 0;
}

/**
 * Parse a JSON (name:object) pair where object is either a number or string
 * @param cptr Parse buffer pointer
 * @param name
 * @param val
 * @param maxlen
 * @return 0 on success, -1 on failure
 */
static int
_parse_pair(char **cptr, char *name, char *val, size_t maxlen) {
    if (0 != _parse_str(cptr, name, maxlen))
        return -1;
    if (!_parse_chk(cptr, ':'))
        return -1;
    int rc;
    if (_parse_peek(cptr) == '"') {
        rc = _parse_str(cptr, val, maxlen);
    } else {
        rc = _parse_number(cptr, val, maxlen);
    }
    return rc;
}

/**
 * Parse a vector of JSON string or number objects
 * @param cptr Parse buffer pointer
 * @param a Associative array as returned by assoc_new()
 * @return 0 on success, -1 on failure
 */
static int
_parse_vector(char **cptr, assoc_array_t a) {
    if (!_parse_chk(cptr, '['))
        return -1;

    int rc = 0;
    char buf_name[64];
    char buf_val[64];
    while (**cptr) {

        if (!_parse_chk(cptr, '{')) {
            rc = -1;
            break;
        }

        if (-1 == _parse_pair(cptr, buf_name, buf_val, sizeof (buf_val))) {
            rc = -1;
            break;
        }

        if (!_parse_chk(cptr, '}')) {
            rc = -1;
            break;
        }

        assoc_put(a, buf_name, buf_val);

        if (!_parse_chk(cptr, ',')) {
            break;
        }
    }

    if (-1 == rc || !_parse_chk(cptr, ']'))
        return -1;

    return 0;
}

/**
 * Import an exported array as eported by assoc_export_json()
 * @param a Associative array as returned by assoc_new()
 * @param buf Zero terminated import buffer
 * @return 0 on success, -1 on error
 */
int
assoc_import_from_json(assoc_array_t a, char *buf) {
    char *cptr = buf;
    if (_parse_chk(&cptr, '{')) {

        // Read _id
        size_t bufsize = 64;
        char id_name[bufsize], id_value[bufsize];
        if (_parse_pair_str(&cptr, id_name, id_value, bufsize))
            return -1;
        if (strcmp(id_name, EXPORT_ID_NAME) || strcmp(id_value, EXPORT_ID_NAME_VAL))
            return -1;
        if (!_parse_chk(&cptr, ','))
            return -1;

        // Read num_values
        char num_values_name[bufsize], num_values[bufsize];
        if (_parse_pair_number(&cptr, num_values_name, num_values, bufsize))
            return -1;
        if (!_parse_chk(&cptr, ','))
            return -1;

        // Read max values
        char num_mvalues_name[bufsize], num_mvalues[bufsize];
        if (_parse_pair_number(&cptr, num_mvalues_name, num_mvalues, bufsize))
            return -1;
        if (!_parse_chk(&cptr, ','))
            return -1;

        // Check that this is the "pairs" key
        char pairs_key[64];
        if (0 != _parse_str(&cptr, pairs_key, sizeof (pairs_key)) || strcmp(EXPORT_PAIRS_KEY, pairs_key))
            return -1;
        if (!_parse_chk(&cptr, ':'))
            return -1;

        _parse_vector(&cptr, a);

        if (_parse_chk(&cptr, '}'))
            return 0;
        else
            return -1;

    } else {
        return -1;
    }

}

/**
 * Import the associative array from the named file
 * @param a Associative array as returned by assoc_new()
 * @param filename File to import from in JSON export format
 * @return 0 on success, -1 on failure
 */
int
assoc_import_from_json_file(assoc_array_t a, char *filename) {
    int fd = open(filename, O_RDONLY);

    if (fd < 0) {
        return -1;
    }

    struct stat stat;
    if (-1 == fstat(fd, &stat)) {
        close(fd);
        return -1;
    }

    char *buffer = calloc(stat.st_size + 1, sizeof (char));
    if (NULL == buffer) {
        close(fd);
        return -1;
    }

    int len = read(fd, buffer, stat.st_size);
    close(fd);
    if (len < 0 || len != stat.st_size) {
        free(buffer);
        return -1;
    }
    *(buffer + stat.st_size) = '\0';

    int rc = assoc_import_from_json(a, buffer);

    free(buffer);

    return rc;

}
