/* =========================================================================
 * File:        FUTILS.C
 * Description: A collection of file and directory utility functions
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
#include <errno.h>
#include <sys/param.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "g7ctrl.h"
#include "g7config.h"
#include "utils.h"
#include "libxstr/xstr.h"
#include "logger.h"
#include "futils.h"

/*
 * Remove specified directory and all files and directories under it
 * It behaves similar to "rm -rf dir"
 * @param dir Directory to remove
 * @return 0 on success, -1 on failure
 */
int
removedir(const char *dir) {
    struct dirent *dirp;
    struct stat statbuf;
    char tmpbuff[512];

    if (dir == NULL) {
        return -1;
    }

    DIR *dp = opendir(dir);
    if (dp == NULL) {
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL) {
        if ((strcmp(dirp->d_name, ".") != 0) && (strcmp(dirp->d_name, "..") != 0)) {
            snprintf(tmpbuff, 512, "%s/%s", dir, dirp->d_name);
            if (-1 == lstat(tmpbuff, &statbuf)) {
                logmsg(LOG_ERR, "lstat() faile for file '%s'. (%d : %s)", tmpbuff, errno, strerror(errno));
                (void) closedir(dp);
                return -1;
            }

            if (S_ISDIR(statbuf.st_mode)) {
                if (removedir(tmpbuff) < 0) {
                    (void) closedir(dp);
                    return -1;
                }
            } else if (S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
                if (remove(tmpbuff)) {
                    (void) closedir(dp);
                    return -1;
                }
            } else {
                (void) closedir(dp);
                return -1;
            }
        }
    }
    int ret = rmdir(dir);
    (void) closedir(dp);
    return ret;
}

/**
 * Move file from "from" to "to" if the target file already
 * exists then try to rename the file by adding a "_nn" suffix
 * where nn is a two digit number. The file will keep the same
 * file extension.
 * The "newname" buffer is set to the name of the file copied
 * to which might have changed. If newname==NULL the argument will 
 * be ignored.
 * @param from      original name
 * @param to        new name
 * @param newname   (out) the calculated new name in case "to"-file exists. Ignored if NULL
 * @param maxlen    maximum buffer length for newname
 * @return 0 on success, -1 on fail
 */
int
mv_and_rename(char *from, char *to, char *newname, size_t maxlen) {
    struct stat filestat;
    char buff1[256], short_filename[128], to_directory[256], suffix[8];

    if (-1 == stat(from, &filestat)) {
        logmsg(LOG_ERR, "FATAL: Cannot move and rename file '%s'. (%d : %s)", from, errno, strerror(errno));
        return -1;
    }

    if (0 == stat(to, &filestat)) {
        // File exists. Try renaming it and see if that works
        strncpy(buff1, to, 255);
        strncpy(short_filename, basename(buff1), 127);
        short_filename[127] = '\0';
        strncpy(to_directory, dirname(buff1), 255);
        to_directory[255] = '\0';
        int k = strlen(short_filename) - 1;
        while (k > 0 && short_filename[k] != '.') {
            k--;
        }
        if (short_filename[k] == '.') {
            if (strlen(&short_filename[k]) > 7) {
                // A suffix > 7 characters, don't think so!
                //logmsg(LOG_ERR,"FATAL: Cannot move and rename file '%s' to '%s'. Invalid file suffix '%s'",
                //      from,to,&short_filename[k]);
                return -1;
            }
            strncpy(suffix, &short_filename[k], 7);
            suffix[7] = '\0';
            short_filename[k] = '\0';
        } else {
            //logmsg(LOG_ERR,"FATAL: Cannot move and rename file '%s' to '%s' destination file must have a valid suffix.",from,to);
            return -1;
        }

        int i = 0, ret;
        do {
            i++;
            snprintf(buff1, 255, "%s/%s_%03d%s", to_directory, short_filename, i, suffix);
            ret = stat(buff1, &filestat);
        } while (i < 999 && ret == 0);
        if (i >= 999) {
            logmsg(LOG_ERR, "FATAL: Cannot move and rename file '%s' to '%s'. Too many duplicates.", from, to);
            return -1;
        }
    } else if (errno == ENOENT) {
        // File doesn't exist
        strncpy(buff1, to, 255);
        buff1[255] = '\0';
    } else {
        // Some other problems
        logmsg(LOG_ERR, "FATAL: Cannot move and rename file '%s' to '%s' (%d : %s)", from, to, errno, strerror(errno));
        return -1;
    }
    int ret = rename(from, buff1);
    if (-1 == ret) {
        logmsg(LOG_ERR, "FATAL: Cannot move and rename file '%s' to '%s' (%d : %s)", from, buff1, errno, strerror(errno));
    }
    if (newname != NULL) {
        *newname = '\0';
        strncpy(newname, buff1, maxlen - 1);
        newname[maxlen - 1] = '\0';
    }    
    return ret;
}

/*
 * Check if directory exists and if not create it
 *
 * @param basedir parent directory
 * @param dir Directory to create
 * @return 0 on success, -1 on failure
 */
int
chkcreatedir(const char *basedir, char *dir) {
    const mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    char bdirbuff[512];
    struct stat filestat;
    if (basedir == NULL) {
        return -1;
    }
    if (dir && *dir)
        snprintf(bdirbuff, sizeof (bdirbuff) - 1, "%s/%s", basedir, dir);
    else
        snprintf(bdirbuff, sizeof (bdirbuff) - 1, "%s", basedir);
    bdirbuff[sizeof (bdirbuff) - 1] = '\0';
    logmsg(LOG_DEBUG, "Checking directory '%s'", bdirbuff);
    if (-1 == stat(bdirbuff, &filestat)) {
        if (-1 == mkdir(bdirbuff, mode)) {
            logmsg(LOG_ERR, "FATAL: Cannot create directory %s (%d : %s).",
                    bdirbuff, errno, strerror(errno));
            return -1;
        } else {
            logmsg(LOG_DEBUG, "Created directory '%s'", bdirbuff);
        }
    }
    return 0;
}

/**
 * Strip the suffix by replacing the last '.' with a '\0'
 * The found suffix is placed in the space pointed to by
 * the suffix parameter if slen > 0
 *
 * @param filename  Original filname
 * @param suffix    (out)Found and stripped suffix
 * @param slen      Max length of suffix string
 * @return -1 on failure, 0 on success
 */
int
strip_filesuffix(char *filename, char *suffix, int slen) {
    if (filename == NULL || suffix == NULL) {
        return -1;
    }
    size_t len = strnlen(filename, 256);
    if (len >= 256) {
        logmsg(LOG_ERR, "FATAL: String too long to strip suffix");
        return -1;
    }
    size_t k = len - 1;
    while (k > 0 && filename[k] != '.') {
        k--;
    }
    if (k > 0) {
        strncpy(suffix, &filename[k + 1], slen - 1);
        suffix[slen - 1] = '\0';
        filename[k] = '\0';
    }
    return 0;
}

/**
 * Return the last n line from the logfile
 * @param n Number of lines to return, 0 to return entire log file
 * @param buffer Buffer
 * @param maxlen Maximum length of the buffer
 * @return 0 success, -1 failure
 */
int
tail_logfile(unsigned n, char *buffer, size_t maxlen) {
    if (n > 999)
        return -1;

    // We can only show the logfile if a proper file has been specified
    // and not stdout or the system logger
    if (strcmp(logfile_name, "stdout") == 0 || strcmp(logfile_name, LOGFILE_SYSLOG) == 0) {
        logmsg(LOG_ERR, "Trying to view logfile when logfile is not a file.");
        return -1;
    }

    char cmd[512];
    if (n > 0) {
        snprintf(cmd, 512, "tail -n %u %s", n, logfile_name);
    } else {
        snprintf(cmd, 512, "cat %s", logfile_name);
    }

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        logmsg(LOG_ERR, "Failed popen() in tail_logfile(). (%d : %s)", errno, strerror(errno));
        return -1;
    }

    char linebuffer[512];
    *buffer = '\0';
    while (maxlen > 512 && NULL != fgets(linebuffer, sizeof (linebuffer), fp)) {
        strncat(buffer, linebuffer, maxlen - 1 - strlen(buffer));
        maxlen -= strlen(linebuffer);
    }

    int ret = 0;
    if (maxlen <= sizeof (linebuffer) || maxlen <= 512) {
        strncat(buffer, "\n(..logfile truncated)\n", sizeof (linebuffer) - 1 - strlen(buffer));
        ret = -1;
    }
    pclose(fp);
    return ret;
}

/**
 * An iterator function that will apply the function argument supplied as the last
 * argument to all files in directory "dirbuff" with specified suffix .
 *
 * @param dirbuff       Directory to scan
 * @param suffix        Only include files with this suffix (including '.')
 * @param maxfiles      Maximum files to scan
 * @param numfiles      (out)Actual number of files found (and scanned)
 * @param callback      The callback function that is applied to each file
 * @return -1 on failure, 0 on success
 */
int
process_files(const char *dirbuff, char *suffix, size_t maxfiles, size_t *numfiles, pfi_cu_callback callback) {
    struct stat filestat;
    *numfiles = 0;

    if (-1 == stat(dirbuff, &filestat)) {
        logmsg(LOG_ERR, "Cannot find directory '%s' ( %d : %s )", dirbuff, errno, strerror(errno));
        return -1;
    }

    // Now loop through all files in 'dirbuff' directory
    DIR *dp;
    struct dirent *dirp;
    char tmpbuff[255];

    dp = opendir(dirbuff);
    if (dp == NULL) {
        logmsg(LOG_ERR, "Cannot open directory '%s' (%d : %s)", dirbuff, errno, strerror(errno));
        return -1;
    }

    while ((dirp = readdir(dp)) != NULL) {
        if ((strcmp(dirp->d_name, ".") != 0) && (strcmp(dirp->d_name, "..") != 0)) {

            // Only read files with suffix
            unsigned len = strnlen(dirp->d_name, 512);
            _Bool sflag = TRUE;
            if (suffix && strlen(suffix) > 1) {
                sflag = len > strlen(suffix) && (strncmp(suffix, dirp->d_name + len - strlen(suffix), 9) == 0);
            }


            if (sflag) {

                snprintf(tmpbuff, sizeof (tmpbuff) - 1, "%s/%s", dirbuff, dirp->d_name);
                lstat(tmpbuff, &filestat);

                if (S_ISREG(filestat.st_mode) || S_ISLNK(filestat.st_mode)) {

                    if (*numfiles >= maxfiles) {
                        logmsg(LOG_ERR, "Maximum number of files (%zd) in directory '%s' exceeded.", maxfiles, dirbuff);
                        closedir(dp);
                        return -1;
                    }
                    logmsg(LOG_DEBUG, "Processing file '%s'", tmpbuff);

                    (void) callback(tmpbuff, *numfiles);
                    (*numfiles)++;
                }

            } else {
                logmsg(LOG_DEBUG, "Ignoring file '%s' in directory '%s' (unknown suffix)", dirp->d_name, dirbuff);
            }

        }
    }
    closedir(dp);
    return 0;

}


/**
 * Read a file into a memory buffer
 * @param[in] name Name of file to read
 * @param[out] readlen The read file size
 * @param[out] buffer The memory buffer where the file is read into. It is the calling 
 * routines responsibility to free the buffer
 * @param[in] maxlen The maximum size of memory buffer
 * @return 0 on success, -1 on failure
 */
int
read_file_buffer(char *name, size_t *readlen, char **buffer) {
    int fd = open(name,O_RDONLY);
    
    if( fd < 0 ) {
        logmsg(LOG_ERR,"Cannot open file \"%s\" ( %d : %s)",name,errno,strerror(errno));
        return -1;
    }
    
    struct stat stat;
    int rc = fstat(fd,&stat);    
         
    if( -1 == rc ) {         
        close(fd);                 
        logmsg(LOG_ERR,"Cannot stat file \"%s\" (%d : %s)",name,errno,strerror(errno));
        return -1;
    }
    
    *buffer = calloc(stat.st_size, sizeof(char));
    if( NULL == *buffer ) {
        logmsg(LOG_ERR,"Cannot allocate memory");
        close(fd);
        return -1;
    }
    
    int len = read(fd,*buffer,stat.st_size);    
    close(fd);         
    if( len < 0 || len != stat.st_size  )  {        
        logmsg(LOG_ERR,"Cannot read \"%s\" ( %d : %s)",name,errno,strerror(errno));
        free(*buffer);
        return -1;
    }
    
    *readlen = stat.st_size;
    return 0;
    
}
