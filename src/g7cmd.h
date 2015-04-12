/* =========================================================================
 * File:        G7CMD.H
 * Description: Command handling for native G7 command
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: g7cmd.h 733 2015-03-01 17:45:37Z ljp $
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

#ifndef G7CMD_H
#define	G7CMD_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "xstr.h"

/**
 * Default serial port rate setting. This is more than what the device
 * can saturate. In fact the device seems to max out at around ~40kB/s
 * so the serial speed is not the limiting factor here.
 */
#define DEVICE_BAUD_RATE 57600

/**
 * Command modes for device command. All commands support the
 * writing but not all commands support the query (R) mode.
 */
#define CMD_MODE_R 1
#define CMD_MODE_W 2
#define CMD_MODE_RW (CMD_MODE_R|CMD_MODE_W)

/**
 * Type of commands
 */
#define CMD_TYPE_GET 1
#define CMD_TYPE_SET 2
#define CMD_TYPE_GETSET_BINARY 4
#define CMD_TYPE_GETSET 8
#define CMD_TYPE_DO 16

/**
 * Each command may have a number of arguments of different types. The types
 * can be either string, bool, int, float or selection from a list. These constants
 * helps define each command.
 */
#define ARGT_BOOL 0
#define ARGT_INT 1
#define ARGT_FLOAT 2
#define ARGT_SELECT 3
#define ARGT_STRING 4

/// Maximum number of items in the select list
#define MAX_SELECT 15

/// Maximum number of argument for any command
#define MAX_ARGS 15

// A special label for argument that is not shown to the user. Instead this argument
// is automatically set to on/off depending on what the user have specified with the
// on/off argument.
#define AUTO_ENABLE_LABEL "_Enabled"

// Internal error code. This is used to close down the thread once the user have
// disconnected.
#define USER_DISCONNECT (-99)

// Size for a medium sized buffer. Used in many places.
#define BUFFER_10K (10*1024)
#define BUFFER_20K (20*1024)
#define BUFFER_50K (50*1024)
#define BUFFER_100K (100*1024)

// Event IDs as sent back by the device
#define EVENT_GETLOC 0
#define EVENT_REC 1
#define EVENT_TRACK 2
#define EVENT_TIMER 4
#define EVENT_WAKEUP 34
#define EVENT_SLEEP 37
#define EVENT_LOWBATT 40
#define EVENT_GFEN 48
#define EVENT_SETRA 100

/**
 * Defines the structure for a list of all event types used by the device.
 * Each entry consist of the event id, a short description and the related
 * command that is used to generate/setup this event.
 * @see get_event_cmd()
*/
struct g7event {
    /** Event id */
    int id;
    /** Human description string of event */
    const char *desc;
    /** Related device command that can generate this event */
    const char *cmd;
};

extern struct g7event g7event_list[];
extern const size_t g7event_list_num;

void *
cmd_clientsrv(void *arg);

int
get_event_cmd(const int eventid, char *cmd, char *desc);

int
get_devcmd_from_srvcmd(const char *srvcmd, size_t maxlen, char *devcmd);

int
get_srvcmd_from_devcmd(const char *devcmd, char *srvcmd, size_t maxlen);

int
cmdarg_to_text(const int sockd, const char *devcmd, struct splitfields *flds);

int
extract_devcmd_reply_simple(const char *raw,char *reply,size_t maxreply);


#ifdef	__cplusplus
}
#endif

#endif	/* G7CMD_H */

