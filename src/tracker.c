/* =========================================================================
 * File:        TRACKER.C
 * Description: Listening for incoming packages on the designated
 *              socket that the device is connecting on. All incoming
 *              traffic is stored in the tracker log file which is
 *              actually an sqlite DB
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
#include <syslog.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <sqlite3.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "config.h"
#include "g7ctrl.h"
#include "utils.h"
#include "futils.h"
#include "logger.h"
#include "g7config.h"
#include "serial.h"
#include "libxstr/xstr.h"
#include "dbcmd.h"
#include "g7cmd.h"
#include "g7sendcmd.h"
#include "mailutil.h"
#include "nicks.h"
#include "geoloc.h"
#include "geoloc_cache.h"

#define LEN_10K (10*1024)
#define LEN_1K (1024)
#define LEN_LARGE LEN_10K
#define LEN_MEDIUM LEN_1K
#define LEN_SMALL (255)

/** Length of KEEP_ALIVE special packet*/
#define KEEP_ALIVE_LEN 8

/** Large buffer to handle potential large data set from the device */
#define BUFFER_50K (50*1024)

#define GFEN_TRACK_TAG "TR48"

/**
 * Check for any special handling requested when we receive this event
 * @param flds Data from the event
 * @param cli_info Detailed information about current client
 */
static void
chk_specialhandling(struct splitfields *flds, struct client_info *cli_info) {
    unsigned eventid = xatoi(flds->fld[GM7_LOC_EVENTID]);
    if (eventid > 100) {
        logmsg(LOG_ERR, "chk_specialhandling() : Unknown event id=%d. Expected id to be <= 100", eventid);
        return;
    }

    switch (eventid) {
        case EVENT_GFEN: // GFEN
            if (enable_gfen_tracking) {
                // This indicates that we put the device in tracking mode after a
                // GFEN event has been received (i.e. the device has crossed an electronic fence)
                // Insert the command to start tracking in the device queue so that the tracker
                // thread can match this command when the reply comes back
                char cmdbuff[64];
                char pinbuff[8];
                char tagbuff[8];
                get_device_pin(pinbuff, sizeof (pinbuff) - 1);

                // Special tag for this occasion to supress warning on stray command in tracker thread
                xstrlcpy(tagbuff, GFEN_TRACK_TAG, sizeof(tagbuff));

                snprintf(cmdbuff, sizeof (cmdbuff), "$WP+TRACK+%s=%s,1,%u,0,0,0,4,15\r\n", tagbuff, pinbuff, (unsigned) gfen_tracking_interval);
                int rc = write(cli_info->cli_socket, cmdbuff, strlen(cmdbuff));
                if (rc != (int) strlen(cmdbuff)) {
                    logmsg(LOG_ERR, "Failed to send automatic TRACK after GFEN to device");
                    return;
                } else {
                    logmsg(LOG_DEBUG, "Sent automatic TRACK after GFEN to device \"%s\"", cmdbuff);
                }

            }
            break;
    }
}

/**
 * Run a system command
 * @param arg Command string
 * @return NULL
 */
void *
system_thread(void *arg) {
    char *system_cmd = (char *) arg;

    // To avoid reserving ~8MB after the thread terminates we
    // detach it. Without doing this the pthreads library would keep
    // the exit status until the thread is joined (or detached) which would mean
    // loosing 8MB for each created thread
    pthread_detach(pthread_self());

    int rc = system(system_cmd);

    if (0 == rc) {
        logmsg(LOG_NOTICE, "Shell script: \"%s\" have run.", system_cmd);
    } else {
        logmsg(LOG_ERR, "Error running shell script: \"%s\"", system_cmd);
    }

    free(system_cmd);
    pthread_exit(NULL);
    return (void *) 0;
}

/**
 * Check if the corresponding action script to the received event
 * should be executed. Normally events 2 and 0 will not be executed
 * since they are the normal position updated events and doesn't
 * indicate any alarms. However if the argument forceExecution is true
 * then the action script for event 2 and 0 will also be eceuted.
 * @param flds Data from the event to be passed to the action script
 */
static void
chk_actionscript(struct splitfields *flds) {
    unsigned eventid = xatoi(flds->fld[GM7_LOC_EVENTID]);

    if (eventid > 100) {
        logmsg(LOG_ERR, "Event id > 100");
        return;
    }

    // Ignore position update (0) and track position update (2) type of events
    // unless forced
    //if ( TRUE==forceExecute || (2 != event && 0 != event)) {

    // Translate dev id to nick name if it exists

    char scriptName[256];
    snprintf(scriptName, sizeof (scriptName), "%s/event_scripts/%u_action.sh", data_dir, eventid);

    if (0 == access(scriptName, R_OK)) {
        char nick[16];
        if (db_get_nick_from_devid(flds->fld[GM7_LOC_DEVID], nick)) {
            // No nickname. Put device ID in its place
            xmb_strncpy(nick, flds->fld[GM7_LOC_DEVID], sizeof (nick) - 1);
            //nick[sizeof (nick) - 1] = '\0';
        }
        const size_t size = 2048;
        char *cmd = _chk_calloc_exit(size);
        snprintf(cmd, size, "sh %s -t %s -d %s -l \"%s\" -n \"%s\" -m \"%s\"",
                scriptName,
                flds->fld[GM7_LOC_DATE], flds->fld[GM7_LOC_DEVID],
                flds->fld[GM7_LOC_LAT], flds->fld[GM7_LOC_LON], nick);

        pthread_t dummy_threadid;
        int rc = pthread_create(&dummy_threadid, NULL, system_thread, (void *) cmd);
        if (rc) {
            logmsg(LOG_ERR, "Cannot start script thread: \"%s\"", cmd);
        }
    }
}

/**
 * Add information about disk usage to the dictionary
 * @param rkeys
 * @return 0 on success, -1 on failure
 */
static int add_diskspace2dict(dict_t dict) {
    // Add information on disk usage
    char ds_fs[LEN_SMALL], ds_size[LEN_SMALL], ds_avail[LEN_SMALL], ds_used[LEN_SMALL];
    int ds_use=0;
    if (0 == get_diskspace(data_dir, ds_fs, ds_size, ds_used, ds_avail, &ds_use)) {
        char buf[16];
        add_dict(dict, "DISK_SIZE", ds_size);
        add_dict(dict, "DISK_USED", ds_used);
        snprintf(buf, sizeof (buf), "%d", ds_use);
        add_dict(dict, "DISK_PERCENT_USED", buf);
        return 0;
    }    
    return -1;
}

static void
add_serverinfo2dict(dict_t dict) {
        
    // Get full current time to include in mail
    char buf[LEN_MEDIUM];
    time_t now = time(NULL);
    ctime_r(&now, buf);
    buf[strnlen(buf, sizeof (buf)) - 1] = 0; // Remove trailing newline
    add_dict(dict, "SERVERTIME", buf);

    // Include the server name in the mail
    gethostname(buf, sizeof (buf));
    buf[sizeof (buf) - 1] = '\0';
    add_dict(dict, "SERVERNAME", buf);
    
}

static int
add_avgload2dict(dict_t dict) {

    // Add system load information
    double avg1 = 0, avg5 = 0, avg15 = 0;
    char sysloadBuffer[64];
    sysloadBuffer[0] = '\0';
    if (0 == getsysload(&avg1, &avg5, &avg15)) {
        snprintf(sysloadBuffer, sizeof (sysloadBuffer), "%.2f %.2f %.2f", avg1, avg5, avg15);
        add_dict(dict, "SYSTEM_LOADAVG", sysloadBuffer);
        return 0;
    }
    return -1;
}

/**
 * This gets called once for each new tracker connection the server receives
 * and if this is enabled (in config) then it has the possibility to both send
 * a mail and execute a shell script. This can typically be setup as an alarm
 * when the device starts moving. This is similar to the device WAKE-UP report
 * but that report is usually only sent after a minimum of 2min after the actual
 * wake up (in order to get a fix on the location). When it comes to alarms 
 * every minute count so this can be used to send a notification right away.
 * @param devid Device id as a string
 */
static void
chk_connection_notification(char *devid, struct client_info *cli_info) {

    if (!script_on_tracker_conn && !mail_on_tracker_conn)
        return;

    char nick[16];
    char nick_devid[32];
    if (db_get_nick_from_devid(devid, nick)) {
        // No nickname. Put device ID in its place
        xmb_strncpy(nick, devid, sizeof (nick) - 1);
        xmb_strncpy(nick_devid, devid, sizeof (nick_devid) - 1);
    } else {
        snprintf(nick_devid, sizeof (nick_devid), "%s (%s)", nick, devid);
    }

    // First kick off any scripts that needs to run
    if (script_on_tracker_conn) {
        // Then check if there is a shell script to be executed
        char scriptName[512];
        snprintf(scriptName, sizeof (scriptName), "%s/event_scripts/tracker_conn.sh", data_dir);
        if (0 == access(scriptName, R_OK)) {
            const size_t size = 1024;
            char *cmd = _chk_calloc_exit(size);
            snprintf(cmd, size, "sh %s -d %s -n \"%s\"", scriptName, devid, nick);
            logmsg(LOG_DEBUG, "Executing script on tracker connect cmd='%s'", cmd);
            pthread_t dummy_threadid;
            int rc = pthread_create(&dummy_threadid, NULL, system_thread, (void *) cmd);
            if (rc) {
                logmsg(LOG_ERR, "Cannot start script thread: \"%s\"", cmd);
            }
        } else {
            logmsg(LOG_WARNING, "Script on tracker connect enabled but no script found at '%s'", scriptName);
        }
    }

    // Then check if we should send a mail
    if (mail_on_tracker_conn) {
        dict_t dict = new_dict();
        
        char short_devid[8];        
        *short_devid = '\0';            
        if( use_short_devid ) {
            const size_t devid_len = strlen(devid);
            for( size_t i=0 ; i < 4; i++) {
                short_devid[i] = devid[devid_len-4+i];
            }            
            short_devid[4] = '\0';            
            add_dict(dict, "DEVICEID", short_devid);
        } else {
            add_dict(dict, "DEVICEID", devid);
        }
                        
        add_serverinfo2dict(dict);
        add_diskspace2dict(dict);
        add_avgload2dict(dict);
        
        add_dict(dict,  "NICK", nick);
        add_dict(dict,  "NICK_DEVID", nick_devid);
        add_dict(dict,  "DAEMONVERSION", PACKAGE_VERSION);
        add_dict(dict,  "CLIENT_IP", cli_info->cli_ipadr);

        char subjectbuff[LEN_SMALL];
        if( use_short_devid ) {
            snprintf(subjectbuff, sizeof (subjectbuff), SUBJECT_NEWCONNECTION, mail_subject_prefix, short_devid);
        } else {
            snprintf(subjectbuff, sizeof (subjectbuff), SUBJECT_NEWCONNECTION, mail_subject_prefix, devid);
        }
        
        if (-1 == send_mail_template(subjectbuff, daemon_email_from, send_mailaddress, "mail_tracker_conn", 
                                     dict, NULL, 0, NULL)) {
            logmsg(LOG_ERR, "Failed to send mail using template \"mail_tracker_conn\"");
        }
        
        logmsg(LOG_INFO, "Sent mail for new device connection (%s) to \"%s\"", devid, send_mailaddress);
        free_dict(dict);

    }
}


/**
 * Check if a mail should be sent for this event. Normally no mail is sent
 * for the normal events 0 && 2
 * @param flds Fields from the event to be used in the mail
 * @param forceSend  Force mail even for event 0 && 2
 */
static void
chk_sendmail(struct splitfields *flds, int forceSend) {
    char eventCmd[128], eventDesc[128];
    int event = xatoi(flds->fld[GM7_LOC_EVENTID]);

    // We never send a mail on the REC=1 event and normally not on
    // GETLOCATION (=0) or TRACK (=2) unless forceSend is true
    if (event != 1 && (forceSend || (event != 0 && event != 2))) {

        int rc = get_event_cmd(event, eventCmd, eventDesc);
        if (-1 == rc) {
            logmsg(LOG_ERR, "Mail NOT sent. Packet with unknown event type received (=%d)", event);
            return;
        }

        dict_t dict = new_dict();
        
        add_serverinfo2dict(dict);
        add_diskspace2dict(dict);
        add_avgload2dict(dict);

        // Format the displayed date/time from the device so it is a bit easier to read in the mail
        char datetimeFmt[32];
        size_t tmpIdx = 4;
        strncpy(datetimeFmt, flds->fld[GM7_LOC_DATE], 4);
        datetimeFmt[tmpIdx++] = '-';
        datetimeFmt[tmpIdx++] = flds->fld[GM7_LOC_DATE][4];
        datetimeFmt[tmpIdx++] = flds->fld[GM7_LOC_DATE][5];
        datetimeFmt[tmpIdx++] = '-';
        datetimeFmt[tmpIdx++] = flds->fld[GM7_LOC_DATE][6];
        datetimeFmt[tmpIdx++] = flds->fld[GM7_LOC_DATE][7];

        // Start with time
        datetimeFmt[tmpIdx++] = ' ';
        datetimeFmt[tmpIdx++] = ' ';
        datetimeFmt[tmpIdx++] = flds->fld[GM7_LOC_DATE][8];
        datetimeFmt[tmpIdx++] = flds->fld[GM7_LOC_DATE][9];
        datetimeFmt[tmpIdx++] = ':';
        datetimeFmt[tmpIdx++] = flds->fld[GM7_LOC_DATE][10];
        datetimeFmt[tmpIdx++] = flds->fld[GM7_LOC_DATE][11];
        datetimeFmt[tmpIdx++] = ':';
        datetimeFmt[tmpIdx++] = flds->fld[GM7_LOC_DATE][12];
        datetimeFmt[tmpIdx++] = flds->fld[GM7_LOC_DATE][13];
        datetimeFmt[tmpIdx] = '\0';

        // Translate dev id to nick name if it exists
        char nick[16];
        char nick_devid[512];
        if (db_get_nick_from_devid(flds->fld[GM7_LOC_DEVID], nick)) {
            // No nickname. Put device ID in its place
            xmb_strncpy(nick, flds->fld[GM7_LOC_DEVID], sizeof (nick) - 1);
            xmb_strncpy(nick_devid, flds->fld[GM7_LOC_DEVID], sizeof (nick_devid) - 1);
        } else {
            snprintf(nick_devid, sizeof (nick_devid), "%s (%s)", nick, flds->fld[GM7_LOC_DEVID]);
        }

        char short_devid[8];        
        *short_devid = '\0';            
        if( use_short_devid ) {
            const size_t devid_len = strlen(flds->fld[GM7_LOC_DEVID]);
            for( size_t i=0 ; i < 4; i++) {
                short_devid[i] = flds->fld[GM7_LOC_DEVID][devid_len-4+i];
            }            
            short_devid[4] = '\0';            
            add_dict(dict, "DEVICEID", short_devid);
        } else {
            add_dict(dict, "DEVICEID", flds->fld[GM7_LOC_DEVID]);
        }
        
        // Add generic fields
        add_dict(dict,"NICK_DEVID", nick_devid);
        add_dict(dict, "EVENTCMD", eventCmd);
        add_dict(dict, "EVENTDESC", eventDesc);
        add_dict(dict, "DAEMONVERSION", PACKAGE_VERSION);
        add_dict(dict, "NICK", nick);
        add_dict(dict, "DATETIME", datetimeFmt);
        
        //
        // Add cache stat fields
        //
        unsigned cache_calls;
        double cache_hitrate, cache_fill;
        size_t cache_mem;
        char valbuff[32];
        
        get_cache_stat(GEOCACHE_ADDR, &cache_calls, &cache_hitrate, &cache_fill, &cache_mem);        
        
        snprintf(valbuff,sizeof(valbuff),"%u",geocache_address_size);
        add_dict(dict,"ADDRESS_CACHE_MAXSIZE", valbuff);

        snprintf(valbuff,sizeof(valbuff),"%.0f %%",cache_fill*100);
        add_dict(dict,"ADDRESS_CACHE_FILL", valbuff);
        
        snprintf(valbuff,sizeof(valbuff),"%.0f %% ",cache_hitrate*100);
        add_dict(dict,"ADDRESS_CACHE_HITRATE", valbuff);
        
        snprintf(valbuff,sizeof(valbuff),"%zu kB ",cache_mem/1024);
        add_dict(dict,"ADDRESS_CACHE_MEM", valbuff);
        
        snprintf(valbuff,sizeof(valbuff),"%u",cache_calls);        
        add_dict(dict,"ADDRESS_CACHE_TOTCALLS", valbuff);
        

        get_cache_stat(GEOCACHE_MINIMAP, &cache_calls, &cache_hitrate, &cache_fill, &cache_mem);        
        
        snprintf(valbuff,sizeof(valbuff),"%u",geocache_minimap_size);
        add_dict(dict,"MINIMAP_CACHE_MAXSIZE", valbuff);

        snprintf(valbuff,sizeof(valbuff),"%.0f %%",cache_fill*100);
        add_dict(dict,"MINIMAP_CACHE_FILL", valbuff);
        
        snprintf(valbuff,sizeof(valbuff),"%.0f %% ",cache_hitrate*100);
        add_dict(dict,"MINIMAP_CACHE_HITRATE", valbuff);
        
        snprintf(valbuff,sizeof(valbuff)," %zu kB ",cache_mem/1024);
        add_dict(dict,"MINIMAP_CACHE_MEM", valbuff);
        
        snprintf(valbuff,sizeof(valbuff),"%u",cache_calls);        
        add_dict(dict,"MINIMAP_CACHE_TOTCALLS", valbuff);
        
        //
        // Add location update data from device
        //
        add_dict(dict, "LON", flds->fld[GM7_LOC_LON]);
        add_dict(dict, "LAT", flds->fld[GM7_LOC_LAT]);
        add_dict(dict, "VOLTAGE", flds->fld[GM7_LOC_VOLT]);
        add_dict(dict, "SPEED", flds->fld[GM7_LOC_SPEED]);
        add_dict(dict, "SAT", flds->fld[GM7_LOC_SAT]);
        add_dict(dict, "HEADING", flds->fld[GM7_LOC_HEADING]);

        char rndval[32];
        snprintf(rndval,sizeof(rndval),"%d",rand());
        add_dict(dict, "RANDOM", rndval);
        
        // Check to see if we should do a reverse address lookup
        if (use_address_lookup) {
            char address[512];            
            // No need for error check since the address field will have  "?" in case of error
            get_address_from_latlon(flds->fld[GM7_LOC_LAT], flds->fld[GM7_LOC_LON], address, sizeof (address));
            add_dict(dict, "APPROX_ADDRESS", address);
        } else {
            add_dict(dict, "APPROX_ADDRESS", "(disabled)");
        }

        char subjectbuff[LEN_MEDIUM];
        snprintf(subjectbuff, sizeof (subjectbuff), SUBJECT_EVENTMAIL, 
                mail_subject_prefix, 
                use_short_devid ? short_devid : flds->fld[GM7_LOC_DEVID], 
                eventDesc);
            

        if (include_minimap) {
            
            logmsg(LOG_DEBUG,"Adding minimap to mail");

            char kval[32];
            snprintf(kval,sizeof(kval),"%d",minimap_width);
            add_dict(dict, "IMG_WIDTH", kval);
            
            snprintf(kval,sizeof(kval),"%d",minimap_overview_zoom);
            add_dict(dict, "ZOOM_OVERVIEW", kval);
            
            snprintf(kval,sizeof(kval),"%d",minimap_detailed_zoom);
            add_dict(dict, "ZOOM_DETAILED", kval);

            char *overview_imgdata, *detailed_imgdata;
            const char *lat = flds->fld[GM7_LOC_LAT];
            const char *lon = flds->fld[GM7_LOC_LON];
            const unsigned short overview_zoom = minimap_overview_zoom;
            const unsigned short detailed_zoom = minimap_detailed_zoom;
            char imgsize[12];
            snprintf(imgsize,sizeof(imgsize),"%dx%d",minimap_width,minimap_height);
            const char *overview_filename = "overview_map.png";
            const char *detailed_filename = "detailed_map.png";
            size_t overview_datasize=0, detailed_datasize=0;

            int rc1 = get_minimap_from_latlon(lat, lon, overview_zoom, minimap_width, minimap_height, &overview_imgdata, &overview_datasize);
            if( 0 == rc1 )
                rc1 = get_minimap_from_latlon(lat, lon, detailed_zoom, minimap_width, minimap_height, &detailed_imgdata, &detailed_datasize);

            if (-1 == rc1 ) {
                logmsg(LOG_ERR, "Failed to get static map from Google. Are you using a correct API key?");
                logmsg(LOG_ERR, "Sending mail without the static maps.");
                rc = send_mail_template(subjectbuff, daemon_email_from, send_mailaddress,
                                "mail_event",
                                dict, NULL, 0, NULL);
            } else {
                struct inlineimage_t *inlineimg_arr = calloc(2, sizeof (struct inlineimage_t));
                setup_inlineimg(&inlineimg_arr[0], overview_filename, overview_datasize, overview_imgdata);
                setup_inlineimg(&inlineimg_arr[1], detailed_filename, detailed_datasize, detailed_imgdata);

                rc = send_mail_template(subjectbuff, daemon_email_from, send_mailaddress,
                                    "mail_event_img",
                                    dict, NULL, 2, inlineimg_arr);

                free_inlineimg_array(inlineimg_arr, 2);
                free(inlineimg_arr);

            }

        } else {
            // rc = -1 => Error
            // rc = -99 => Mail disabled in the configuration
            rc = send_mail_template(subjectbuff, daemon_email_from, send_mailaddress,
                    "mail_event",
                    dict, NULL, 0, NULL);
        }
        
        if (-1 == rc) {
            logmsg(LOG_ERR, "Failed to send mail using template \"mail_event\"");
        } else if ( 0 == rc ) {
            logmsg(LOG_INFO, "Sent mail for event \"%s\" to \"%s\"", eventCmd, send_mailaddress);
        }        
        
        free_dict(dict);
    }
}

/**
 * This callback is called after each location event has been stored in the DB
 * @param flds The splitted field in the location event
 * @param cb_option Force mail flag
 */
static void
store_loc_Callback(struct splitfields *flds, void *cb_option) {
    struct client_info *cli_info = (struct client_info *) cb_option;

    logmsg(LOG_DEBUG, "Checking mail");
    // Send potential mail on this event
    chk_sendmail(flds, force_mail_on_all_events);

    logmsg(LOG_DEBUG, "Checking actionscript");
    // Check for any potential action script to run
    chk_actionscript(flds);

    // Check for any special handling of this event type
    chk_specialhandling(flds, cli_info);
}

/**
 * This handler takes care of all that needs to be done when we have received
 * a location update package, i.e. storing it in the DB and potentially sending
 * a mail and calling an event script. See store_loc_Callback()
 * @param cli_info Information record on connected device client
 * @param buffer The actual data package we have received
 * @param len Length of data packet
 * @return 0 on success, -1 on failure
 */
static int
handleLocPackage(struct client_info *cli_info, const char *buffer, const size_t len) {
    logmsg(LOG_DEBUG, "LOC PKG: (%s:%d) -> %s [%zd]", cli_info->cli_ipadr, cli_info->cli_socket, "ARRIVE_PKG_LOC", len);
    logmsg(LOG_DEBUG, "RAW: [%s]", buffer);
    const unsigned num_loc=1;
    return db_store_locations(cli_info->cli_socket,num_loc,buffer, store_loc_Callback, (void *) cli_info);
}

/**
 * This handler takes care of all that needs to be done when we have the reply
 * from a previously given command to the device.
 * We handle this by checking the queue of outstanding
 * @param cli_info Information record on connected device client
 * @param buffer The actual data package we have received
 * @param len Length of data packet
 * @return 0 on success, -1 on failure
 */
static int
handleCmdReplyPackage(struct client_info *cli_info, const char *buffer, const size_t len) {
    logmsg(LOG_INFO, "RPLY PKG: (%s:%d) -> %s [%zd] \"%s\"", cli_info->cli_ipadr, cli_info->cli_socket, "ARRIVE_PKG_CMDREPLY", len, buffer);


    // Now find out which client sent this command so we can send the reply
    // back to the correct socket. If no socket is found then we assume that the
    // client has disconnected before the reply came back and log this as a
    // "stray signal" or rathe a stray "command reply"

    // As usual there is a special case in play here. The repoly to the GETLOCATION
    // command. This reply down not follow the rules at all and have no tags or comamnd
    // The reply is simple: (for example):
    // "3000000001,20140206224257,17.960205,59.366856,0,0,0,4,0,4.12V,0"
    // We identify this case with the lack of the initial "$" character

    _Bool isok = FALSE;
    char cmdname[16], tag[8];
    *cmdname = *tag = '\0';

    if (*buffer != '$' && *buffer == '3') {
        // Must be a location reply
        // Extract the device ID
        const char *ptr = buffer;
        char devid_buff[12];
        size_t idx = 0;
        while (*ptr != ',' && idx < sizeof (devid_buff) - 1) {
            devid_buff[idx++] = *ptr++;
        }
        devid_buff[idx] = '\0';
        if (*ptr != ',')
            return -1;
        unsigned devid = xatol(devid_buff);
        if (devid != cli_info->cli_devid) {
            logmsg(LOG_ERR, "Location reply is NOT from the expected tracker=%u but from=%u", cli_info->cli_devid, devid);
            return -1;
        }
    } else {

        // 1. Parse the reply to extract tag and command
        struct splitfields flds;
        if (extract_devcmd_reply(buffer, &isok, cmdname, tag, &flds)) {
            logmsg(LOG_ERR, "Cannot parse CMD reply: \"%s\"", buffer);
            return -1;
        }
    }
    // 2. Look up the command in the queue and get the entry
    struct cmdqueue_t *entry;
    if (cmdqueue_match(cli_info->cli_devid, tag, &entry)) {
        // No matching command was found
        if (strcmp(tag, GFEN_TRACK_TAG)) {
            logmsg(LOG_INFO, "Stray CMD reply [%u]: \"%s\"", cli_info->cli_devid, buffer);
            return -1;
        } else {
            logmsg(LOG_INFO, "Device reply for automatic TRACK after GFEN \"%s\"", buffer);
            return 0;
        }
    }
    xmb_strncpy(entry->reply, buffer, sizeof (entry->reply) - 1);
    logmsg(LOG_DEBUG, "Matching cmd: [%u:%s] \"%s\" ts=%lu, sock=%d",
            entry->devid, entry->tag, entry->cmd, entry->ts, entry->user_sockd);
    entry->validreply = TRUE;
    return 0;

    /*
    if( entry->user_sockd > 0 ) {
        // Normal user command so just write ti back to the user socket and clean queue
        (void)handle_device_reply(entry->user_sockd,buffer);
        logmsg(LOG_DEBUG, "GPRS RPLY: [%u:%s:%s]->%s",cli_info->cli_devid,tag,cmdname,buffer);
        if( -1 == cmdqueue_clridx(queue_idx) ) {
            logmsg(LOG_ERR,"Failed to clean command queue for index=%zd",queue_idx);
        } else {
            logmsg(LOG_DEBUG,"Cleaned command queue for index=%zd",queue_idx);
        }
    } else {
     */

    //}
    //return 0;
}

/**
 * Check for KEEP_ALIVE_PACKAGES from the device and handle it.
 * The only thing we do is to return the package back to the device.
 * @param cli_info  The client info record for the client that sent the KEEP_ALIVE package
 * @param buffer  Incoming data
 * @param len     Length of data
 * @return 0 on receiving a KEEP_ALIVE_PACKET, 1 for normal package, -1 on failure
 */
static int
handleKeepAlivePackage(struct client_info *cli_info, const char *buffer, const size_t len) {
    logmsg(LOG_DEBUG, "PKG: (%s:%d)-> %s", cli_info->cli_ipadr, cli_info->cli_socket, "ARRIVE_PKG_KEEPALIVE");
    if (KEEP_ALIVE_LEN == len &&
            (char) 0xD0 == *buffer && (char) 0xD7 == *(buffer + 1)) {
        unsigned devid =
                (unsigned char) buffer[4]*1 +
                (unsigned char) buffer[5]*256 +
                (unsigned char) buffer[6]*65536 +
                (unsigned char) buffer[7]*16777216;
        unsigned seq = (unsigned char) buffer[2]*1 + (unsigned char) buffer[3]*256;
        logmsg(LOG_DEBUG, "KEEP_ALIVE_PACKAGE [deviceid=%u : seq=%04u]", devid, seq);
        if (0 == cli_info->cli_devid) {
            _Bool is_reconnection = FALSE;
            // Check if this device ID already exists in that case kill that thread
            for (size_t i = 0; i < max_clients; i++) {
                if (client_info_list[i].cli_ts && devid == client_info_list[i].cli_devid) {
                    // Check that this isn't ourself
                    if (strcmp(client_info_list[i].cli_ipadr, cli_info->cli_ipadr) &&
                            client_info_list[i].cli_socket != cli_info->cli_socket) {
                        char ip[16] = {'\0'};
                        xmb_strncpy(ip, client_info_list[i].cli_ipadr, 15);
                        pthread_cancel(client_info_list[i].cli_thread);
                        logmsg(LOG_DEBUG, "Canceled old tracker client at IP=%s", ip);
                        is_reconnection = TRUE;
                    }
                }
            }


            if (!is_reconnection) {
                char devidbuff[12];
                snprintf(devidbuff, sizeof (devidbuff), "%u", devid);
                chk_connection_notification(devidbuff, cli_info);
            }

            // Note the device id for this connection
            cli_info->cli_devid = devid;
        }

        ssize_t rc = write(cli_info->cli_socket, buffer, KEEP_ALIVE_LEN);
        if (rc < 0 || rc != KEEP_ALIVE_LEN) {
            logmsg(LOG_ERR, "Failed to write back KEEP_ALIVE packet ( %d : %s )", errno, strerror(errno));
            return -1;
        }

        return 0;

    } else {

        return 1;

    }
}

/**
 * Tracker thread cleanup function
 * @param arg
 */
static void
trk_thread_cleanup(void *arg) {
    struct client_info *cli_info = (struct client_info *) arg;

    logmsg(LOG_DEBUG, "Tracker thread CleanupHandler() for IP=%s", cli_info->cli_ipadr);
    if (-1 == _dbg_close(cli_info->cli_socket)) {
        logmsg(LOG_ERR, "Failed to close socket %d to device %s. ( %d : %s )", cli_info->cli_socket, cli_info->cli_ipadr, errno, strerror(errno));
    }
    pthread_mutex_lock(&socks_mutex);
    memset(cli_info, 0, sizeof (struct client_info));
    cli_info->target_cli_idx = -1;
    cli_info->target_usb_idx = -1;
    num_clients--;
    pthread_mutex_unlock(&socks_mutex);
}

/** Timeout in seconds fro the listening for tracker connections. We use
 * the timeout to check how long time the tracker has been idle.
 */
#define TRACKER_SOCKET_TIMEOUT 2

#define ARRIVE_PKG_KEEPALIVE 0
#define ARRIVE_PKG_CMDREPLY 1
#define ARRIVE_PKG_LOC 2

/**
 * Determine what kind of package we have received
 * @param buffer The data received from the network
 * @return >= 0 The package type, -1 failure
 */
static int
arrivingPackageType(const char *buffer) {
    if ('$' == *buffer) {
        return ARRIVE_PKG_CMDREPLY;
    } else if ('3' == *buffer || '[' == *buffer) {
        /* This is a location update. Depending on the event-id this can
         * either be the reply from a previous GETLOCATION command (event-id == 0)
         * or it could be one of the other asynchronous "true" location updates
         * that should go straight to the DB since they were not directly the
         * consequence of a user command.
         */
        struct splitfields flds;
        if (0 == xstrsplitfields(buffer, ',', &flds)) {
            // 3000000001,20140107232526,17.961028,59.366470,0,0,0,0,1,4.20V,0
            if (11 != flds.nf) {
                logmsg(LOG_ERR, "Location packet should have 11 fields: %s", buffer);
                return -1;
            }
            const int eventid = xatoi(flds.fld[8]);
            if (0 == eventid) {
                // GETLOCATION reply
                return ARRIVE_PKG_CMDREPLY;
            } else {
                // All other event ID is some type of automatic updated from the
                // tracker
                return ARRIVE_PKG_LOC;
            }
        } else {
            logmsg(LOG_ERR, "Failed to split received location packet: %s", buffer);
            return -1;
        }
    }
    return -1;
}

/**
 * This is the thread entry point that gets started for each device that
 * connects to us.
 * The passed argument is a structure of connection information where
 * the id of the device as well as the socket to use to communicate
 * with the device. 
 *
 * This thread only terminates when the daemon is terminated.
 *
 * @param arg Thread argument. Structure with client information
 * @return  (void *)0
 */
void *
tracker_clientsrv(void *arg) {
    int rc;
    ssize_t numreads;
    struct client_info *cli_info = (struct client_info *) arg;
    fd_set read_fdset;
    struct timeval timeout;

    // To avoid reserving ~8MB after the thread terminates we
    // detach it. Without doing this the pthreads library would keep
    // the exit status until the thread is joined (or detached) which would mean
    // loosing 8MB for each created thread
    pthread_detach(pthread_self());

    // The pthread_cleanup_push() has an evil implementation in glibc
    // It uses open do {} while(0) which means it must always be paired with
    // an pthread_cleanup_pop() to compile at all. It also means that no
    // continue or break can be used in between the calls.
    // The reason for using this is that the thread can be canceled from the
    // main worker thread if the device IP that this thread is connected to
    // is changing. In that case the worker thread will cancel an old tracker thread
    // when the tracker connects on a new IP. Strictly speaking it wouldn't be
    // necessary since it will timeout after max_device_idle_time but it could mean
    // that in the meantime we have multiple IP for the same tracker and that doesn't
    // look very good !

    pthread_cleanup_push(trk_thread_cleanup, arg);

    unsigned idle_time = 0;
    char *buffer = calloc(BUFFER_50K, sizeof(char));
    if (!buffer) {
        logmsg(LOG_CRIT, "Cannot allocate memory for reading incoming tracker data. Aborting.");
    } else {
        do {
            FD_ZERO(&read_fdset);
            FD_SET((unsigned) cli_info->cli_socket, &read_fdset);

            timerclear(&timeout);
            timeout.tv_sec = TRACKER_SOCKET_TIMEOUT;

            rc = select(cli_info->cli_socket + 1, &read_fdset, NULL, NULL, &timeout);
            if (rc == 0) {
                //Timeout

                idle_time += TRACKER_SOCKET_TIMEOUT;

                if (idle_time >= max_device_idle_time) {
                    numreads = -1; // Force a disconnect
                    logmsg(LOG_DEBUG, "Tracker disconnected after being idle for more than %d seconds.", max_device_idle_time);
                } else {
                    numreads = 1; // To keep the loop going
                }

            } else {

                idle_time = 0;
                *buffer = '\0';
                numreads = socket_read(cli_info->cli_socket, buffer, BUFFER_50K);

                char *ptr, *ptrstart;
                char oldchar;
                if (numreads > 0) {

                    buffer[BUFFER_50K - 1] = '\0';
                    buffer[numreads] = '\0';

                    if (*buffer) {

                        if ((char) 0xD0 == *buffer && (char) 0xD7 == *(buffer + 1)) {
                            logmsg(LOG_DEBUG, "Incoming KEEP_ALIVE from IP=%s:%d (len=%zd)",
                                    cli_info->cli_ipadr, cli_info->cli_socket, numreads);
                            rc = handleKeepAlivePackage(cli_info, buffer, numreads);
                            if( -1 == rc ) {
                                logmsg(LOG_DEBUG, "Failed to handle incoming KEEP_ALIVE package");
                            }
                        } else {
                            // This gets a bit complicated due to the fact that the device might
                            // send back multiple location events in the same reply. We must therefore
                            // split the replies and handle them one by one. We do this by inserting
                            // a string terminating 0 after each '\\r\\n' sequence found and restore
                            // the overwritten character in the next loop
                            ptr = buffer;
                            do {
                                ptrstart = ptr;
                                while (*ptr && *ptr != '\r')
                                    ptr++;
                                if (*ptr == '\r' && *(ptr + 1) == '\n') {
                                    oldchar = *(ptr + 2);
                                    *(ptr + 2) = '\0';
                                } else {
                                    logmsg(LOG_ERR, "Expecting device event to end with '\\r\\n' but found %d %d",(int)(*ptr),(int)(*(ptr+1)));                                    
                                    break;
                                }

                                logmsg(LOG_DEBUG, "Incoming event from IP=%s:%d (len=%zd)",                                        
                                        cli_info->cli_ipadr, cli_info->cli_socket, numreads);
                                logmsg(LOG_DEBUG, "Event string: \"%s\"",ptrstart);
                                int pkg_t = arrivingPackageType(ptrstart);
                                switch (pkg_t) {
                                    case ARRIVE_PKG_CMDREPLY:
                                        rc = handleCmdReplyPackage(cli_info, ptrstart, numreads);
                                        break;
                                    case ARRIVE_PKG_LOC:
                                        rc = handleLocPackage(cli_info, ptrstart, numreads);
                                        break;
                                    default:
                                        rc = -1;
                                        break;
                                }
                                if (rc < 0) {
                                    logmsg(LOG_ERR, "Error handling %s",
                                            pkg_t == ARRIVE_PKG_CMDREPLY ? "ARRIVE_PKG_CMDREPLY" :
                                            pkg_t == ARRIVE_PKG_LOC ? "ARRIVE_PKG_LOC" : "UNKNOWN_PACKAGE TYPE");
                                }

                                *(ptr + 2) = oldchar;
                                ptr += 2;

                            } while (oldchar);
                        }
                    }

                } else {
                    if (numreads > 0) {
                        logmsg(LOG_ERR, "Received an incomplete location update from tracker");
                    }
                }
            }
        } while (numreads > 0);

        free(buffer);
    }

    logmsg(LOG_DEBUG, "Connection from device IP=%s on socket %d closed.", cli_info->cli_ipadr, cli_info->cli_socket);

    pthread_cleanup_pop(TRUE);
    pthread_exit(NULL);
    return (void *) 0;
}


/* EOF */
