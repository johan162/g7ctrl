/* =========================================================================
 * File:        g7srvcmd.c
 * Description: Handle all commands relating to the server itself. The
 *              client give these commands with an initial '.'
 * Author:      Johan Persson (johan162@gmail.com)
 *
 * Copyright (C) 2013-2015 Johan Persson
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
#include <syslog.h>
#include <errno.h>
#include <sqlite3.h>
#include <signal.h>
#include <pthread.h>


#include "build.h"
#include "config.h"
#include "g7config.h"
#include "g7ctrl.h"
#include "connwatcher.h"
#include "g7cmd.h"
#include "xstr.h"
#include "g7sendcmd.h"
#include "dbcmd.h"
#include "logger.h"
#include "utils.h"
#include "nicks.h"
#include "wreply.h"
#include "unicode_tbl.h"
#include "geoloc.h"
#include "mailutil.h"
#include "g7pdf_report_view.h"


/**
 * Structure to store the help text for the DB commands
 */
struct srvcmd_help {
    char *cmd;
    char *desc;
    char *syn;
    char *arg;
    char *examples;
};

/*
 help         - Print help for all commands
.target      - Set the device to use for commands
.usb         - Set the USB connected device to use for commands
.ver         - Give version information of server
.dev         - Determine if a device is connected
.lc          - List connected command clients
.ld          - List connected devices
.dn          - Delete specified nick
.nick        - Register a nick-name for connected device
.nl          - List all registered nicks
 */
/**
 * List of help texts for DB commands
 */
struct srvcmd_help srvcmd_help_list [] = {
    {"lookup",
       "Toggle reverse lookup lookup from location before storing in database",
       "",
       "",
       ""
    },
    {"cachestat",
       "Print information about Geo location cache usage",
       "",
       "",
       ""
    },    
    {"target",
        "Specify which target device to use to send commands to.\n"
        "The target is specified as either the client number (as listed by \".lc\" command)\n"
        "or the nick name for the device. For this command to work the device must have an\n"
        "established contact over GPRS",
        "[nnn|nick-name]",
        "nnn         - Client number (as listed by \".lc\"\n"
        "nick-name   - Nick name (must be defined)",
        "\".target 1\"     - Use connected client 1 as recipient\n"
        "\".target mycar\" - Communicate to device with nick \"mycar\""
    },
    {"usb",
        "Specify which USB connected device to send commands to or list\n"
        "or list the status of the defined USB ports.\n"
        "The target is specified as the USB index (as listed by \".lc\" command)",
        "[n]",
        "n         - USB index (as listed by \".lc\"",
        "\".usb 1\"     - Use connected client to USB 1 as recipient of commands\n"
        "\".usb\"       - List all USB ports"
    },    
    {"ver",
       "Return daemon version",
       "",
       "",
       ""
    },
    {"lc",
       "List all connected command clients with their IP and connection time",
       "",
       "",
       ""
    },
    {"ld",
       "List all connected USB devices to the daemon both USB connections and GPRS connections",
       "",
       "",
       ""
    },    
    {"dn",
       "Delete existing nick",
       "nick-name",
       "nick-name   - The name of the nick to delete",
       ""
    },
    {"nick",
       "Register a new nick name. The device to register must be connected to the USB port",
       "nick-name [phone-number]",
       "nick-name    - The nick name of the new device\n"
       "phone-number - Optional. Registered phone number",
       ".nick MyCar 070-123 45 67"
    },
    {"ln",
       "List all registered nick names using the specified format. Defaults to format 2.",
       "[n]",
       "0   - Compressed. One record per line\n"
       "1   - Semi-compressed. Four lines per record\n"
       "2   - One field in record per line",
       ".nl 2"
    },
    {"ratereset",
       "Reset the 24h suspension on Google API calls due to excess usage",
       "",
       "",
       ".ratereset"
    },
    {"report",
       "Generate a PDF and JSON report for the current attached device",
       "filename [title]",
       "filename - Filename of the generated report (without the .pdf suffix)\n"
       "[title] - The title as printed in the report header",
       ".report dev01 Motorcycle"
    },    
    {"date",
       "Print server date and time",
       "",
       "",
       ".date"
    },
    {"table",
       "Switch between ASCII and Unicode box drawing characters for output tables",
       "",
       "",
       ".table"
    }    
};

/**
 * Number of srvcommands
 */
const size_t num_srvcmd_len = sizeof(srvcmd_help_list)/sizeof(struct srvcmd_help);

/**
 * Handle the "nick" server command. Store or update the current connected
 * device in the nick table.
 * @param cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate bac
 *                 to the client as well as information about which device the client
 *                 has set as target device.

 * @param nick Nick name
 * @param phone Phone number
 * @return 0 on success, -1 on failure
 */
int
_srvcmd_nick(struct client_info *cli_info, const char *nick,const char *phone) {
    const int sockd = cli_info->cli_socket;
    // First gather all information from connected device
    //01.  car01       3000000001  2354561265412    823543892652      079-0023459876   WP 2.1.13, Rev A02 P     2014-01-17 10:12  2014-01-19 12:33
    char _config[48],_imei[48],_sim[48],_fwver[32];
    char devid[32],imei[32],sim[32],fwver[32];

    // Get device  information
    // "$OK:UNCFG=3000000001,0000,"
    _writef_reply_interactive(sockd,"Gathering information from device. Please wait ...\n");
    _writef_reply_interactive(sockd,"[0%%]..");
    if( -1 == send_cmdquery_reply(cli_info, "CONFIG",_config,sizeof(_config)) ) {
        return -1;
    }
    extract_devcmd_reply_first_field(_config,devid,sizeof(devid));

    // "$OK:IMEI=352964052600039"
    _writef_reply_interactive(sockd,"[25%%]..");
    if( -1 == send_cmdquery_reply(cli_info, "IMEI",_imei,sizeof(_imei)) ) {
        return -1;
    }
    extract_devcmd_reply_first_field(_imei,imei,sizeof(imei));

    // "$OK:SIMID=89460844007000101944"
    _writef_reply_interactive(sockd,"[50%%]..");
    if( -1 == send_cmdquery_reply(cli_info, "SIM",_sim,sizeof(_sim)) ) {
        return -1;
    }
    extract_devcmd_reply_first_field(_sim,sim,sizeof(sim));

    // "$OK:VER=M7 2.005 GP rev00c,V2"
    _writef_reply_interactive(sockd,"[75%%]..");
    if( -1 == send_cmdquery_reply(cli_info, "VER",_fwver,sizeof(_fwver)) ) {
        return -1;
    }
    extract_devcmd_reply_first_field(_fwver,fwver,sizeof(fwver));

    _writef_reply_interactive(sockd,"[100%%]\n");
    int rc = db_update_nick(nick,devid,imei,sim,phone,fwver);
    if( -1 == rc ) {
        _writef_reply_err(sockd,-1,"Failed to update/create nick for device. Check supplied arguments.\n");
        return -1;
    }

    // Confirm the new nick by issuing the get nic command
    db_get_nick_list(sockd,imei,2);

    return 0;
}

#define FILTER_CMD_CONNECTIONS 1
#define FILTER_DEV_CONNECTIONS 2
#define FILTER_GPRS_CONNECTIONS 3

/**
 * Print list of connections
 * @param[in] cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate back
 *                 to the client as well as information about which device the client
 *                 has set as target device.
 * @param filter FIlter action for what to list. 
 * 1=list command connections, 2=list device connections (both USB and GPRS),
 * 3=list only GPRS connections. In all situations highlight the current active device connection
 * @return 0 success, -1 failure
 */
int
_srv_cmd_lc(struct client_info *cli_info, const int filter) {
    const int sockd = cli_info->cli_socket;
    int y, m, d, h, min, s;
    size_t rows = 1;

    // Construct the array of data fields
    const size_t nCols = 4;
    char *tdata[(max_clients + 1) * nCols];
    memset(tdata, 0, sizeof (tdata));
    tdata[0 * nCols + 0] = strdup(" # ");
    tdata[0 * nCols + 1] = strdup("  IP  ");
    tdata[0 * nCols + 2] = strdup("  Conn. Time  ");
    tdata[0 * nCols + 3] = strdup("  Dev ID  ");

    char buff1[8], buff2[32], buff3[32], buff4[32];
    for (size_t i = 0; i < max_clients; i++) {

        // If the client exist this is the same as checking if the thread is valid
        if (client_info_list[i].cli_thread) {
            if ((FILTER_CMD_CONNECTIONS == filter && client_info_list[i].cli_is_cmdconn) ||
                    ((FILTER_DEV_CONNECTIONS == filter || FILTER_GPRS_CONNECTIONS == filter) && !client_info_list[i].cli_is_cmdconn)) {
                snprintf(buff1, sizeof (buff1), " %02zu ", rows);
                fromtimestamp(client_info_list[i].cli_ts, &y, &m, &d, &h, &min, &s);
                snprintf(buff3, sizeof (buff3), " %04d-%02d-%02d %02d:%02d ", y, m, d, h, min);

                if (client_info_list[i].cli_is_cmdconn) {
                    snprintf(buff2, sizeof (buff2), " %c%s ",
                            client_info_list[i].cli_socket == cli_info->cli_socket ? '*' : ' ',
                            client_info_list[i].cli_ipadr);
                    *buff4 = '\0';
                } else {
                    snprintf(buff2, sizeof (buff2), " %c%s ",
                            client_info_list[i].cli_devid == cli_info->target_deviceid ? '*' : ' ',
                            client_info_list[i].cli_ipadr);
                    snprintf(buff4, sizeof (buff4), " %u ", client_info_list[i].cli_devid);
                }
                tdata[rows * nCols + 0] = strdup(buff1);
                tdata[rows * nCols + 1] = strdup(buff2);
                tdata[rows * nCols + 2] = strdup(buff3);
                tdata[rows * nCols + 3] = strdup(buff4);
                ++rows;
            }
        }
    }

    if (rows == 1) {
        // Insert an empty row
        tdata[1 * nCols + 0] = strdup(" ");
        tdata[1 * nCols + 1] = strdup(" ");
        tdata[1 * nCols + 2] = strdup(" ");
        tdata[1 * nCols + 3] = strdup(" ");
        rows++;
    }

    table_t *t = utable_create_set(rows, nCols, tdata);
    //utable_set_title(t, 1 == filter ? "Command connections" : "GPRS connections", TITLESTYLE_LINE);
    utable_set_row_halign(t, 0, CENTERALIGN);
    if (cli_info->use_unicode_table) {
        utable_stroke(t, sockd, TSTYLE_DOUBLE_V2);
    } else {
        utable_stroke(t, sockd, TSTYLE_ASCII_V3);
    }
    utable_free(t);
    for (size_t i = 0; i < rows * nCols; i++) {
        free(tdata[i]);
    }
    return 0;
}



/**
 * Print help for the server commands to the client socket
 * @param[in] cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate bac
 *                 to the client as well as information about which device the client
 *                 has set as target device.
 * @param fields The db command to give help for
 */
void
srvcmd_help(struct client_info *cli_info, char **fields) {

    const int sockd = cli_info->cli_socket;
    size_t i = 0;
    logmsg(LOG_DEBUG,"Server command: .%s",fields[1]);
    TMPBUFF_10K(tmpBuff);
    while (i < num_srvcmd_len) {
        if (0 == strcmp(srvcmd_help_list[i].cmd, fields[1])) {
            xvstrncat(tmpBuff,SIZE_10KB,".%s - %s\n\nSYNOPSIS:\n.%s %s\n\n",
                    fields[1], srvcmd_help_list[i].desc,fields[1], srvcmd_help_list[i].syn);
            if (strlen(srvcmd_help_list[i].arg) > 0) {
                xvstrncat(tmpBuff,SIZE_10KB,"ARGUMENTS:\n%s\n\n",srvcmd_help_list[i].arg);
            }
            xvstrncat(tmpBuff,SIZE_10KB, "EXAMPLES:\n%s\n\n", srvcmd_help_list[i].examples);
            _writef_reply(sockd,"%s",tmpBuff);
            free(tmpBuff);
            return;
        }
        ++i;
    }
    _writef_reply_err(sockd, -1, "\"%s\" is not a valid server command.\n",fields[1]);
    free(tmpBuff);
}

/**
 * Print internal information about connected client. Used for troubleshooting
 * @param cli_info The client context
 */
void
_srv_cmd_debug_info(struct client_info *cli_info) {
    // Print client information as debug information
    _writef(cli_info->cli_socket,
            "Devid: %u\n"\
            "IP: %s\n"\
            "IsCMD: %d\n"\
            "ts: %u\n"\
            "targ_dev_id: %u\n"\
            "targ_cli_idx: %d\n"\
            "targ_sock: %d\n"\
            "targ_usb_idx: %d\n",
            cli_info->cli_devid,
            cli_info->cli_ipadr,
            cli_info->cli_is_cmdconn,
            (unsigned)cli_info->cli_ts,
            cli_info->target_deviceid,            
            (int)cli_info->target_cli_idx,
            (int)cli_info->target_socket,
            (int)cli_info->target_usb_idx
            );
    
    list_usb_stat(cli_info);
}

/**
 * Generate the reports in JSON and PDF. The directory to store the reports are speciied
 * in the INI file
 * @param cli_info Client context
 * @param filename The base filename for the report
 * @param report_title The title as whosn in the header of the PDF report
 */
void
_srv_device_report(struct client_info *cli_info, char *filename, char *report_title) {
    char full_path[1024];
    int sockd = cli_info->cli_socket;
    _writef(sockd,"Gathering information from device, please wait ... \n");
    strncpy(full_path,filename,sizeof(full_path));
    int stat=export_g7ctrl_report(cli_info, full_path, sizeof(full_path), report_title);
    if( 0==stat ) {
        _writef(sockd,"Wrote device report to \"%s\"",full_path);
    } else {
        _writef(sockd,"FAILED to create device report. Communication problem?\n");
    }           
}

/**
 * Return the full date and time in localized format
 * @param sockd Socket to write back to client
 */
void
_srv_date(int sockd) {
    time_t now = time(NULL);
    char buff[64];
    ctime_r(&now, buff);
    buff[strlen(buff)-1]='\0';
    _writef(sockd,"%s",buff);
}

/**
 * Display the geocache hit statistics to the user
 * @param cli_info Client context
 */
void
_srv_cache_stat(struct client_info *cli_info) {
    
    const int sockd = cli_info->cli_socket;
    
    unsigned addr_tot_calls, minimap_tot_calls;
    double addr_hitrate, minimap_hitrate;
    double addr_cache_fill, minimap_cache_fill;
    size_t addr_musage,minimap_musage;
    
    get_cache_stat(CACHE_ADDR, &addr_tot_calls, &addr_hitrate, &addr_cache_fill, &addr_musage);    
    get_cache_stat(CACHE_MINIMAP, &minimap_tot_calls, &minimap_hitrate, &minimap_cache_fill, &minimap_musage);
    
    const size_t nCols = 5;
    char *tdata[3 * 5];
    memset(tdata, 0, sizeof (tdata));
    char valbuff[32];
    int row=0;
    
    /* Header */
    tdata[row * nCols + 0] = strdup("  Cache ");
    tdata[row * nCols + 1] = strdup("  Tot ");
    tdata[row * nCols + 2] = strdup("  Fill ");
    tdata[row * nCols + 3] = strdup("  Hits ");
    tdata[row * nCols + 4] = strdup("  Mem ");
    
    row++;
    /* Address */
    tdata[row * nCols + 0] = strdup(" Address ");
    
    snprintf(valbuff,sizeof(valbuff),"%u ",addr_tot_calls);
    tdata[row * nCols + 1] = strdup(valbuff);
    
    snprintf(valbuff,sizeof(valbuff),"%.0f %% ",addr_cache_fill*100);
    tdata[row * nCols + 2] = strdup(valbuff);
    
    snprintf(valbuff,sizeof(valbuff),"%.0f %% ",addr_hitrate*100);
    tdata[row * nCols + 3] = strdup(valbuff);

    snprintf(valbuff,sizeof(valbuff)," %zu kB ",addr_musage/1024);
    tdata[row * nCols + 4] = strdup(valbuff);
    
    row++;
    /* Minimap */
    tdata[row * nCols + 0] = strdup(" Minimap ");
    
    snprintf(valbuff,sizeof(valbuff),"%u ",minimap_tot_calls);
    tdata[row * nCols + 1] = strdup(valbuff);
    
    snprintf(valbuff,sizeof(valbuff),"%.0f %% ",minimap_cache_fill*100);
    tdata[row * nCols + 2] = strdup(valbuff);
    
    snprintf(valbuff,sizeof(valbuff),"%.0f %% ",minimap_hitrate*100);
    tdata[row * nCols + 3] = strdup(valbuff);

    snprintf(valbuff,sizeof(valbuff)," %zu kB ",minimap_musage/1024);
    tdata[row * nCols + 4] = strdup(valbuff);
    
    row++;
    
    /* Setup table */
    table_t *t = utable_create_set(row, nCols, tdata);
    

    utable_set_table_halign(t, RIGHTALIGN);
    utable_set_row_halign(t, 0, CENTERALIGN);
    utable_set_interior(t,TRUE,FALSE);
    if (cli_info->use_unicode_table) {
        utable_stroke(t, sockd, TSTYLE_DOUBLE_V4);
    } else {
        utable_stroke(t, sockd, TSTYLE_ASCII_V2);
    }
    utable_free(t);
    for (size_t i = 0; i < row * nCols; i++) {
        free(tdata[i]);
    }
}

/**
 * Internal sever command
 * @param cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate back
 *                 to the client as well as information about which device the client
 *                 has set as target device.
 * @param rcmdstr Internal server command
 */
void
exec_srv_command(struct client_info *cli_info, char *rcmdstr) {
    
    logmsg(LOG_DEBUG,"Entering server command");
    
    const int sockd = cli_info->cli_socket;
    char **field = (void *) NULL;
    int nf = 0;
    char *cmdstr = rcmdstr + 1;

    if (0 < matchcmd("^_kill" _PR_E, cmdstr, &field)) {
        // Undocumented command to force a program stackdump
        abort();
    } else if (0 < matchcmd("^date" _PR_E, cmdstr, &field)) {
        _srv_date(cli_info->cli_socket);
    } else if (0 < matchcmd("^cachestat" _PR_E, cmdstr, &field)) {
        _srv_cache_stat(cli_info);                
    } else if (0 < matchcmd("^report" _PR_S _PR_FILEPATH _PR_E, cmdstr, &field)) {
        _srv_device_report(cli_info,field[1],NULL);        
    } else if (0 < matchcmd("^report" _PR_S _PR_FILEPATH _PR_S _PR_ANPS _PR_E, cmdstr, &field)) {
        _srv_device_report(cli_info,field[1],field[2]);                
    } else if (0 < matchcmd("^usb" _PR_S _PR_N1 _PR_E, cmdstr, &field)) {
        set_usb_device_target_by_index(cli_info,xatoi(field[1]));
    } else if (0 < matchcmd("^target" _PR_S _PR_N1 _PR_E, cmdstr, &field)) {
        set_gprs_device_target_by_index(cli_info, xatoi(field[1]));
    } else if (0 < matchcmd("^target" _PR_S _PR_AN _PR_E, cmdstr, &field)) {
        set_gprs_device_target_by_nickname(cli_info, field[1]);
    } else if (0 < (nf = matchcmd("^ver" _PR_E, cmdstr, &field))) {
#ifdef __APPLE__
        _writef_reply(sockd, "GM7 Server version %s",PACKAGE_VERSION);
#else
        _writef_reply(sockd, "GM7 Server version %s (build %lu-%lu)",
                PACKAGE_VERSION, (unsigned long) &__BUILD_DATE, (unsigned long) &__BUILD_NUMBER);
#endif
    } else if (0 < (nf = matchcmd("^usb" _PR_E, cmdstr, &field))) {
        list_usb_stat(cli_info);
    } else if (0 < (nf = matchcmd("^lc" _PR_E, cmdstr, &field))) {
        _srv_cmd_lc(cli_info, FILTER_CMD_CONNECTIONS);
    } else if (0 < (nf = matchcmd("^ratereset" _PR_E, cmdstr, &field))) {
        geocode_rate_limit_reset();
        staticmap_rate_limit_reset();
        _writef(sockd,"Google API rate limit reset");
        
#ifdef TEST_MAIL_WITH_MINIMAP        
    } else if (0 < (nf = matchcmd("^tstmail" _PR_E, cmdstr, &field))) {
        if( 0 == tst_mailimg() )
            _writef(sockd,"Sent test image mail");        
        else
            _writef(sockd,"FAILED to send test image mail");        
#endif        
        
    } else if (0 < (nf = matchcmd("^lookup" _PR_E, cmdstr, &field))) {
        use_address_lookup = !use_address_lookup;
        _writef(sockd,"Address lookup : %s",use_address_lookup ? "on" : "off");
    } else if (0 < (nf = matchcmd("^ld" _PR_E, cmdstr, &field))) {
        _srv_cmd_lc(cli_info, FILTER_DEV_CONNECTIONS);
    } else if (0 < (nf = matchcmd("^table" _PR_E, cmdstr, &field))) {
        cli_info->use_unicode_table = !cli_info->use_unicode_table;
        _writef_reply(sockd,"Table drawing : %s",cli_info->use_unicode_table ? "On" : "Off");
    } else if (0 < (nf = matchcmd("^target" _PR_E, cmdstr, &field))) {
        _srv_cmd_lc(cli_info, FILTER_GPRS_CONNECTIONS);        
    } else if (0 < (nf = matchcmd("^dbg" _PR_E, cmdstr, &field))) {
        _srv_cmd_debug_info(cli_info);        
    } else if (0 < (nf = matchcmd("^nick" _PR_S _PR_AAN _PR_S _PR_NDS _PR_E, cmdstr, &field))) {
        _srvcmd_nick(cli_info, field[1], field[2]);
    } else if (0 < (nf = matchcmd("^nick" _PR_S _PR_AAN _PR_E, cmdstr, &field))) {
        _srvcmd_nick(cli_info, field[1], "");
    } else if (0 < (nf = matchcmd("^ln" _PR_E, cmdstr, &field))) {
        db_get_nick_list(sockd, NULL, 3); // Default to format 3 if none is given
    } else if (0 < (nf = matchcmd("^ln" _PR_S _PR_N _PR_E, cmdstr, &field))) {
        db_get_nick_list(sockd, NULL, xatoi(field[1]));
    } else if (0 < (nf = matchcmd("^dn" _PR_S _PR_AAN _PR_E, cmdstr, &field))) {
        if (0 == db_delete_nick(field[1])) {
            _writef_reply(sockd, "Deleted nick \"%s\"", field[1]);
        } else {
            _writef_reply_err(sockd, -1, "Nick \"%s\" doe not exist", field[1]);
        }
    } else {
        _writef_reply_err(sockd, -1, "Incorrect command.");
    }
}

/* EOF */
