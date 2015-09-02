/* =========================================================================
 * File:        G7CMD.C
 * Description: Command handling for native G7 command
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: g7cmd.c 1043 2015-09-02 06:59:54Z ljp $
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

// Standard UNIX includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sqlite3.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <signal.h>

#include "config.h"
#include "build.h"
#include "g7ctrl.h"
#include "utils.h"
#include "logger.h"
#include "g7config.h"
#include "serial.h"
#include "xstr.h"
#include "connwatcher.h"
#include "dbcmd.h"
#include "presets.h"
#include "g7sendcmd.h"
#include "g7cmd.h"
#include "g7srvcmd.h"
#include "export.h"
#include "geoloc.h"

/** Each command handler takes a socket, command index and a mode specific optional flag */
typedef int (*ptrcmd)(struct client_info *, const int, const int);

// Forward prototypes for command handlers to deal with the special case
// when we download the log from the device
int exec_cmd_batt(struct client_info *cli_info, const int cmdidx, const int gsflag);
int exec_cmd_address(struct client_info *cli_info, const int cmdidx, const int gsflag);

//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wunused-parameter"
//#pragma GCC diagnostic pop

/**
 * The following are prototypes for the four classes of command handlers that
 * we use. One command handler caters for multiple device commands depending on
 * usage.
 * - Binary commands are commands that the user can enable/disable
 * - Get command are commands where only a read from the device is available
 * - Do commands are commands without arguments but with side effects such as
 *   resetting the device.
 * - Set command is for specifying arguments to a command.
 *
 * Note: A number of commands support both Get/Set calling. In the command list
 * the actual supported modes for a command is specified with the CMD_MODE_xxx flag.
 */
static int cmd_binary_template(struct client_info *cli_info, const int cmdidx, const int gsflag);
static int cmd_get_template(struct client_info *cli_info, const int cmdidx, const int gsflag);
static int cmd_do_template(struct client_info *cli_info, const int cmdidx, const int gsflag);
static int cmd_set_template(struct client_info *cli_info, const int cmdidx, const int gsflag);
int exec_dlrec(struct client_info *cli_info, const int cmdidx, const int gsflag);
int exec_get_locg(struct client_info *cli_info);
int prepare_cmd_to_device(struct client_info *cli_info, const int cmdidx, const int mode, const char *argstrlist);
int get_argidx_for_cmdidx(int cmdidx);
int get_cmd_args(const int sockd, int cmdidx, size_t maxlen, char *argbuff);
static int error_notsupported_cmd(const int sockd, const char *cmd, int arg);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunneeded-internal-declaration"


/**
 * A list of the possible event that causes the device to send back
 * an event to the server
 */
struct g7event g7event_list[] = {
    {EVENT_GETLOC, "Position data", "GETLOCATION"},
    {EVENT_REC, "Logging data", "REC"},
    {EVENT_TRACK, "Track position data", "TRACK"},
    {EVENT_TIMER, "Timer report", "PSMT-TIMER"},
    {EVENT_WAKEUP, "Wake Up Report", "PSMT-WAKE"},
    {EVENT_SLEEP, "Enter Sleeping Report", "PSMT-SLEEP"},
    {EVENT_LOWBATT, "Internal Battery Low Alert", "LOWBATT"},
    {EVENT_GFEN, "Virtual fence crossing", "GFEN"},
    {EVENT_SETRA, "Unit Detaching Report", "SETRA"}
};



// Size of g7event_list (number of entries)
const size_t g7event_list_num = sizeof (g7event_list) / sizeof (struct g7event);

#pragma clang diagnostic pop
#pragma GCC diagnostic pop

/**
 * Structure to hold information about one command. Both the daemon command strings
 * as well as the raw device command are stored. The are sometimes
 * the same but in a few cases some more descriptive name has been chosen in the server.
 * Each command also has information on what modes it supports as well as a pointer to the command handling
 * template. Finally there is a short and long help string that is used to display information
 * when the user calls the help command.
 */
struct g7command {
    /** Command used in server (what the user types) */
    const char *srvcmd;
    /** Command string without $WP+ prefix to send to device */
    const char *cmdstr;
    /** Pointer to function handling this command */
    const ptrcmd cmdhandler;
    /** Supported command types: 0=get, 1=set, 3=get/set binary, 4=get/set */
    const int type;
    /** Combination of CMD_MODE_xxx flags to indicate if this command support R,W or both */
    const int modes;
    /** Short description of command */
    const char *descrshort;
    /** Long description of command */
    const char *descrlong;
};

// TODO: get geofevt must be tweaked to take an argument !
/**
 * List of all supported commands in the daemon
 */
static struct g7command g7command_list[] = {
    /* Binary commands, i.e. on/off/read */
    /* 0 */
    { "roam", "ROAMING", cmd_binary_template, CMD_TYPE_GETSET_BINARY, CMD_MODE_RW, "Enable/Disable GPRS roaming function", ""},
    /* 1 */
    { "phone", "VLOCATION", cmd_binary_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Enable the function \"Get the current location by making a phone call\"", ""},
    /* 2 */
    { "led", "ELED", cmd_binary_template, CMD_TYPE_GETSET_BINARY, CMD_MODE_RW, "Enable/Disable the LED indicator on/off", ""},
    /* 3 */
    { "gfen", "GFEN", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Enable/Disabling virtual fence", ""},
    /* 4 */
    { "sleep", "SLEEP", cmd_binary_template, CMD_TYPE_GETSET_BINARY, CMD_MODE_RW, "Enable/Disable \"Sleeping Report\"", ""},

    /* Get commands */
    /* 5 */
    { "loc", "GETLOCATION", cmd_get_template, CMD_TYPE_GET, CMD_MODE_R, "Get latest location", "Get latest location"},
    /* NOTE: This command is actually handled "manually" in the command parser since this is a non-native command to the device
     * We just include it here so that it will be shown in the correct place when the help is shown.
     */
    /* 6 */
    { "locg", "GETLOCATION", NULL, CMD_TYPE_GET, CMD_MODE_R, "Get latest location as a Google map string", ""},
    /* 7 */
    { "imei", "IMEI", cmd_get_template, CMD_TYPE_GET, CMD_MODE_R, "Query the IMEI number of the internal GSM module", ""},
    /* 8 */
    { "sim", "SIMID", cmd_get_template, CMD_TYPE_GET, CMD_MODE_R, "Query the identification of the SIM card", ""},
    /* 9 */
    { "ver", "VER", cmd_get_template, CMD_TYPE_GET, CMD_MODE_R, "Get current firmware version", "Get SW version"},
    /* 10 */
    { "nrec", "DLREC", cmd_get_template, CMD_TYPE_GET, CMD_MODE_R, "Get number of locations in device memory", "Get number of locations and date range for locations stored in device memory, see also dlrec and clreac"},
    /* 11 */
    { "batt", "XXXX", exec_cmd_batt, CMD_TYPE_GET, CMD_MODE_R, "Read the battery voltage from device", ""},

    /* Get/Set command */
    /* 12 */
    { "track", "TRACK", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Control device tracking set up", ""},
    /* 13 */
    { "mswitch", "SETRA", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Set handling of detach event", ""},
    /* 14 */
    { "tz", "SETTZ", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Set the time zone information for the device", ""},
    /* 15 */
    { "sms", "SMSM", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Switch the SMS format (Text of PDU mode)", ""},
    /* 16 */
    { "comm", "COMMTYPE", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Set/Read device communication type and its parameters", ""},
    /* 17 */
    { "vip", "SETVIP", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Preset up to 5 SMS phone numbers for receiving different alerts", ""},
    /* 18 */
    { "ps", "PSMT", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Enable/Disable power saving mode", ""},
    /* 19 */
    { "config", "UNCFG", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Set/Read device ID, Password, and PIN Code of the SIM card", ""},
    /* 20 */
    { "gfevt", "SETEVT", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Enable/Disable/Set/Read GEO-Fencing event", ""},
    /* 21 */
    { "rec", "REC", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Enable/Disable/read logging function to the device", ""},
    /* 22 */
    { "lowbatt", "LOWBATT", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Set/Read the internal battery low level alert", ""},
    /* 23 */
    { "sens", "GSS", cmd_set_template, CMD_TYPE_GETSET, CMD_MODE_RW, "Get/Set sensitivity for movement detection", ""},

    /* Execute system commands (no arguments) */
    /* 24 */
    { "clrec", "CLREC", cmd_do_template, CMD_TYPE_DO, CMD_MODE_W, "Erase all stored locations from the device memory", ""},
    /* 25 */
    { "dlrec", "DLREC", exec_dlrec, CMD_TYPE_DO, CMD_MODE_W, "Download all stored locations in device memory to DB", ""},

    /* 26 */
    { "reboot", "REBOOT", cmd_do_template, CMD_TYPE_DO, CMD_MODE_W, "Restart-up the device", ""},
    /* 27 */
    { "reset", "RESET", cmd_do_template, CMD_TYPE_DO, CMD_MODE_W, "Reset all parameters to the manufactory default settings", ""},
    /* 28 */
    { "test", "TEST", cmd_do_template, CMD_TYPE_DO, CMD_MODE_W, "Device diagnostic function", ""},
    /* 29 */
    { "address", "GETADDRESS", exec_cmd_address, CMD_TYPE_GET, CMD_MODE_R, "Get latest location as a street address", ""}
};

#define CMDIDX_SETEVT 20 // Index for setevt since that requires special handling compare with all other GET functions

/// Size of g7command_list (number of entries)
const size_t g7command_list_len = sizeof (g7command_list) / sizeof (struct g7command);

/**
 * The following structure and list details of all possible arguments to all commands. This is used
 * to question the user for each argument in turn when he wants to execute a specific command.
 */
struct cmdargs {
    /** Command name */
    const char *srvcmd;
    /** Number of arguments in command*/
    const size_t numargs;

    /** Each argument is described in its own structure*/
    const struct {
        /** The text label to prompt user for this arg (excluding selection choice) */
        char *arglabel;
        /** Short description of argument*/
        char *argdesc;
        /** Type of argument (for error checking), 0=bool, 1=integer, 2=float, 3=selection */
        int type;
        /** Number of possible selects for this argument (only used with type==3) */
        size_t nsel;

        /** Structure to hold information about each possible select alternative*/
        const struct {
            /** Value as the result of the selected options */
            char *val;
            /** Prompt to display for this option*/
            char *selectlabel;
        } select[MAX_SELECT];
        /** The list of possible arguments for this command */
    } argl[MAX_ARGS];
};

// Needed so GCC doesn't bark about the CLANG pragmas
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

// Needed so that CLANG doesn't bark about the uninitialized fields when the
// command doesn't use all the arguments available to initialize
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

/**
 * All the commands that allows "set" must have their arguments described in this structure.
 * It is possible for a some commands to have zero explicit arguments, for example "set led". 
 * Those commands still send an argument to the device
 * They must however be included here and explicitly be set to have 0 user arguments.
 * 
 * There is no need to include "get" and "do" only commands here since they never take any arguments
 */
static struct cmdargs cmdargs_list[] = {
    { "sleep", 1,
        {
            { "Message mode", "Set how to handle message at when device enters sleep", ARGT_SELECT, 3,
                {
                    { "1", "Log message to device"},
                    { "2", "Send back message"},
                    { "3", "Both log and send back"},
                }}
        }},
    { "roam", 0,
        {
            {0, 0, 0, 0,
                {
                    {0, 0}
                }}
        }},
    { "phone", 2,
        {
            {"On/Off", "Enable disable location by phone", ARGT_SELECT, 2,
                {
                    {"0", "Disabled"},
                    {"1", "Enabled"}
                }},
            {"VIP mask", "Bitmask for allowed VIP numbers", ARGT_INT, 0,
                {
                    {0, 0}
                }}
        }},
    { "led", 0,
        {
            {0, 0, 0, 0,
                {
                    {0, 0}
                }}
        }},
    { "mswitch", 2,
        {
            {"Report action", "Action to perform when device is detached", ARGT_SELECT, 4,
                {
                    {"0", "Disable"},
                    {"1", "Log on device"},
                    {"2", "Send back to server"},
                    {"3", "Both log and send back"}
                }},
            {"VIP mask", "Bitmask of VIP numbers to alert", ARGT_INT, 0,
                {
                    {0, 0}
                }}
        }},

    { "lowbatt", 2,
        {
            {"Report action", "Action to perform at low battery voltage", ARGT_SELECT, 4,
                {
                    {"0", "Disable"},
                    {"1", "Log on device"},
                    {"2", "Send back to server"},
                    {"3", "Both log and send back"}
                }},
            {"VIP mask", "Bit mask of VIP numbers to alert", ARGT_INT, 0,
                {
                    {0, 0}
                }}
        }},

    { "sms", 1,
        {
            {"Mode", "Set format for SMS sent back", ARGT_SELECT, 2,
                {
                    {"0", "Use PDU mode"},
                    {"1", "Use text mode"}
                }}
        }},
    { "sens", 1,
        {
            {"Sensitivity", "Set sensitivity for accelerometer", ARGT_SELECT, 5,
                {
                    {"1", "[1] Highest sensitivity"},
                    {"2", "[2]"},
                    {"3", "[3]"},
                    {"4", "[4]"},
                    {"5", "[5] Lowest sensitivity"},
                }}
        }},
    { "tz", 3,
        {
            {"Sign", "Ahead or before GMT zone", ARGT_SELECT, 2,
                {
                    {"+", "Ahead of GMT"},
                    {"-", "Before GMT"}
                }},
            {"Hour", "Number of hours to adjust (0-9)", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Min", "Minutes to adjust", ARGT_SELECT, 4,
                {
                    {"00", "0 min"},
                    {"15", "15 min"},
                    {"30", "30 min"},
                    {"45", "45 min"}
                }},
        }},

    { "gfen", 5,
        {
            {"Enable", "Turn option on/off", ARGT_BOOL, 0,
                {
                    {0, 0}
                }},
            {"Radius", "Radius in meter for fence. Radius is counted from the center where M7 sleeps (last recorded pos)", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Zone trigger", "Trigger when leaving or entering zone", ARGT_SELECT, 2,
                {
                    {"1", "Inside zone, moving inside->outside"},
                    {"2", "Outside zone,  moving inside->outside"}
                }},
            {"Report action", "What to do on event", ARGT_SELECT, 4,
                {
                    {"0", "Disabled"},
                    {"1", "Log to device"},
                    {"2", "Send back to server"},
                    {"3", "Both log and send back"}
                }},
            {"VIP mask", "Bitmask of VIP numbers to alert (0 to disable)", ARGT_INT, 0,
                {
                    {0, 0}
                }}
        }},

    { "track", 7,
        {
            { "Tracking mode", "Condition when tracking is started", ARGT_SELECT, 10,
                {
                    { "0", "Disabled"},
                    { "1", "Time interval"},
                    { "2", "Distance interval"},
                    { "3", "Time AND Distance interval"},
                    { "4", "Time OR distance interval"},
                    { "5", "Heading mode"},
                    { "6", "Heading OR time"},
                    { "7", "Heading OR distance"},
                    { "8", "Heading OR (Time AND Distance"},
                    { "9", "Heading OR Time OR Distance"}
                }},
            { "Timer interval", "Used for mode 1", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            { "Dist. interval", "Used for mode 2-9", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            { "Nbr of times", "Number of times to track (0=unlimited)", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            { "Track basis", "Wait for GPS fix or not", ARGT_SELECT, 2,
                {
                    {"0", "Send report if GPS fix"},
                    {"1", "Send report always"}
                }},
            { "Comm select", "Set type of report", ARGT_SELECT, 5,
                {
                    {"0", "Over USB"},
                    {"1", "Over GSM"},
                    {"2", "Over Reserved (not used)"},
                    {"3", "UDP over GPRS"},
                    {"4", "TCP/IP over GPRS"}
                }},
            { "Heading", "Used for modes 5-9", ARGT_INT, 0,
                {
                    {0, 0}
                }},
        }},

    { "comm", 10,
        {
            {"CommSelect", "Select primary type of communication", ARGT_SELECT, 5,
                {
                    {"0", "Use USB"},
                    {"1", "Use SMS over GSM"},
                    {"2", "CSD <reserved>"},
                    {"3", "UDP over GPRS"},
                    {"4", "TCP/IP over GPRS"}
                }},
            {"SMS Base Phone", "SMS base number to call", ARGT_STRING, 0,
                {
                    {0, 0}
                }},
            {"CSD Base Phone", "CSD base number <reserved and not used>", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"GPRS APN", "The operators APN", ARGT_STRING, 0,
                {
                    {0, 0}
                }},
            {"GPRS User", "User name if required", ARGT_STRING, 0,
                {
                    {0, 0}
                }},
            {"GPRS Passwd", "Password if required", ARGT_STRING, 0,
                {
                    {0, 0}
                }},
            {"GPRS Server", "Server IP address where the device reports back to", ARGT_STRING, 0,
                {
                    {0, 0}
                }},
            {"GPRS Server Port", "TCP/IP Port to use on server", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"GPRS Keep Alive", "Interval (in sec) between Keep Alive Packets", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"GPRS DNS", "Optional DNS server for device to use.", ARGT_STRING, 0,
                {
                    {0, 0}
                }}
        }},

    { "ps", 7,
        {
            {"Mode", "When to enable sleep mode", ARGT_SELECT, 4,
                {
                    {"0", "Sleep after 3min if no movement. Wake on movements. GSM=Stdby, GPRS=GPS=Off, G=On"},
                    {"1", "Sleep after 3min, wake every n min. GSM=GPRS=GPS=G=Off"},
                    {"2", "Sleep after 3min. wake on timer. GSM=GPRS=GPS=G=Off"},
                    {"3", "Sleep after 3min. wake on movement. GSM=GPRS=GPS=Off, G=On"}
                }},
            {"Sleep interval", "Used with mode 1, Interval in min between wake ups", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Wake up report", "Action when awaken", ARGT_SELECT, 4,
                {
                    {"0", "Disable"},
                    {"1", "Log on device"},
                    {"2", "Send back to server"},
                    {"3", "Both log and send back"}
                }},
            {"VIP mask", "Bitmask of VIP numbers to alert (0 disables)", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Timer 1", "Used with mode=2. 00-23 hr", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Timer 2", "Used with mode=2. 00-23 hr", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Timer 3", "Used with mode=2. 00-23 hr", ARGT_INT, 0,
                {
                    {0, 0}
                }}
        }},

    { "config", 3,
        {
            {"Device ID", "Set device ID (leftmost digit must always be 3)", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Device Password", "Set device Password (numeric)", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"SIM Card PIN", "The SIM cards PIN code to use", ARGT_INT, 0,
                {
                    {0, 0}
                }}
        }},

    { "vip", 5,
        {
            {"VIP 1", "Mobile number 1 (incl country prefix '+')", ARGT_STRING, 0,
                {
                    {0, 0}
                }},
            {"VIP 2", "Mobile number 2 (incl country prefix '+')", ARGT_STRING, 0,
                {
                    {0, 0}
                }},
            {"VIP 3", "Mobile number 3 (incl country prefix '+')", ARGT_STRING, 0,
                {
                    {0, 0}
                }},
            {"VIP 4", "Mobile number 4 (incl country prefix '+')", ARGT_STRING, 0,
                {
                    {0, 0}
                }},
            {"VIP 5", "Mobile number 5 (incl country prefix '+')", ARGT_STRING, 0,
                {
                    {0, 0}
                }},
        }},

    { "gfevt", 7,
        {
            {"Event ID", "Event ID. Maximum of 50 events. In range 50-99", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Enabled", "Enable/Disable event", ARGT_BOOL, 0,
                {
                    {0, 0}
                }},
            {"Longitude", "Longitude for center of event zone", ARGT_FLOAT, 0,
                {
                    {0, 0}
                }},
            {"Latitude", "Latitude for center of event zone", ARGT_FLOAT, 0,
                {
                    {0, 0}
                }},
            {"Radius", "Radius of event zone in meters (0-65535)", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"ZoneControl", "HOw to trigger event", ARGT_SELECT, 2,
                {
                    {"1", "Entering zone"},
                    {"2", "Leaving zone"}
                }},
            {"Action", "What to do on event", ARGT_SELECT, 3,
                {
                    {"1", "Log to device"},
                    {"2", "Send to server"},
                    {"3", "Both log and send"}
                }},
            {"VIP Phone mask", "Bit combination to enable/disable the five VIP numbers", ARGT_INT, 0,
                {
                    {0, 0}
                }}

        }},

    { "rec", 6,
        {
            {"Mode", "When should logging be done", ARGT_SELECT, 10,
                {
                    {"0", "Disable"},
                    {"1", "Time mode, log every n seconds"},
                    {"2", "Distance mode, log every n meters"},
                    {"3", "Time AND Distance mode"},
                    {"4", "Time OR Distance mode"},
                    {"5", "Heading mode, logging starts when heading > set value"},
                    {"6", "Heading OR Time mode"},
                    {"7", "Heading OR Distance"},
                    {"8", "Heading OR (Time AND Distance)"},
                    {"9", "Heading OR Time OR Distance"},
                }},
            {"Time interval", "Time interval (in sec) used in mode setting", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Distance interval", "Distance interval (in meters) used in mode setting", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Number of reports", "The number of reports to send bakc on event (0=continously logging)", ARGT_INT, 0,
                {
                    {0, 0}
                }},
            {"Record basis", "Logging mode", ARGT_SELECT, 2,
                {
                    {"0", "Wait for GPS fix before sending back report"},
                    {"1", "Don't wait for GPS fix before sending back report"},
                }},
            {"Heading", "Heading value used in mode (10-90)", ARGT_INT, 0,
                {
                    {0, 0}
                }}
        }},
};

/**
 * If devcmd is "test" then return 0 and set the buffer to the translated test
 * result. The test command needs special handling since it is a command that needs
 * to translate a list but it has not list as input, only output
 * @param devcmd
 * @param flds
 * @return 0 if command was "test" and the result could be translated
 */
int
cmd_test_reply_to_text(const char *strval, char *buf, size_t maxlen) {
    
    int val = *strval - '0';
    if( val < 0 || val > 3) {
        logmsg(LOG_ERR,"Value returned by \"test\" command is out of range = %d", val);
        return -1;
    }
    
    if( 0 == val )  {
        snprintf(buf,maxlen,"%s","PASS");
    } else if( 1 == val ) {
        snprintf(buf,maxlen,"%s","GSM Failure");
    } else if( 2 == val ) {
        snprintf(buf,maxlen,"%s","GPS Failure");
    } else {
        snprintf(buf,maxlen,"%s","GSM and GPS failure");
    }
    return 0;
}


#pragma clang diagnostic pop
#pragma GCC diagnostic pop

// Size of cmdargs_list (number of entries)
const size_t cmdargs_list_len = sizeof (cmdargs_list) / sizeof (struct cmdargs);


/**
 * Helper function for cmdarg_to_text()
 * @param str String buffer to write result to
 * @param maxlen Maximum length of string buffer
 * @param devcmd The device commadn used
 * @param flds All the fields in the reply
 * @return 0 on success -1 on failure
 */
int
cmdarg_to_text_string(char *str, const size_t maxlen, const char *devcmd, struct splitfields *flds) {
    char srvcmd[15];
    if (get_srvcmd_from_devcmd(devcmd, srvcmd, sizeof (srvcmd))) {
        logmsg(LOG_ERR, "Unknown device command");
        return -1;
    }
    size_t idx = 0;
    while (idx < cmdargs_list_len && strcmp(srvcmd, cmdargs_list[idx].srvcmd))
        idx++;
    if (idx >= cmdargs_list_len) {
        logmsg(LOG_INFO, "Command does not have arguments");
        return 0;
    }
    
    // Find out what type of command this is in order to handle binary commands
    // which is a special case since they are not considered to have argument
    size_t cmdidx = 0;
    while (cmdidx < g7command_list_len && strcmp(srvcmd, g7command_list[cmdidx].srvcmd))
        cmdidx++;
    if (cmdidx >= g7command_list_len) {
        logmsg(LOG_CRIT, "INTERNAL Error: Cannot find command \"%s\"",srvcmd);
        return 0;
    }
    
    *str='\0';
    if( CMD_TYPE_GETSET_BINARY == g7command_list[cmdidx].type ) {
        
        if( 1 == flds->nf ) {
            snprintf(str, maxlen, "\n%16s: %s\n", "Status",flds->fld[0][0]=='1' ? "On" : "Off");
        } else {
            logmsg(LOG_ERR, "Wrong number of arguments for %s. Found %zd but expected %d",
                    devcmd, flds->nf, 1);
            return -1;            
        }
        
    } else {
    
        if (flds->nf != cmdargs_list[idx].numargs) {
            logmsg(LOG_ERR, "Wrong number of arguments for %s. Found %zd but expected %zd",
                    devcmd, flds->nf, cmdargs_list[idx].numargs);
            return -1;
        }
        snprintf(str, maxlen, "\n");
        char buff[1024];
        *buff='\0';
        size_t nsel = 0;
        for (size_t i = 0; i < cmdargs_list[idx].numargs; i++) {
            switch (cmdargs_list[idx].argl[i].type) {
                case ARGT_BOOL:
                case ARGT_INT:
                case ARGT_FLOAT:
                case ARGT_STRING:
                    xvstrncat(str, maxlen, "%16s: %s\n", cmdargs_list[idx].argl[i].arglabel, flds->fld[i]);
                    break;
                case ARGT_SELECT: // selection
                    nsel = cmdargs_list[idx].argl[i].nsel;
                    size_t j = 0;
                    while (j < nsel && strcmp(cmdargs_list[idx].argl[i].select[j].val, flds->fld[i]))
                        ++j;
                    if (j >= nsel) {
                        logmsg(LOG_ERR, "Unknown argument value (%s) for select for command %s", flds->fld[i], devcmd);
                        return -1;
                    }
                    xvstrncat(str, maxlen,  "%16s: %s\n", cmdargs_list[idx].argl[i].arglabel, cmdargs_list[idx].argl[i].select[j].selectlabel);
                    break;
                default:
                    logmsg(LOG_CRIT, "Internal error. Unknown command type=%d", cmdargs_list[idx].argl[i].type);
                    return -1;
            }
        }
    }
    return 0;
}

/**
 * Translate the command reply into plaintext
 * @param sockd Client socket to communicate on
 * @param devcmd Device command to translate
 * @param flds The arguments to the device command
 * @return 0 on success, -1 on failure
 */
int
cmdarg_to_text(const int sockd, const char *devcmd, struct splitfields *flds) {
    const int maxlen=1025;
    char *str = calloc(maxlen,sizeof(char));
    if( str==NULL ) {
        logmsg(LOG_ERR,"Out of memory in cmdarg_to_text");
        return -1;
    }
    int rc;
    if( -1 == (rc=cmdarg_to_text_string(str, maxlen, devcmd, flds)) ) {
        logmsg(LOG_ERR,"Failed to translate command back to human readable strings!");
    }
    
    _writef(sockd,"%s",str);
    free(str);
    return rc;
}


/*============================================================================
 * Generic command templates used by device commands
 *============================================================================
 */

/**
 * This is a command template used for the table driven binary commands
 * @param sockd
 * @param cmdidx
 * @param gsflag
 * @return 0 on success, -1 on failure
 */
static int
cmd_binary_template(struct client_info *cli_info, const int cmdidx, const int gsflag) {
    
    const int sockd = cli_info->cli_socket;
    
    int mode = 0;
    char buff[256];
    *buff = '\0';

    switch (gsflag) {
        case 0: // Read
            mode = CMD_MODE_R;
            break;
        case 1: // On
            mode = CMD_MODE_W;
            int argidx = get_argidx_for_cmdidx(cmdidx);
            if (argidx < 0)
                return argidx;
            if (0 == cmdargs_list[argidx].numargs) {
                strcpy(buff, "1");
            } else {
                int rc = get_cmd_args(sockd, cmdidx, sizeof (buff), buff);
                if (rc) {
                    return rc;
                }
            }
            break;
        case 2: // Off
            mode = CMD_MODE_W;
            strcpy(buff, "0");
            break;
        default:
            return error_notsupported_cmd(sockd, g7command_list[cmdidx].cmdstr, gsflag);
    }
    return prepare_cmd_to_device(cli_info, cmdidx, mode, buff);
}

/**
 * This is a command template used for the table driven get commands
 * @param sockd
 * @param cmdidx
 * @param gsflag
 * @return 0 on success, -1 on failure
 */
static int
cmd_get_template(struct client_info *cli_info, const int cmdidx, const int gsflag) {
    
    const int sockd = cli_info->cli_socket;
    int mode = CMD_MODE_R;
    char buff[16];
    *buff = '\0';
    if (gsflag) {
        _writef(sockd, "[ERR] Invalid invocation of \"%s\"", g7command_list[cmdidx].cmdstr);
        return -1;
    }
    return prepare_cmd_to_device(cli_info, cmdidx, mode, buff);
}

/**
 * This is a command template used for the table driven set commands
 * @param sockd
 * @param cmdidx
 * @param gsflag
 * @return 0 on success, -1 on failure
 */
static int
cmd_set_template(struct client_info *cli_info, const int cmdidx, const int gsflag) {
    const int sockd = cli_info->cli_socket;
    int mode = 0;
    char buff[256];
    *buff = '\0';

    // Ugly special case for get gfevt which is handled by separate function.
    // If we get called here it means that the user has not supplied the
    // extra argument to gfevt which is required.
    if (CMDIDX_SETEVT == cmdidx && 0 == gsflag) {
        _writef(sockd, "[ERR] Missing argument. Command synopsis: \"get gfevt EVENTID\"");
        return -1;
    }


    logmsg(LOG_DEBUG, "_cmd_set_template(), cmdidx=%d, gsflag=%d", cmdidx, gsflag);
    switch (gsflag) {
        case 0: // Read
            mode = CMD_MODE_R;
            break;
        case 1: // On
            mode = CMD_MODE_W;
            int argidx = get_argidx_for_cmdidx(cmdidx);
            if (argidx < 0) {
                const char * const err = "Well, this is embarrassing. You found a bug. Please report Error #01 in cmd_set_template()";
                _writef(sockd, "[ERR] %s", err);
                logmsg(LOG_ERR, "%s", err);
                return -1;
            }
            if (0 == cmdargs_list[argidx].numargs) {
                strcpy(buff, "1");
            } else {
                int rc = get_cmd_args(sockd, cmdidx, sizeof (buff), buff);
                if (rc) {
                    return rc;
                }
            }
            break;
        default:
            return error_notsupported_cmd(sockd, g7command_list[cmdidx].cmdstr, gsflag);
    }
    return prepare_cmd_to_device(cli_info, cmdidx, mode, buff);

}

/**
 * This is a command template used for the table driven do commands
 * @param sockd Client socket
 * @param cmdidx Command index (in command table)
 * @param gsflag Get/Set flag
 * @return 0 on success, -1 on failure
 */
static int
cmd_do_template(struct client_info *cli_info, const int cmdidx, const int gsflag) {
    const int sockd = cli_info->cli_socket;
    int mode = CMD_MODE_W;
    char buff[16] = {'\0'};
    if (gsflag) {
        _writef(sockd, "[ERR] Invalid invocation of \"%s\"", g7command_list[cmdidx].cmdstr);
        return -1;
    }
    return prepare_cmd_to_device(cli_info, cmdidx, mode, buff);
}


/*============================================================================
 * Manage the command tables
 *============================================================================
 */

/**
 * Return the name of the command that is associated with the supplied
 * eventid. It is the responsibility of the calling routine to supply string
 * pointers that are at least 16 characters long for the cmd and 128 chars
 * for the description.
 *
 * @param[in] eventid The eventid to find the command for
 * @param[out] cmd The command will be written to the string pointed to
 * @param[out] desc If <> NULL then a one line description of the event will be
 * written to this buffer.
 * @return 0 on success, -1 on failure (event does not exist)
 */
int
get_event_cmd(const int eventid, char *cmd, char *desc) {
    if (cmd == NULL || desc == NULL)
        return -1;
    *cmd = *desc = '\0';
    size_t i = 0;
    while (i < g7event_list_num && g7event_list[i].id != eventid) {
        i++;
    }
    if (i < g7event_list_num) {
        strncpy(cmd, g7event_list[i].cmd, 15);
        strncpy(desc, g7event_list[i].desc, 127);
        return 0;
    }
    return -1;
}

/**
 * Get the the index into the argument table corresponding to a specific command
 * @param cmdidx Command index
 * @return -1 on not found, >= 0 the argument index
 */
int
get_argidx_for_cmdidx(int cmdidx) {
    size_t i = 0;
    while (i < cmdargs_list_len &&
            xstricmp(g7command_list[cmdidx].srvcmd, cmdargs_list[i].srvcmd)) {
        i++;
    }
    if (i >= cmdargs_list_len) {
        logmsg(LOG_DEBUG, "Internal error. get_argidx_for_cmdidx(int cmdidx). No argidx for cmdidx=%d", cmdidx);
        return -1;
    }
    return i;
}


/**
 * Return the corresponding index in command table for the specified command
 * name
 * @param cmdname Command name from user
 * @return -1 on failure , >= 0 for corresponding index
 */
int
get_cmdidx_from_srvcmd(const char *cmdname) {
    size_t i = 0;
    while (i < g7command_list_len && xstricmp(cmdname, g7command_list[i].srvcmd)) {
        ++i;
    }
    if (i < g7command_list_len) {
        return i;
    }
    return -1;
}


/**
 * Get the the index into the argument table corresponding to a specific server command
 * @param cmdname Server command name
 * @return -1 if the command does not have a argument list, >= 0 on success which is the arg table index
 */
int
get_argidx_for_srvcmd(const char *cmdname) {
    int cmdidx=get_cmdidx_from_srvcmd(cmdname);
    if( cmdidx==-1 )
        return -1;
    return get_argidx_for_cmdidx(cmdidx);
}

/**
 * Translate a single returned value from command to the corresponding human readable
 * string corresponding to the given value
 * @param cmdname The server command name (the command given in the shell, not the raw device command)
 * @param argnum The unmber of the argument to be translate (starts at 0)
 * @param val The value to the translated
 * @param human_string The corresponding human string
 * @param maxlen The maximum length of the human string
 * @return 0 on success, -1 on failure
 */
int
translate_cmd_argval_to_string(const char *cmdname, size_t argnum, char *val, char *human_string, size_t maxlen) {
    int argidx = get_argidx_for_srvcmd(cmdname);
    if( argidx < 0 ) {
        // Command does not have any arguments. Just return the value
        xstrlcpy(human_string,val,maxlen);
        return 0;
    }
    struct cmdargs *args = &cmdargs_list[argidx]; 
    if( argnum >= args->numargs )
        return -1;
     
    // It's only select value we need to translate other values such as 
    // float and ints are used verbatim
    if( args->argl[argnum].type == ARGT_SELECT ) {
        size_t i=0;
        while( i < args->argl[argnum].nsel && strcmp(val,args->argl[argnum].select[i].val) ) {
            i++;
        }
        if( i == args->argl[argnum].nsel ) {
            // Unknown value
            xstrlcpy(human_string,val,maxlen);
            return -1;
        } else {
            xstrlcpy(human_string,args->argl[argnum].select[i].selectlabel,maxlen);
            return 0;
        }
    }
    xstrlcpy(human_string,val,maxlen);
    return 0;
}

/**
 * Get the corresponding server command from the device command
 * @param devcmd Device command
 * @param srvcmd Server command
 * @return -1 on failure, 0 on success
 */
int
get_srvcmd_from_devcmd(const char *devcmd, char *srvcmd, size_t maxlen) {
    *srvcmd = 0;
    size_t i = 0;
    while (i < g7command_list_len && xstricmp(devcmd, g7command_list[i].cmdstr)) {
        ++i;
    }
    if (i >= g7command_list_len)
        return -1;
    strncpy(srvcmd, g7command_list[i].srvcmd, maxlen - 1);
    return 0;
}

/**
 * Get the corresponding raw device command corresponding to the user level
 * command supplied as first argument
 * @param srvcmd User command name
 * @param maxlen Maxlen for devcmd buffer
 * @param devcmd Translated raw device command (out)
 * @return -1 on failure, 0 on success
 */
int
get_devcmd_from_srvcmd(const char *srvcmd, size_t maxlen, char *devcmd) {
    int idx = get_cmdidx_from_srvcmd(srvcmd);
    if (idx < 0) {
        return -1;
    }
    strncpy(devcmd, g7command_list[idx].cmdstr, maxlen - 1);
    devcmd[maxlen - 1] = '\0';
    return 0;
}


/**
 * Find the device command corresponding to the specified command index
 * @param idx Command index
 * @param devcmd Filled with the corresponding device command
 * @param maxlen Maximum buffer length for device command
 * @return 0 if command was found, -1 if the command index is out of range
 */
int
get_devcmd(size_t idx, char *devcmd, size_t maxlen) {

    if( idx < g7command_list_len ) {
        strncpy(devcmd, g7command_list[idx].cmdstr, maxlen - 1);
        devcmd[maxlen - 1] = '\0';
        return 0;
    }
    return -1;
}

/**
 * Internal helper. Write error string to client
 * @param sockd Socket to communicate with client on
 * @param cmd Faulty command
 * @param arg
 * @return -1
 */
static int
error_notsupported_cmd(const int sockd, const char *cmd, int arg) {
    _writef(sockd, "[ERR] Not supported command. Internal fault.\n");
    logmsg(LOG_CRIT, "Unrecognized argument command mode =%d for \"%s\"", arg, cmd);
    return -1;
}

/**
 * Prepare to send a command to the device by adding the necessary
 * prefix and device command name. The function is supplied with a string
 * (comma separated) with the actual argument to the device. Note that the
 * password should NOT be included as an argument.
 * @param sockd Socket to communicate with user on
 * @param cmdidx Command index for the command we are executing
 * @param mode Whether we are querying or writing the device
 * @param argstrlist A string with any necessary argument separated by ","
 * @return -1 on failure , 0 on success
 */
int
prepare_cmd_to_device(struct client_info *cli_info, const int cmdidx, const int mode, const char *argstrlist) {

    const int sockd = cli_info->cli_socket;
    
    char buffer[1024];
    char pinbuffer[8], tagbuffer[8];
    get_device_pin(pinbuffer, sizeof (pinbuffer));
    get_device_tag(tagbuffer, sizeof (tagbuffer));

    if (!(mode & g7command_list[cmdidx].modes)) {
        _writef(sockd, "[ERR] This command is not valid. See \"help %s\"\n", g7command_list[cmdidx].srvcmd);
        logmsg(LOG_ERR, "This command (%s) is not valid. mode=%d, command-modes=%d", g7command_list[cmdidx].srvcmd, mode, g7command_list[cmdidx].modes);
        return -1;
    }

    if (CMD_MODE_W == mode) {
        
        // Command prefix $WP+<cmd>[+<tag>]=<pwd>
        if (*tagbuffer) {
            snprintf(buffer, sizeof (buffer) - 1, "$WP+%s+%s=%s",
                    g7command_list[cmdidx].cmdstr, tagbuffer, pinbuffer);
        } else {
            snprintf(buffer, sizeof (buffer) - 1, "$WP+%s=%s",
                    g7command_list[cmdidx].cmdstr, pinbuffer);
        }
        if (argstrlist && *argstrlist) {
            strcat(buffer, ",");
            strncat(buffer, argstrlist, sizeof (buffer) - strlen(buffer) - 1);
        }
        
    } else {
        
        // Command prefix $WP+<cmd>[+<tag>]=<pwd>,?
        if (*tagbuffer) {
            if (argstrlist && *argstrlist) {
                snprintf(buffer, sizeof (buffer) - 1, "$WP+%s+%s=%s,%s,?", g7command_list[cmdidx].cmdstr, tagbuffer, pinbuffer, argstrlist);
            } else {
                snprintf(buffer, sizeof (buffer) - 1, "$WP+%s+%s=%s,?", g7command_list[cmdidx].cmdstr, tagbuffer, pinbuffer);
            }
        } else {
            if (argstrlist && *argstrlist) {
                snprintf(buffer, sizeof (buffer) - 1, "$WP+%s=%s,%s,?", g7command_list[cmdidx].cmdstr, pinbuffer, argstrlist);
            } else {
                snprintf(buffer, sizeof (buffer) - 1, "$WP+%s=%s,?", g7command_list[cmdidx].cmdstr, pinbuffer);
            }
        }
        
    }
    return send_rawcmd(cli_info, buffer, tagbuffer);
}

/**
 * Ask for and read a single argument from the user on specified socket.
 * This is the only state the daemon has since it will stay within the
 * command argument gathering mode until the user breaks out of the command
 * prematurely (with a  "." character) or the argument sequence is completed.
 * @param sockd Socket to communicate on
 * @param label Label to show user when asking fro input
 * @param maxb Maximum length of reply buffer
 * @param arg The string representing the returned argument
 * @return 0 on success, -1 on failure.
 */
int
get_arg(const int sockd, char *label, size_t maxb, char *arg) {
    // Give user a prompt and read argument.
    fd_set read_fdset;
    struct timeval timeout;
    int ret;

    *arg = '\0';
    if (*label)
        _writef(sockd, "%s", label);

    FD_ZERO(&read_fdset);
    FD_SET(sockd, &read_fdset);

    timerclear(&timeout);
    timeout.tv_sec = 300; // 5 min timeout to enter value
    ret = select(sockd + 1, &read_fdset, NULL, NULL, &timeout);
    if (0 == ret) {
        // Timeout
        logmsg(LOG_INFO, "Timeout for command argument \"%s\"", label);
        return -1;
    } else {
        char buffer[512];
        ssize_t len = socket_read(sockd, buffer, sizeof (buffer) - 1);
        if (0 == len) {
            // User has disconnected
            logmsg(LOG_DEBUG, "get_arg(): Read 0 bytes from socket. Returning USER_DISCONNECT");
            return USER_DISCONNECT;
        }

        if (len < 0) {
            logmsg(LOG_ERR, "Error reading command argument from user. ( %d : %s )", errno, strerror(errno));
            return -1;
        }
        buffer[len] = '\0';
        xstrtrim_crnl(buffer);
        logmsg(LOG_DEBUG, "get_arg(): Read \"%s\" (len=%zd) from socket", buffer, len);

        buffer[sizeof (buffer) - 1] = '\0';
        buffer[len] = 0;
        xstrtrim_crnl(buffer);
        len = strnlen(buffer, sizeof (buffer));
        if ((size_t) len > maxb) {
            return -1;
        } else {
            strncpy(arg, buffer, maxb);
            arg[maxb - 1] = '\0';
        }
    }
    return 0;
}

/**
 * Gather all necessary arguments for this command from the user
 * @param sockd Socket to communicate with the user on
 * @param cmdidx Command index for command
 * @param maxlen Maximum buffer length for argument reply buffer
 * @param argbuff Buffer holding all the read arguments
 * @return -1 on failure, 0 on success
 */
int
get_cmd_args(const int sockd, int cmdidx, size_t maxlen, char *argbuff) {

    int argidx = get_argidx_for_cmdidx(cmdidx);
    if (argidx < 0)
        return -1;
    struct cmdargs *p = &cmdargs_list[argidx];
    char reply[64];
    int rc = 0;
    *argbuff = '\0';
    for (size_t i = 0; i < p->numargs; i++) {

        char info[64] = {'\0'};
        int select_in_range = 0;
        _Bool isdigit = TRUE;

        switch (p->argl[i].type) {
            case ARGT_BOOL:
                // Ignore the "Enable/Disable" label since that parameter is
                // handled by the binary command template to reduce the mandatory
                // parameters for the user to handle.
                if (0 == strcmp(AUTO_ENABLE_LABEL, p->argl[i].arglabel)) {
                    strcpy(reply, "1");
                } else {
                    _writef(sockd, "%s - %s\n"
                            "   0 - Disable\n"
                            "   1 - Enable\n", p->argl[i].arglabel, p->argl[i].argdesc);
                    do {
                        rc = get_arg(sockd, "Select (0/1) ?\r\n", sizeof (reply), reply);
                        if (rc < 0)
                            return rc;
                        if (1 <= strlen(reply) && '.' == *reply) {
                            _writef(sockd, "[ERR] Input aborted.");
                            return -1;
                        }
                        select_in_range = (1 == strlen(reply) && ('0' == *reply || '1' == *reply));
                    } while (!select_in_range);
                }
                break;

                /* NOTE: For now we do not do any special check depending on the expected argument
                 * type. This can be seen as a preparation for future enhancements.
                 */
            case ARGT_STRING:
            case ARGT_FLOAT:
                _writef(sockd, "%s - %s?", p->argl[i].arglabel, p->argl[i].argdesc);
                rc = get_arg(sockd, "\r\n", sizeof (reply), reply);
                if (rc < 0)
                    return rc;
                if (1 <= strlen(reply) && '.' == *reply) {
                    _writef(sockd, "[ERR] Input aborted.");
                    return -1;
                }
                break;

            case ARGT_INT:
                isdigit = TRUE;
                do {
                    _writef(sockd, "%s - %s?", p->argl[i].arglabel, p->argl[i].argdesc);
                    rc = get_arg(sockd, "\r\n", sizeof (reply), reply);
                    if (rc < 0)
                        return rc;
                    if (! *reply)
                        break;
                    if (1 <= strlen(reply) && '.' == *reply) {
                        _writef(sockd, "[ERR] Input aborted.");
                        return -1;
                    }
                    size_t len = strlen(reply);
                    size_t j = 0;
                    while (j < len && isdigit) {
                        isdigit = reply[j] >= '0' && reply[j] <= '9';
                        if (!isdigit) {
                            logmsg(LOG_DEBUG, "!isdigit: j=%zd c=%d", j, (int) reply[j]);
                        }
                        ++j;
                    }
                    if (!isdigit) {
                        _writef(sockd, "Expected numeric value not \"%s\"\n", reply);
                    }
                } while (!isdigit);
                break;

            case ARGT_SELECT:
                _writef(sockd, "%s - %s\n", p->argl[i].arglabel, p->argl[i].argdesc);
                *info = '(';
                for (size_t j = 0; j < p->argl[i].nsel; j++) {
                    _writef(sockd, "  %s - %s\n", p->argl[i].select[j].val, p->argl[i].select[j].selectlabel);
                    strncat(info, p->argl[i].select[j].val, sizeof (info) - strlen(info) - 1);
                    if (j < p->argl[i].nsel - 1)
                        strcat(info, "/");
                }
                strcat(info, ")");
                do {
                    _writef(sockd, "Select %s?", info);
                    rc = get_arg(sockd, "\r\n", sizeof (reply), reply);
                    if (rc < 0)
                        return rc;
                    if (!*reply)
                        break;
                    if (1 <= strlen(reply) && '.' == *reply) {
                        _writef(sockd, "[ERR] Input aborted.");
                        return -1;
                    }
                    // Check value against all select value
                    select_in_range = 0;
                    for (size_t t = 0; t < p->argl[i].nsel && !select_in_range; t++) {
                        select_in_range = (0 == strcmp(reply, p->argl[i].select[t].val));
                    }
                    if (!select_in_range) {
                        _writef(sockd, "Value \"%s\" outside permissable range.\n", reply);
                    }
                } while (!select_in_range);
                break;
            default:
                return -1;
        }
        if (strlen(argbuff) + strlen(reply) + 1 >= maxlen) {
            return -1;
        }
        if (i > 0)
            strcat(argbuff, ",");
        strcat(argbuff, reply);
    }

    return 0;
}

/*============================================================================
 * Execute user commands
 *============================================================================
 */

#define INVALID_BINARY_CMD "Command \"%s\" is not valid. Try help."

/**
 * Execute a binary command from user. A binary command is a command that
 * specifies a device state to be on or off
 * @param field Parsed fields in command string
 * @param sockd Socket to write client on
 * @return -1 on failure, 0 on success
 */
int
exec_binary_command(struct client_info *cli_info, char **field) {
    const int sockd = cli_info->cli_socket;
    // We have found one of the binary commands
    size_t i = 0;
    ptrcmd cmdf = NULL;

    while (i < g7command_list_len) {
        if (CMD_TYPE_GETSET_BINARY == g7command_list[i].type && 0 == xstricmp(field[1], g7command_list[i].srvcmd)) {
            break;
            ;
        }
        ++i;
    }
    if (i < g7command_list_len) {
        // Found binary command
        cmdf = g7command_list[i].cmdhandler;
        return cmdf(cli_info, i, (0 == xstricmp(field[2], "on")) ? 1 : 2);
    } else {
        char msg[256];
        snprintf(msg, sizeof (msg), INVALID_BINARY_CMD, field[1]);
        logmsg(LOG_ERR, "%s", msg);
        _writef(sockd, "[ERR] %s", msg);
    }
    return 0;
}

#define INVALID_CMD_MSG_GET "Command \"get %s\" is not valid. Try help."

/**
 * Execute a "get" command that gets some information from the device
 * @param field Parsed fields in command string
 * @param sockd Socket to write client on
 * @return -1 on failure, 0 on success
 */
int
exec_get_command(struct client_info *cli_info, char **field) {
    const int sockd = cli_info->cli_socket;
    // We have found one of the binary commands
    size_t i = 0;
    ptrcmd cmdf = NULL;

    while (i < g7command_list_len) {
        if ((CMD_TYPE_GET == g7command_list[i].type ||
                CMD_TYPE_GETSET_BINARY == g7command_list[i].type ||
                CMD_TYPE_GETSET == g7command_list[i].type) &&
                0 == xstricmp(field[1], g7command_list[i].srvcmd)) {
            break;
        }
        ++i;
    }
    if (i < g7command_list_len) {
        // Found get command
        cmdf = g7command_list[i].cmdhandler;
        return cmdf(cli_info, i, 0);
    } else {
        char msg[256];
        snprintf(msg, sizeof (msg), INVALID_CMD_MSG_GET, field[1]);
        logmsg(LOG_ERR, "%s", msg);
        _writef(sockd, "[ERR] %s", msg);
    }
    return 0;
}

/**
 * Return the google map string in the supplied url buffer. It is the
 * calling routines responsibility to make sure that the url is long
 * enough to hold the constructed google string.
 * NOTE: Google maps uses the common convention of lat,lon while the
 * device returns lon,lat !! (This will definitely put someone on the
 * other side of the globe occasionally)
 * @param[in] lat Latitude string 8 characters
 * @param[in] lon Longitude string 8 characters
 * @param[out] url
 * @param[in] maxlen maximum length of url
 * @return 0 on success, -1 on failure
 */
int
get_googlemap_string(char *lat, char *lon, char *url, size_t maxlen) {

    *url = '\0';
    if (8 <= strlen(lat) && 8 <= strlen(lon)) {
        snprintf(url, maxlen, "https://www.google.com/maps?q=%s,%s", lat, lon);
        return 0;
    } else {
        logmsg(LOG_ERR, "Internal error lat (%s) or lon (%s) string from device not as expected!", lat, lon);
        return -1;
    }
}

/**
 * Read the location from the device and translate it to a street address using
 * Google map services
 * @param cli_info Client context
 * @param address String to write address to
 * @param maxaddress Maximum string length
 * @return 0 on success, -1 on failure
 */
int
exec_get_address(struct client_info *cli_info, char *address, const size_t maxaddress)  {
    char reply[256];
    if (-1 == send_cmdquery_reply(cli_info, "loc", reply, sizeof (reply))) {
        logmsg(LOG_ERR, "Internal error cannot execute command 'loc'");
        return -1;        
    }    
    // 3:rd and 4:th fields are the lon & lat so extract them from the device reply
    struct splitfields flds;
    int rc = xstrsplitfields(reply, LOC_DELIM, &flds);
    if (0 == rc) {
        
        rc = get_address_from_latlon(flds.fld[3], flds.fld[2], address, maxaddress);
        if (0 == rc) {
            logmsg(LOG_DEBUG, "Found address: %s", address);
        } else {
            logmsg(LOG_ERR, "Failed to find address");
            return -1;
        }
    } else {
        logmsg(LOG_ERR,"Cannot parse returned device string \"%s\"",reply);
        return -1;
    }
    return 0;    
}

/**
 * Special handling for the extended "get loc" command which is "get locg"
 * which will get the last location from device as a google map string.
 * The command will first return the normal result from get loc and then
 * on the next line (separated by a single "\n" write the URL for a
 * google map line
 * @param cli_info Client context
 * @return 0 on success, -1 on failure
 */
int
exec_get_locg(struct client_info *cli_info) {
    const int sockd = cli_info->cli_socket;
    char reply[256];
    if (-1 == send_cmdquery_reply(cli_info, "locg", reply, sizeof (reply))) {
        _writef(sockd, "[ERR] Internal error cannot execute command");
        return -1;
    }
    // 3:rd and 4:th fields are the lon & lat so extract them from the device reply
    struct splitfields flds;
    int rc = xstrsplitfields(reply, LOC_DELIM, &flds);
    if (0 == rc) {
        char mapstr[128];
        rc = get_googlemap_string(flds.fld[3], flds.fld[2], mapstr, sizeof (mapstr));
        if (0 == rc) {
            _writef(sockd, "%s%s", DEVICE_REPLY_PROMPT, mapstr);
        } else {
            _writef(sockd, "[ERR] Failed to get location");
            return -1;
        }
    } else {
        logmsg(LOG_ERR,"Cannot parse returned device string \"%s\"",reply);
        _writef(sockd, "[ERR] Failed to get location");
        return -1;
    }
    return 0;
}

/**
 * Handle the "get gfevt EVTID". This is a special case of the get structure since this
 * is the only get command that requires an extra compulsive argument, the event id
 * to get information about
 * @param field The parsed field of the command string field[1] is assumed to
 * contain the event ID
 * @param sockd Client socket
 * @return o 0n success, -1 on failure (event id out of range)
 */
int
exec_get_gfevt(struct client_info *cli_info, char **field) {
    const int sockd = cli_info->cli_socket;
    int mode = CMD_MODE_R;
    int evtid = xatoi(field[1]);
    if (evtid < 50 || evtid > 99) {
        _writef(sockd, "[ERR] Event ID must be in range [50,99]");
        return -1;
    }
    return prepare_cmd_to_device(cli_info, CMDIDX_SETEVT, mode, field[1]);
    return 0;
}

#define INVALID_CMD_MSG_SET "Command \"set %s\" is not valid. Try help."

/**
 * Execute a "set" command that sends data to the device
 * @param field Parsed fields in command string
 * @param sockd Socket to write client on
 * @return -1 on failure, 0 on success
 */
int
exec_set_command(struct client_info *cli_info, char **field) {
    // We have found one of the binary commands
    const int sockd = cli_info->cli_socket;
    size_t i = 0;
    ptrcmd cmdf = NULL;

    while (i < g7command_list_len) {
        if ((CMD_TYPE_SET == g7command_list[i].type || CMD_TYPE_GETSET == g7command_list[i].type) &&
                0 == xstricmp(field[1], g7command_list[i].srvcmd)) {
            break;
        }
        ++i;
    }
    if (i < g7command_list_len) {
        // Found set command
        cmdf = g7command_list[i].cmdhandler;
        return cmdf(cli_info, i, 1);
    } else {
        char msg[256];
        snprintf(msg, sizeof (msg), INVALID_CMD_MSG_SET, field[1]);
        logmsg(LOG_ERR, "%s", msg);
        _writef(sockd, "[ERR] %s", msg);
    }
    return 0;
}

#define INVALID_CMD_MSG_DO "Command \"do %s\" is not valid. Try help."

/**
 * Execute a command with no arguments on the device
 * @param field Parsed fields in command string
 * @param sockd Socket to write client on
 * @return -1 on failure, 0 on success
 */
int
exec_do_command(struct client_info *cli_info, char **field) {
    // We have a "do" command
    const int sockd = cli_info->cli_socket;
    size_t i = 0;
    ptrcmd cmdf = NULL;

    while (i < g7command_list_len) {
        if (CMD_TYPE_DO == g7command_list[i].type &&
                0 == xstricmp(field[1], g7command_list[i].srvcmd)) {
            break;
        }
        ++i;
    }
    if (i < g7command_list_len) {
        // Found set command
        cmdf = g7command_list[i].cmdhandler;
        return cmdf(cli_info, i, 0);
    } else {
        char msg[256];
        snprintf(msg, sizeof (msg), INVALID_CMD_MSG_DO, field[1]);
        logmsg(LOG_ERR, "%s", msg);
        _writef(sockd, "[ERR] %s", msg);
    }
    return 0;
}

/**
 * Temporary structure used to sort commands in on-line help list
 */
struct cmdhlplist_t {
    const char *cmd; // Client command name
    const char *cmdhelp; // Short description
};

/**
 * Sorting callback for qsort
 * @param a First record to compare
 * @param b Second record to compare
 * @return -1 if a < b , 0 if a==b, 1 if a > b
 */
static int
_qsort_help_for_cmd(const void *a, const void *b) {
    return strcmp(
            ((struct cmdhlplist_t *) a)->cmd,
            ((struct cmdhlplist_t *) b)->cmd);
}

/**
 * Print a list of all commands with a one line explanation
 * @param sockd Socket to write client on
 */
void
exec_help_commandlist(struct client_info *cli_info) {
    const int sockd = cli_info->cli_socket;
    logmsg(LOG_DEBUG, "Creating help list");
    _writef(sockd, "Device Command list:\n--------------------\n");

    // read command names first to sort the list alphabetically
    struct cmdhlplist_t *cmdhlplist = (struct cmdhlplist_t *) calloc(g7command_list_len, sizeof (struct cmdhlplist_t));
    if (NULL == cmdhlplist) {
        logmsg(LOG_CRIT, "Out of memory allocation structure for help!");
        return;
    }
    for (size_t i = 0; i < g7command_list_len; ++i) {
        cmdhlplist[i].cmd = g7command_list[i].srvcmd;
        cmdhlplist[i].cmdhelp = g7command_list[i].descrshort;
    }
    qsort(cmdhlplist, g7command_list_len, sizeof (struct cmdhlplist_t), _qsort_help_for_cmd);
    size_t i = 0;
    while (i < g7command_list_len) {
        _writef(sockd, "%-9s - %s\n", cmdhlplist[i].cmd, cmdhlplist[i].cmdhelp);
        ++i;
    }
    free(cmdhlplist);

    _writef(sockd, "\n");
    _writef(sockd, "Use \"help <command>\" for detailed help on each command.\n");
    _writef(sockd, "\n");
    _writef(sockd, "Server Command list:\n--------------------\n");
    _writef(sockd, "help                   - Print help for all commands\n");
    _writef(sockd, ".address               - Toggle address lookup when storing locations\n");
    _writef(sockd, ".cachestat             - Display statistics for the Geolocation cache\n");
    _writef(sockd, ".date                  - Display server date and time\n");    
    _writef(sockd, ".dn                    - Delete specified nick\n");    
    _writef(sockd, ".lc                    - List command connections\n");
    _writef(sockd, ".ld                    - List all devices connections (on USB and GPRS)\n");
    _writef(sockd, ".ln                    - List all registered nicks\n");
    _writef(sockd, ".nick                  - Register a nick-name for connected device\n");    
    _writef(sockd, ".ratereset             - Reset Geolocation lookup rate suspension\n");
    _writef(sockd, ".report                - Generate a PDF report of connected device to specified file\n");
    _writef(sockd, ".table                 - Switch between ASCII and Unicode box drawing characters for output tables\n");
    _writef(sockd, ".target                - List devices connected over GPRS or set active GPRS connection\n");    
    _writef(sockd, ".usb                   - List devices on USB or set active USB connection\n");
    _writef(sockd, ".ver                   - Give version information of server\n");    


    _writef(sockd, "\nDB Command list:\n----------------\n");
    _writef(sockd, "db dist                - Calculate the approximate distance from a set of chosen locations\n");
    _writef(sockd, "db deletelocations     - Clear all stored locations\n");
    _writef(sockd, "db export              - Export selected locations in chosen format to file\n");
    _writef(sockd, "db head                - Display the newest received locations\n");    
    _writef(sockd, "db lastloc             - Retrieve the last stored location in the DB\n");
    _writef(sockd, "db mailpos             - Mail the last stored location in the DB\n");
    _writef(sockd, "db mailcsv             - Export the database in CSV format and mail as compressed attachment\n");
    _writef(sockd, "db mailgpx             - Export the database in GPX format and mail as compressed attachment\n");
    _writef(sockd, "db size                - Return number of location events in DB\n");
    _writef(sockd, "db tail                - Display the oldest received locations\n");
    _writef(sockd, "db sort [device|arrival] - Set sort order for db head & tail commands\n");
    
    _writef(sockd, "\nPreset command list:\n----------------\n");
    _writef(sockd, "preset list            - List all read preset files with short description\n");
    _writef(sockd, "preset refresh         - Re-read all preset files from disk\n");
    _writef(sockd, "preset use <preset>    - Use (execute) the specified preset (\"@<preset>\" as shortform)\n");
    _writef(sockd, "preset help <preset>   - Give more detailed help on the specified preset\n");
    _writef(sockd, "\n");

}

/**
 * Print detailed help for a specific command
 * @param sockd Socket to write client on
 * @param cmd_name Command name
 */
void
exec_help_for_command(struct client_info *cli_info, char *cmd_name) {
    const int sockd = cli_info->cli_socket;
    size_t i = 0;
    logmsg(LOG_DEBUG, "Creating help for %s", cmd_name);
    while (i < g7command_list_len) {
        if (0 == xstricmp(cmd_name, g7command_list[i].srvcmd)) {
            break;
        }
        ++i;
    }
    if (i < g7command_list_len) {

        // Send back short help
        _writef(sockd, "NAME:\n  %s - %s\n\n", g7command_list[i].srvcmd, g7command_list[i].descrshort);

        // Send back command synopsis
        // 1=get, 2=set, 4=get/set binary, 8=get/set, 16=execute
        char *synopsis = "";
        _writef(sockd, "SYNOPSIS:\n");

        size_t nargs = 1;
        switch (g7command_list[i].type) {
            case 1:
                synopsis = "  get %s\n";
                break;
            case 2:
                synopsis = "  set %s\n";
                break;
            case 4:
                synopsis = "  set %s [on|off]\n"
                        "  get %s\n";
                nargs = 2;
                break;
            case 8:
                synopsis = "  [get|set] %s\n";
                break;
            case 16:
                synopsis = "  do %s\n";
                break;
            default:
                synopsis = "";
                logmsg(LOG_CRIT, "Unrecognized command type in send_help_for_command()");
                break;
        }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        if (1 == nargs) {
            _writef(sockd, synopsis, g7command_list[i].srvcmd);
        } else {
            _writef(sockd, synopsis, g7command_list[i].srvcmd, g7command_list[i].srvcmd);
        }
#pragma clang diagnostic pop
#pragma GCC diagnostic pop

        _writef(sockd, "\n");
        // List all arguments for 2,4,8
        if (g7command_list[i].type & (CMD_TYPE_GETSET | CMD_TYPE_GETSET_BINARY | CMD_TYPE_SET)) {
            _writef(sockd, "\nARGUMENTS:\n");
            int argidx = get_argidx_for_cmdidx(i);
            if (argidx >= 0) {
                const size_t numargs = cmdargs_list[argidx].numargs;
                for (size_t j = 0; j < numargs; ++j) {
                    _writef(sockd, "  %-19s - %s\n", cmdargs_list[argidx].argl[j].arglabel, cmdargs_list[argidx].argl[j].argdesc);
                }
            }
            _writef(sockd, "\n\n");
        }

        _writef(sockd, "DESCRIPTION:\n");
        _writef(sockd, "Long description not yet available.\n");

    } else {
        logmsg(LOG_ERR, "\"%s\" is not valid command.", cmd_name);
        _writef(sockd, "[ERR] \"%s\" is not valid command.", cmd_name);
    }
}

/**
 * Read the battery voltage from a connected device. It is the calling functions
 * responsibility that the response buffer is at least 6 chars long
 * @param[out] battvolt Battery voltage as string
 * @return 0 on success, -1 n failure
 */
int
cmd_get_dev_batt(struct client_info *cli_info, char *battvolt) {
    // We use the loc command to read the battery voltage
    // The response string from device is:
    // 3000000001,20140107232526,17.961028,59.366470,0,0,0,0,1,4.20V,0
    char reply[128];
    *battvolt = '\0';
    int rc = send_cmdquery_reply(cli_info, "loc", reply, sizeof (reply));
    if (!rc) {
        // Extract the 10:th field
        struct splitfields flds;
        rc = xstrsplitfields(reply, ',', &flds);
        if (!rc) {
            if (flds.nf > 9) {
                strncpy(battvolt, flds.fld[9], 5);
            }
        }
    }

    if (rc) {
        logmsg(LOG_ERR, "Failed to read battery voltage from device");
    }
    return rc;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/**
 * Command function for "get address"
 * @param cli_info Client context
 * @param cmdidx  Dummy flag to match the generic command prototype
 * @param gsflag  Dummy flag to match the generic command prototype
 * @return 0 on success, -1 on failure
 */
int
exec_cmd_address(struct client_info *cli_info, const int cmdidx, const int gsflag) {
    const int sockd = cli_info->cli_socket;
    char address[256];
    int rc = exec_get_address(cli_info,address,sizeof(address));
    if (-1 == rc) {
        _writef(sockd, "[ERR] Failed to get address");
    } else {
        _writef(sockd, "%s%s", DEVICE_REPLY_PROMPT, address);
    }
    return rc;
}


/**
 * Client command to read battery voltage from device
 * @param sockd   Client socket
 * @param cmdidx  Dummy flag to match the generic command prototype
 * @param gsflag  Dummy flag to match the generic command prototype
 * @return 0 on success, -1 on failure
 */
int
exec_cmd_batt(struct client_info *cli_info, const int cmdidx, const int gsflag) {
    const int sockd = cli_info->cli_socket;
    char volt[6];
    int rc = cmd_get_dev_batt(cli_info, volt);
    if (!rc) {
        _writef(sockd, "%s%s", DEVICE_REPLY_PROMPT, volt);
    } else {
        _writef(sockd, "[ERR] Failed to read battery voltage.");
    }
    return rc;
}

#pragma GCC diagnostic pop

/**
 * Execute a native command for the device given by the client.
 * A native (raw) command does
 * always start with a "$WP+" prefix. To allow user to send native command
 * it must first be enabled in the config file
 * @param sockd Socket to communicate with client on
 * @param cmdstr The actual command string
 */
void
exec_native_command(struct client_info *cli_info, const char *cmdstr) {
    const int sockd = cli_info->cli_socket;
    // This is a raw command that we send strait through to the device
    if (enable_raw_device_commands) {
        (void) send_rawcmd(cli_info, cmdstr, "");
    } else {
        _writef(sockd, "[ERR] Native command sending NOT enabled by administrator.");
    }
}

/**
 * Read all internally recorded locations stored in the device internal
 * memory and store them in the database. After a successful read the
 * device internal memory is cleared.
 * @param sockd Socket used for user communication
 * @param cmdidx Dummy argument to follow the command prototype
 * @param gsflag Get/Set flag not used to follow the command prototype
 * @return 0 on success, -1 on failure
 */
int
exec_dlrec(struct client_info *cli_info, const int cmdidx, const int gsflag) {
    
    logmsg(LOG_DEBUG, "Sending device command to read internal stored locations CMDIDX=%d, GSFLAG=%d", cmdidx, gsflag);
    return get_dlrec_to_db(cli_info);
}

/**
 * Read a password from the user and check it
 * @param sockd Socket to write client on
 * @return -1 on authentication, -1 on failure
 */
int
check_password(int sockd) {
    static const char *AUTH_FAIL_MSG = "Authentication failed. Connection closed.";
    static const char *PWD_LBL_MSG = "Password: ";
    fd_set read_fdset;
    struct timeval timeout;
    int authenticated = 0, ret;

    if (require_client_pwd) {
        int tries = 3;

        while (tries > 0 && !authenticated) {
            _writef(sockd, "%s\r\n", PWD_LBL_MSG);

            FD_ZERO(&read_fdset);
            FD_SET(sockd, &read_fdset);

            timerclear(&timeout);
            timeout.tv_sec = 120; // 2 min timeout to give a password

            ret = select(sockd + 1, &read_fdset, NULL, NULL, &timeout);
            if (0 == ret) {
                // Timeout
                logmsg(LOG_DEBUG, "Timeout for password query on socket %d", sockd);
                break;
            } else {
                char buffer[512];
                int numreads = socket_read(sockd, buffer, sizeof (buffer) - 1);
                buffer[sizeof (buffer) - 1] = '\0';
                buffer[numreads] = 0;
                xstrtrim_crnl(buffer);
                authenticated = (strcmp(buffer, client_pwd) == 0);
            }
            --tries;

        }
        if (!authenticated) {
            logmsg(LOG_INFO, "%s", AUTH_FAIL_MSG);
            _writef(sockd, "%s\r\n", AUTH_FAIL_MSG);
            return -1;
        } else {
            logmsg(LOG_INFO, "User successfully authenticated");
        }
    }
    return 0;
}

/**
 * This is the main entry point for interpretating a user command. This is
 * simplistically done by trying to match a number of REGEXP (using PCRE3)
 * until a match is found for a class of commands. The actual execution of
 * the command is then dispatch to the function that handles this command
 * class.
 * @param cmdstr The raw user command given
 * @param cli_info Information structure for the connecting client
 * @return -1 on failure, 0 on success
 */
int
cmdinterp(char *cmdstr, struct client_info *cli_info) {
    // Command format is one of three
    // 1. Enabling/Disabling functions
    //    switch <function name> on/off
    //    toggle <function name>
    //
    // 2. Parameter setting/getting
    //    set <function name>
    //    get <function name>
    //
    // 3. System commands with no arguments
    //    do <function name>
    //
    // In addition it is possible to get help in two ways
    //
    // 1. help                 - Will give a list of all commands with a
    //                           one line description        // User asked for help
    //
    // 2. help <function name> - Give details help of a specific function
    //                           including description of all arguments.
    //
    // TODO: Binary command should be possible without the trailing "on" and then default to enable
    char **field = (void *) NULL;
    int rc = 0, nf = 0;

    if (0 < matchcmd("^set" _PR_S _PR_AN _PR_S _PR_ONOFF _PR_E, cmdstr, &field)) {
        rc = exec_binary_command(cli_info, field);
    } else if (0 < matchcmd("^get locg" _PR_E, cmdstr, &field)) {
        rc = exec_get_locg(cli_info);
//    } else if (0 < matchcmd("^get address" _PR_E, cmdstr, &field)) {
//        rc = exec_get_address(cli_info);        
    } else if (0 < matchcmd("^get gfevt" _PR_S _PR_N _PR_E, cmdstr, &field)) {
        rc = exec_get_gfevt(cli_info, field);
    } else if (0 < matchcmd("^get" _PR_S _PR_AN _PR_E, cmdstr, &field)) {
        rc = exec_get_command(cli_info, field); // Parameter getting
    } else if (0 < matchcmd("^set" _PR_S _PR_AN _PR_E, cmdstr, &field)) {
        rc = exec_set_command(cli_info, field); // Parameter setting
    } else if (0 < matchcmd("^do" _PR_S _PR_AN _PR_E, cmdstr, &field)) {
        rc = exec_do_command(cli_info, field); // System command/ Command without arguments
    } else if (0 < matchcmd("^help \\." _PR_AN _PR_E, cmdstr, &field)) {
        srvcmd_help(cli_info, field);
    } else if (0 < matchcmd("^help db" _PR_S _PR_AN _PR_E, cmdstr, &field)) {
        db_help(cli_info, field);
    } else if (0 < matchcmd("^help" _PR_E, cmdstr, &field)) {
        exec_help_commandlist(cli_info);
    } else if (0 < matchcmd("^help" _PR_S _PR_AN _PR_E, cmdstr, &field)) {
        exec_help_for_command(cli_info, field[1]);
    } else if (0 < (nf = matchcmd("^\\." _PR_ANPSO _PR_E, cmdstr, &field))) {
        exec_srv_command(cli_info, cmdstr);
    } else if (0 < matchcmd("^\\$WP\\+" _PR_ANP _PR_E, cmdstr, &field)) {
        exec_native_command(cli_info, cmdstr);
    } else if (0 < (nf = matchcmd("^db" _PR_S "export" _PR_SO _PR_OPDEVID _PR_OPEVENTID _PR_OPTOFROMDATE _PR_SO "(kml|gpx|xml|csv|json)" "(" _PR_SO _PR_FNAMEOEXT ")?" _PR_E, cmdstr, &field))) {
        rc = exportdb_to_external_format(cli_info, nf, field);
    } else if (0 < (nf = matchcmd("^db" _PR_S "dist" _PR_SO _PR_OPDEVID _PR_OPEVENTID _PR_OPTOFROMDATE _PR_E, cmdstr, &field))) {
        rc = db_calc_distance(cli_info, nf, field);
    } else if (0 < matchcmd("^db mailgpx" _PR_E, cmdstr, &field)) {
        rc = mail_gpx_attachment(cli_info);
    } else if (0 < matchcmd("^db mailcsv" _PR_E, cmdstr, &field)) {
        rc = mail_csv_attachment(cli_info);
    } else if (0 < matchcmd("^db mailpos" _PR_E, cmdstr, &field)) {
        rc = mail_lastloc(cli_info);
    } else if (0 < matchcmd("^db size" _PR_E, cmdstr, &field)) {
        rc = db_get_numevents(cli_info);
    } else if (0 < matchcmd("^db lastloc" _PR_E, cmdstr, &field)) {
        rc = db_lastloc(cli_info);
    } else if (0 < matchcmd("^db head" _PR_E, cmdstr, &field)) {
        rc = db_head(cli_info,10);       
    } else if (0 < matchcmd("^db head" _PR_S _PR_N _PR_E, cmdstr, &field)) {        
        rc = db_head(cli_info,xatoi(field[1]));               
    } else if (0 < matchcmd("^db tail" _PR_E, cmdstr, &field)) {
        rc = db_tail(cli_info,10);                
    } else if (0 < matchcmd("^db tail" _PR_S _PR_N _PR_E, cmdstr, &field)) {        
        rc = db_tail(cli_info,xatoi(field[1]));                       
    } else if (0 < matchcmd("^db sort device" _PR_E, cmdstr, &field)) {        
        db_set_sortorder(SORT_DEVICETIME);
        _writef(cli_info->cli_socket, "Table sort order: device");
    } else if (0 < matchcmd("^db sort arrival" _PR_E, cmdstr, &field)) {        
        db_set_sortorder(SORT_ARRIVALTIME);        
        _writef(cli_info->cli_socket, "Table sort order: arrival");
    } else if(0 < matchcmd("^db sort" _PR_E, cmdstr, &field)) {
        _writef(cli_info->cli_socket, "Sort order: %s",db_get_sortorder_string());
    } else if (0 < matchcmd("^db deletelocations" _PR_E, cmdstr, &field)) {
        rc = db_empty_loc(cli_info);
    } else if (0 < (nf = matchcmd("^preset" _PR_S "(list|refresh)" _PR_E, cmdstr, &field))) {
        rc = commandPreset(cli_info, cmdstr, nf, field);
    } else if (0 < (nf = matchcmd("^preset" _PR_S "(use|help)" _PR_S _PR_AN _PR_E, cmdstr, &field))) {
        rc = commandPreset(cli_info, cmdstr, nf, field);
    } else if (0 < (nf = matchcmd("^@@" _PR_ANF _PR_E, cmdstr, &field))) {        
        // Execute a function directly with the same syntax as presets read from file
        // but here they are read directly from the commands
        _DBG_REGFLD(nf,field); 
        // Strip the leading "@@" and send the rest of the string for execution by the preset
        // function
        char pinbuff[8],tagbuff[8];
        get_device_pin(pinbuff,sizeof(pinbuff));
        get_device_tag(tagbuff,sizeof(tagbuff));
        rc = execPresetFunc(cli_info, cmdstr+2, tagbuff, pinbuff);
        if( 0==rc ) 
            _writef(cli_info->cli_socket, "OK.");
        else
            _writef(cli_info->cli_socket, "FAILED function \"%s\"",field[1]);
    } else if (0 < (nf = matchcmd("^@" _PR_AN _PR_E, cmdstr, &field))) {
        // Short form for "preset use dummy == @dummy
        char *tmpField[3];
        tmpField[0] = (char *) calloc(32, 1);
        tmpField[1] = (char *) calloc(32, 1);
        tmpField[2] = (char *) calloc(32, 1);
        snprintf(tmpField[0], 32, "preset use %s", field[1]);
        snprintf(tmpField[1], 32, "use");
        snprintf(tmpField[2], 32, "%s", field[1]);
        rc = commandPreset(cli_info, cmdstr, 3, tmpField);
        free(tmpField[0]);
        free(tmpField[1]);
        free(tmpField[2]);
    } else {
        _writef(cli_info->cli_socket, "[ERR] Command \"%s\" does not exist.",cmdstr);
    }
    matchcmd_free(&field);
    return rc;
}

/**
 * This is the thread entry point that gets started for each client that
 * connects to us. It is started from the worker main thread that is listening
 * to the command port
 * The passed argument is the socket the client is connected on and is passed
 * on by the socket server listener in the main thread.
 *
 * @param arg Cast to the socket that the client is connecting on
 * @return This is a thread entry point and does not return in the normal sense.
 */
void *
cmd_clientsrv(void *arg) {
    int rc;
    ssize_t numCharsFromClient;
    struct client_info *cli_info = (struct client_info *) arg;
    fd_set read_fdset;
    struct timeval timeout;
    char *readClientBuffer = _chk_calloc_exit(BUFFER_10K + 1);

    // To avoid reserving ~8MB after the thread terminates we
    // detach it. Without doing this the pthreads library would keep
    // the exit status until the thread is joined (or detached) which would mean
    // loosing 8MB for each created thread.
    pthread_detach(pthread_self());

    //char device_info[256];
    //_Bool last_usb_state = is_usb_connected();
    //if ( last_usb_state ) {
    //    snprintf(device_info, sizeof (device_info), "USB connection detected. Device commands ARE available.");
    //} else {
    //    snprintf(device_info, sizeof (device_info), "USB connection NOT detected. Device commands NOT available.");
    //}
    /*
    snprintf(buffer, sizeof(buffer)-1, "GM7 Server version %s (build %lu-%lu)\n"
                           "Existing clients: %d, Maximum number of clients: %lu\n"
                           "Automatic logout after %d minutes of idle time\n"
                           "%s\n\n",
                           PACKAGE_VERSION, (unsigned long) &__BUILD_DATE, (unsigned long) &__BUILD_NUMBER,
                           ncli_threads, max_clients, max_idle_time / 60,
                           device_info);
     */
    logmsg(LOG_DEBUG, "cli_socket=%d", cli_info->cli_socket);
#ifdef __APPLE__
    _writef(cli_info->cli_socket, "GM7 Server version %s \nType \"help\" to list available commands.\n", PACKAGE_VERSION);
#else
    _writef(cli_info->cli_socket, "GM7 Server version %s (build %lu-%lu)\nType \"help\" to list available commands.\n",
            PACKAGE_VERSION, (unsigned long) &__BUILD_DATE, (unsigned long) &__BUILD_NUMBER);
#endif
    _writef(cli_info->cli_socket, "\r\n");

    // Keep track of client idle time
    unsigned idle_time = 0;

    do {

        FD_ZERO(&read_fdset);
        FD_SET((unsigned) cli_info->cli_socket, &read_fdset);

        timerclear(&timeout);
        timeout.tv_sec = 1;

        // Wait for user to give command
        errno = 0;
        rc = select(cli_info->cli_socket + 1, &read_fdset, NULL, NULL, &timeout);
        if (rc == 0) {
            // Use the timeout as opportunity to give information about changed state of any
            // device connected over USB.
            //_Bool _usb = is_usb_connected();
            //if (last_usb_state != _usb ) {
            //    last_usb_state = _usb;
            //}
            //Timeout
            idle_time += 1;

            if (idle_time >= max_idle_time) {
                numCharsFromClient = -1; // Force a disconnect
                logmsg(LOG_INFO, "Client disconnected after being idle for more than %d seconds.", max_idle_time);
            } else {
                numCharsFromClient = 1; // To keep the loop going
            }

        } else if (FD_ISSET(cli_info->cli_socket, &read_fdset)) {

            // User has typed a command. Read it and do processing.
            idle_time = 0;
            numCharsFromClient = socket_read(cli_info->cli_socket, readClientBuffer, BUFFER_10K);
            if (numCharsFromClient > 0) {
                // When the remote socket closes the read will also succeed but return 0 read bytes
                // and there is no point in doing anything then.

                readClientBuffer[numCharsFromClient] = '\0';
                xstrtrim_crnl(readClientBuffer);

                if (0 == strcmp("exit", readClientBuffer) || 0 == strcmp("quit", readClientBuffer)) {
                    // Exit command. Exit the loop and close the socket
                    _writef(cli_info->cli_socket, "Bye.\n");
                    numCharsFromClient = -1;
                    break;

                } else {
                    if (*readClientBuffer) {
                        logmsg(LOG_DEBUG, "Client IP [%s] , cmd = \"%s\" (len=%zd)", cli_info->cli_ipadr, readClientBuffer, strlen(readClientBuffer));
                        rc = cmdinterp(readClientBuffer, cli_info);
                        if (USER_DISCONNECT == rc) {
                            numCharsFromClient = 0;
                        }
                    }
                    // All responses was sent in the cmdinterp() so tell the client all data
                    // was sent for now by sending the return-new-line                    
                    _writef(cli_info->cli_socket, "\r\n");
                }
            }
        } else {
            logmsg(LOG_CRIT, "KERNEL error. select() claims file ready when it is not!");
            numCharsFromClient = 1;
        }
    } while (numCharsFromClient > 0);

    logmsg(LOG_INFO, "Connection from %s on socket %d closed.", cli_info->cli_ipadr, cli_info->cli_socket);

    // Now clean up the data structures that keeps track on connected clients
    // Since these are global structures modifiable by all connected client they must be mutex protected.
    pthread_mutex_lock(&socks_mutex);

    if (-1 == _dbg_close(cli_info->cli_socket)) {
        logmsg(LOG_ERR, "Failed to close socket %d to client %s. ( %d : %s )",
                cli_info->cli_socket, cli_info->cli_ipadr, errno, strerror(errno));
    }
    
    // Clear the client info structure to 0 for all fields
    memset(cli_info, 0, sizeof (struct client_info));
    cli_info->target_cli_idx = -1;
    cli_info->target_usb_idx = -1;
    num_clients--;

    pthread_mutex_unlock(&socks_mutex);
    free(readClientBuffer);

    pthread_exit(NULL);
    return (void *) 0;
}

/*EOF*/
