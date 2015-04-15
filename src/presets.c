/* =========================================================================
 * File:        presets.c
 * Description: Handle preset command files.
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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <libgen.h>
#include <signal.h>

#include "config.h"
#include "build.h"
#include "g7ctrl.h"
#include "utils.h"
#include "futils.h"
#include "logger.h"
#include "g7config.h"
#include "xstr.h"
#include "presets.h"
#include "g7cmd.h"
#include "connwatcher.h"
#include "g7sendcmd.h"

#define MAX_PRESETS 64

static struct presetType *presets = NULL;
static size_t numPresets = 0;

/**
 * Initialize the memory structures used by the presets
 * @return 0 on success, -1 on failure (out of memory error)
 */
int
init_presets(void) {
    presets = calloc(MAX_PRESETS, sizeof (struct presetType));
    if (NULL == presets) {
        logmsg(LOG_CRIT, "Cannot allocate preset structure");
        return -1;
    }
    return 0;
}

/**
 * This gets called for each of the found presets. Note this is a callback
 * function used together with process_files().
 * @param fullPath Full path to the preset file found
 * @param seqNbr A sequence number to indicate which preset file in order
 *        this is.
 * @return 0 on success, -1 on failure
 */
int
callback_preset_file(char *fullPath, size_t seqNbr) {

    FILE *fp = fopen(fullPath, "r");

    if (NULL == fp) {
        logmsg(LOG_ERR, "Cannot open preset file \"%s\"", fullPath);
        return -1;
    }

    char *tmp_fullPath = strdup(fullPath);
    if (strnlen(basename(tmp_fullPath), 128) >= sizeof (presets[seqNbr].name)) {
        logmsg(LOG_ERR, "Basename of preset filename too large (>=128 chars): %s", fullPath);
        free(tmp_fullPath);
        fclose(fp);
        return -1;
    }
    free(tmp_fullPath);

    size_t llen = 512;
    char *lineBuffer = calloc(1, llen);

    ssize_t rc;
    size_t lnbr = 0, ncmd = 0;
    char suffix[16];

    tmp_fullPath = strdup(fullPath);
    xmb_strncpy(presets[seqNbr].name, basename(tmp_fullPath), sizeof (presets[seqNbr].name) - 1);
    free(tmp_fullPath);

    strip_filesuffix(presets[seqNbr].name, suffix, sizeof (suffix));
    presets[seqNbr].shortdesc[0] = '\0';
    presets[seqNbr].longdesc[0] = '\0';
    presets[seqNbr].ncmd = 0;

    do {
        rc = getline(&lineBuffer, &llen, fp);
        if (rc > 0) {
            xstrtrim_crnl(lineBuffer);
            // Ignore empty lines
            if (! *lineBuffer)
                continue;
            if ('#' == *lineBuffer) {
                // Long or short description
                if (0 == lnbr) {
                    // Short description
                    xmb_strncpy(presets[seqNbr].shortdesc, lineBuffer + sizeof (char), sizeof (presets[seqNbr].shortdesc) - 1);
                } else if (*(lineBuffer + 1)) {
                    // Only include lines which actually have some data
                    if (*(presets[seqNbr].longdesc)) {
                        strncat(presets[seqNbr].longdesc, lineBuffer + sizeof (char), sizeof (presets[seqNbr].longdesc) - strlen(presets[seqNbr].longdesc) - 1);
                    } else {
                        xmb_strncpy(presets[seqNbr].longdesc, lineBuffer + sizeof (char), sizeof (presets[seqNbr].longdesc) - 1);
                    }
                    strcat(presets[seqNbr].longdesc, "\n");
                }
            } else {
                // Command line
                logmsg(LOG_DEBUG, "CMD [%zd,%zd]: %s", seqNbr, ncmd, lineBuffer);
                xmb_strncpy(presets[seqNbr].cmd[ncmd], lineBuffer, sizeof (presets[seqNbr].cmd[ncmd]) - 1);
                ncmd++;
            }
        }
        lnbr++;
    } while (rc > 0);

    free(lineBuffer);
    fclose(fp);
    logmsg(LOG_DEBUG, "Found %zd commands in preset file \"%s\"", ncmd, fullPath);
    presets[seqNbr].ncmd = ncmd;
    return 0;
}

/**
 * Re-read all preset files from disk
 * @param sockd Client socket handler to print messages to
 * @return  0 on success, -1 on failure
 */
int
refreshPresets(int sockd) {

    char dirName[256];
    size_t foundFiles = 0;
    numPresets = 0;

    snprintf(dirName, sizeof (dirName) - 1, "%s/%s", data_dir, PRESET_SUBDIR);
    int rc = process_files(dirName, PRESET_FILESUFFIX, MAX_PRESETS, &foundFiles, callback_preset_file);
    if (rc < 0) {
        logmsg(LOG_CRIT, "Cannot read preset files ( %d : %s ) ", errno, strerror(errno));
        if (sockd >= 0)
            _writef(sockd, "[ERR] Unknown error re-reading preset files.");
        return -1;
    }
    numPresets = foundFiles;

    if (foundFiles) {
        if (sockd >= 0)
            _writef(sockd, "Refreshed %zd preset files.", foundFiles);
        logmsg(LOG_INFO, "Refreshed %zd preset files.", foundFiles);
    } else {
        if (sockd >= 0)
            _writef(sockd, "[ERR] Found no preset files to refresh.");
        logmsg(LOG_INFO, "Found no preset files to refresh.");
    }
    return 0;
}

/**
 * Get the specified preset data
 * @param name Name of preset
 * @param ptr Pointer that gets set to the data structure for this preset
 * @return 0 on success, -1 on failure
 */
int
getPreset(const char *name, struct presetType **ptr) {
    size_t i = 0;
    for (i = 0; i < numPresets; ++i) {
        if (0 == strcmp(name, presets[i].name))
            break;
    }
    if (i >= numPresets) {
        logmsg(LOG_ERR, "Preset \"%s\" can not be found", name);
        return -1;
    }
    *ptr = &presets[i];
    return 0;
}

/**
 * List all existing presets to the specified socket
 * @param sockd Socket to write to
 */
void
listPresets(int sockd) {
    if (0 == numPresets) {
        _writef(sockd, "[ERR] No presets defined.");
        return;
    }

    for (size_t i = 0; i < numPresets; ++i) {
        _writef(sockd, "%02zd. %-10s - %s", i, presets[i].name, presets[i].shortdesc);
        if (i < numPresets - 1) {
            _writef(sockd, "\n");
        }
    }
}

/**
 * Execute a single command function. This is a server device command given in the function form
 * as FUNC(par1,par2,..)
 * @param cli_info  Client context
 * @param presetString  The function string
 * @param tag Command tag
 * @param pin Device PIN code
 * @return 0 on success, -1 on failure
 */
int
execPresetFunc(struct client_info *cli_info, const char *presetString, const char *tag, const char *pin) {
    char cmdBuf[128], tmpBuf[128];

    CLEAR(tmpBuf);
    CLEAR(cmdBuf);
    
    xmb_strncpy(tmpBuf, presetString, sizeof(tmpBuf)-1);    
    
    // Find out command name
    char *c = tmpBuf;
    size_t cnt = 0 ;
    const size_t MAXPRESETNAME=15;
    
    while (cnt < MAXPRESETNAME && *c && *c != '(') {
        c++;
        cnt++;
    }
    if( ! *c || cnt >= MAXPRESETNAME ) {
        logmsg(LOG_ERR,"Preset name too long in \"%s\"",tmpBuf);
        return -1;
    }
    *c = '\0'; // Terminate function name at the first '('

    // Translate the daemon command to the raw device command
    // They are not always identical. The daemon command names are simpler!
    char devCmd[12];
    int rc = get_devcmd_from_srvcmd(tmpBuf, sizeof (devCmd), devCmd);
    if (rc < 0) {
        return -1;
    }
    snprintf(cmdBuf, sizeof (cmdBuf) - 1, "$WP+%s+%s=%s", devCmd, tag, pin);

    char argBuf[64];
    char *ap = argBuf;
    // Build argument list
    do {
        ++c;
        while (*c && (',' != *c) && (')' != *c) )  {
            if( ' ' == *c ) // Eat up any spaces
                c++;
            else
                *ap++ = *c++;
        }
        *ap = '\0';
        if (*argBuf) {
            strcat(cmdBuf, ",");
            strcat(cmdBuf, argBuf);            
        } else {
            // Add an empty argument (Preserve the previous value)
            strcat(cmdBuf, ",");
        }
        ap = argBuf;

    } while (*c && (*c != ')') );


    // We use (-1) for sockd in the call to avoid having any output from the
    // commands displayed to the user.    
    const int old_sockd = cli_info->cli_socket;
    cli_info->cli_socket = -1;

    rc = send_rawcmd(cli_info, cmdBuf, tag);

    cli_info->cli_socket = old_sockd;

    if (rc < 0)
        return -1;
    else
        return 0;
}

/**
 * Construct a raw device command from the command stored in the preset
 * @param pre Pointer to preset data structure
 * @param cmdIdx Command index for this command
 * @param tag The tag to use in command (can be an empty string)
 * @param pin Device PIN code
 * @return 0 on success, -1 on failure
 */
int
execPresetCommand(struct client_info *cli_info, struct presetType *pre, size_t cmdIdx, const char *tag, const char *pin) {
    int rc = execPresetFunc(cli_info, pre->cmd[cmdIdx], tag, pin);
    if (rc < 0) {
        logmsg(LOG_ERR, "Cannot execute command in preset file \"%s\" command #%zd. Incorrect command name?", pre->name, cmdIdx);
        return -1;
    }
    return 0;
}

/**
 * Execute all commands in the specified preset
 * @param sockd Client socket to write to
 * @param presetName Name of preset
 * @return 0 on success, -1 on failure
 */
int
execPreset(struct client_info *cli_info, char *presetName) {

    const int sockd = cli_info->cli_socket;

    struct presetType *ptr;
    char pinbuff[16];
    char tagbuff[16];

    if (-1 == get_device_pin(pinbuff, sizeof (pinbuff))) {
        return -1;
    }

    if (0 == getPreset(presetName, &ptr)) {
        logmsg(LOG_DEBUG, "Executing %zd commands in preset \"%s\"", ptr->ncmd, ptr->name);
        for (size_t i = 0; i < ptr->ncmd; ++i) {
            if (-1 == get_device_tag(tagbuff, sizeof (tagbuff))) {
                return -1;
            }
            logmsg(LOG_DEBUG, "   #%02zd : %s", i, ptr->cmd[i]);
            if (-1 == execPresetCommand(cli_info, ptr, i, tagbuff, pinbuff)) {
                logmsg(LOG_ERR, "Failed to execute command #%02zd in preset \"%s\"", i, ptr->name);
                _writef(sockd, "[ERR] Failed to execute command #%02zd in preset \"%s\".", i, ptr->name);
                return -1;
            }
            usleep(1000); // Sleep 0.1s between each command to give the device some time.
        }
        _writef(sockd, "Successfully executed %zd commands in preset \"%s\"", ptr->ncmd, ptr->name);
        return 0;
    } else {
        _writef(sockd, "[ERR] Preset \"%s\" does not exist.", presetName);
        return -1;
    }
}

/**
 * Print help on detailed preset usage to the client socket
 * @param sockd Client socket to print to
 * @param presetName Name of preset to give help for
 */
void
presetHelp(int sockd, char *presetName) {
    struct presetType *ptr;
    if (0 == getPreset(presetName, &ptr)) {
        _writef(sockd, "%s - %s\n\n%s", ptr->name, ptr->shortdesc, ptr->longdesc);
    } else {
        _writef(sockd, "[ERR] Preset \"%s\" does not exist.", presetName);
    }

}

/**
 * The command dispatcher calls this function when it has detected a
 * generic preset command
 * @param sockd Client socket to communicate on
 * @param cmd Original command string
 * @param nf Number for fields
 * @param fields The parsed fields in the command string
 * @return 0 on success, -1 on failure
 */
int
commandPreset(struct client_info *cli_info, char *cmd, size_t nf, char **fields) {
    // There are three possible commands
    // preset list         - Lists all defined presets
    // preset use <name>   - Execute preset <name>
    // preset help <name>  - Show help for preset <name>
    // preset refresh      - Re-read preset files

    const int sockd = cli_info->cli_socket;
    switch (nf) {
        case 2:
            if (0 == strcmp("refresh", fields[1])) {
                (void) refreshPresets(sockd);
            } else if (0 == strcmp("list", fields[1])) {
                listPresets(sockd);
            }
            break;
        case 3:
            if (0 == strcmp("use", fields[1])) {
                int rc = execPreset(cli_info, fields[2]);
                if (rc < 0)
                    return -1;
            } else if (0 == strcmp("help", fields[1])) {
                presetHelp(sockd, fields[2]);
            }
            break;

        default:
            logmsg(LOG_ERR, "Unknown preset command \"%s\"", cmd);
            _writef(sockd, "[ERR] Unknown preset command \"%s\"", cmd);
            for (size_t i = 0; i < nf; ++i) {
                logmsg(LOG_DEBUG, "#%02zd : %s", i, fields[i]);
            }
            return -1;
    }
    return 0;
}

/* EOF */
