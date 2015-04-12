/* =========================================================================
 * File:        presets.h
 * Description: Handle preset command files
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: presets.h 700 2015-02-05 06:51:55Z ljp $
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

#ifndef PRESETS_H
#define	PRESETS_H

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * Name of the directory under the main data directory where the presets are
 * stored.
 */
#define PRESET_SUBDIR "presets"

/// File suffix for preset files
#define PRESET_FILESUFFIX "preset"

/**
 * Structure to hold information on one preset definition.
 */
struct presetType {
    /** Preset name*/
    char name[32];
    /** Short description of preset (First line in preset file)*/
    char shortdesc[256];
    /** Long description */
    char longdesc[2048];
    /** Number of commands in preset */
    size_t ncmd;
    /** List of commands in preset */
    char cmd[32][64];
};

int
init_presets(void);

int
refreshPresets(int sockd);

int
getPreset(const char *name, struct presetType **ptr);

void
listPresets(int sockd);

int
commandPreset(struct client_info *cli_info, char *cmd, size_t nf, char **fields);

int
execPresetFunc(struct client_info *cli_info, const char *presetString, const char *tag, const char *pin);

#ifdef	__cplusplus
}
#endif

#endif	/* PRESETS_H */

