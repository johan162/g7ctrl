/* =========================================================================
 * File:        FUTILS.H
 * Description: A collection of file and directory utility functions.
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: futils.h 908 2015-04-03 23:24:03Z ljp $
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

#ifndef FUTILS_H
#define	FUTILS_H

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * Callback function prototype used with process_files()
*/
typedef int (*pfi_cu_callback)(char *,size_t);

int
process_files(const char *dirbuff, char *suffix, size_t maxfiles,
              size_t *numfiles, pfi_cu_callback callback);

int
removedir(const char *dir);

int
mv_and_rename(char *from, char *to, char *newname, size_t maxlen);


int
chkcreatedir(const char *basedir,char *dir);

int
strip_filesuffix(char *filename,char *suffix, int slen);

int
tail_logfile(unsigned n, char *buffer, size_t maxlen);

int
read_file_buffer(char *name, size_t *readlen, char **buffer);


#ifdef	__cplusplus
}
#endif

#endif	/* FUTILS_H */

