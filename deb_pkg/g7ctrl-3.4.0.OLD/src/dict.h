/* =========================================================================
 * File:        DICT.H
 * Description: Functions to do dictionary substitution in buffers
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


#ifndef RKEY_H
#define	RKEY_H

#ifdef	__cplusplus
extern "C" {
#endif

/* Maximum size for values. We need this to be able to estimate the maximum buffer size 
 * needed when doing replacements in a buffer.
 */
#define MAX_DICTIONARY_VAL_SIZE 1024
    
/* The dictionary will grow dynamically as needed but this is the initial size
 */
#define DICTIONARY_INIITIAL_SIZE 100

/**
 * Holds each tuple in the dictionary 
 */
struct dict_tuple {
    char *key;
    char *val;
};

/**
 * Data structure for dictionary
 */
typedef struct dict {
    size_t idx;
    size_t maxsize;
    struct dict_tuple *tuple;
} *dict_t;


/* Function pointer for map_dict() function
 */
typedef int (dict_map_t)(char *,char *);

/* API definitions for dictionary
 */

dict_t
new_dict(void);

int
replace_dict_in_buf(dict_t dict, char *buffer, size_t maxlen);

int
replace_dict_in_file(dict_t dict, char *filename, char **buffer);

int
add_dict(dict_t dict, char *key, char *val);

int
free_dict(dict_t dict);

int
map_dict(dict_t dict, dict_map_t f);

char *
getval_dict(dict_t d, char *key);


#ifdef	__cplusplus
}
#endif

#endif	/* RKEY_H */

