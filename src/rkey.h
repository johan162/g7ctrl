/* =========================================================================
 * File:        RKEY.H
 * Description: Functions to do key/val pair substitution in buffers
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: rkey.h 623 2015-01-08 07:25:36Z ljp $
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

#define MAX_KEYPAIR_VAL_SIZE 1024

/**
 * Holds each keypair to be replaced in the buffer/file
 */
struct keypairs {
    char *key;
    char *val;
};

int
replace_keywords(char *buffer, size_t maxlen, struct keypairs keys[], size_t nkeys);

int
replace_keywords_in_file(char *filename,char **buffer, struct keypairs keys[], size_t nkeys);

struct keypairs *
new_keypairlist(size_t maxsize);

int
add_keypair(struct keypairs *keys, size_t maxkeys, char *key, char *val, size_t *idx);

int
free_keypairlist(struct keypairs *keys, size_t maxkeys);


#ifdef	__cplusplus
}
#endif

#endif	/* RKEY_H */

