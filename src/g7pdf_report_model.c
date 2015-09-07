/* =========================================================================
 * File:        G7CTRL_PDF_MODEL.C
 * Description: PDF Device report. Model for device report.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"
#include "g7ctrl.h"
#include "logger.h"
#include "xstr.h"
#include "g7sendcmd.h"
#include "g7cmd.h"
#include "g7pdf_report_model.h"
#include "assocarray.h"
#include "nicks.h"
#include "utils.h"

#define FLD_DEVICE_ID "device_id"
#define FLD_DEVICE_PWD "device_pwd"
#define FLD_SIM_PIN "sim_pin"
#define FLD_LOGGED_NUM "logged_num"
#define FLD_LOGGED_DATES "logged_dates"
#define FLD_COMM_SELECT "comm_select"
#define FLD_SMS_BASE "sms_base"
#define FLD_CSD_BASE "csd_base"
#define FLD_GPRS_APN "gprs_apn"
#define FLD_GPRS_USER "gprs_user"
#define FLD_GPRS_PWD "gprs_pwd"
#define FLD_GPRS_SERVER_IP "server_ip"
#define FLD_GPRS_SERVER_PORT "server_port"
#define FLD_GPRS_KEEP_ALIVE "gprs_keep_alive"
#define FLD_GPRS_DNS "gprs_dns"
#define FLD_ROAM "roam"
#define FLD_TRACK_MODE "track_mode"
#define FLD_TRACK_TIMER "track_time"
#define FLD_TRACK_DIST "track_dist"
#define FLD_TRACK_NUMBER "track_num"
#define FLD_TRACK_GPS_FIX "track_gps_fix"
#define FLD_TRACK_COMMSELECT "track_commselect"
#define FLD_TRACK_HEADING "track_heading"
#define FLD_LOWBATT_REPORT "lowbatt_report"
#define FLD_LOWBATT_VIP "lowbatt_vip"
#define FLD_VLOC_STATUS "vloc_stat"
#define FLD_VLOC_VIP "vloc_vip"
#define FLD_IMEI "imei"
#define FLD_SIM_ID "sim_id"
#define FLD_VIP1 "vip1"
#define FLD_VIP2 "vip2"
#define FLD_VIP3 "vip3"
#define FLD_VIP4 "vip4"
#define FLD_VIP5 "vip5"
#define FLD_PS_MODE "ps_mode"
#define FLD_PS_SLEEP_INTERVAL "ps_interval"
#define FLD_PS_WAKEUP_ACTION "ps_wakup_action"
#define FLD_PS_VIP "pd_vip"
#define FLD_PS_TIMER1 "ps_timer1"
#define FLD_PS_TIMER2 "ps_timer2"
#define FLD_PS_TIMER3 "ps_timer3"
#define FLD_MSWITCH_ACTION "mswitch_action"
#define FLD_MSWITCH_VIP "mswitch_vip"
#define FLD_TEST_RESULT "test_result"
#define FLD_TEST_BATT "test_batt"
#define FLD_SW_VER "sw_ver"
#define FLD_HW_VER "hw_ver"
#define FLD_LED "led"
#define FLD_SMS_MODE "sms_mode"
#define FLD_TZ_SIGN "tz_sign"
#define FLD_TZ_H "tz_h"
#define FLD_TZ_M "tz_m"
#define FLD_SLEEP_ACTION "sleep_action"
#define FLD_REC_MODE "rec_mode"
#define FLD_REC_TIMER "rec_timer"
#define FLD_REC_DIST "rec_dist"
#define FLD_REC_NUMBER "rec_number"
#define FLD_REC_GPS_FIX "rec_gps_fix"
#define FLD_REC_HEADING "rec_heading"
#define FLD_GSENS "gsens"
#define FLD_GFEN_STATUS "gfen_status"
#define FLD_GFEN_RADIUS "gfen_radius"
#define FLD_GFEN_ZONE "gfen_zone"
#define FLD_GFEN_ACTION "gfen_action"
#define FLD_GFEN_VIP "gfen_vip"

#define FLD_GFEN_EVENT_STATUS "gfen_event_status_"
#define FLD_GFEN_EVENT_LON "gfen_event_lon_"
#define FLD_GFEN_EVENT_LAT "gfen_event_lat_"
#define FLD_GFEN_EVENT_RADIUS "gfen_event_radius_"
#define FLD_GFEN_EVENT_ZONE "gfen_event_zone_"
#define FLD_GFEN_EVENT_ACTION "gfen_event_action_"
#define FLD_GFEN_EVENT_VIP "gfen_event_vip_"
#define FLD_GFEN_EVENT_ID "gfen_event_ID_"

#define FLD_ERR_STR "??"

/**
 * Utility macro get the field value from the associative array
 * and give an predefined error string in case the value does not exist
 */
#define GET(f) assoc_get2(device_info, f, FLD_ERR_STR)

/**
 * Callback for device command list. Such a callback is ued to to special processing
 * needed to handle the field values before display.
 */
typedef int (*cb_extract_fields_t)(struct splitfields *, size_t);

/**
 * Structure for the creation of an array of all the commands needed to extract
 * all the information from the device
 */
struct get_dev_info_command_t {
    char *cmd_name;
    size_t num_reply_fields;
    char *fld_names[10];
    cb_extract_fields_t cb_extract;
};

/* Forward declaration */
int
extract_logged_num_and_dates(struct splitfields *flds, size_t idx);

/**
 * Lists all commands to send the device and specifies the name of where to store each reply
 * fields. The last argument is an optional callback that is used to do special processing
 * of the reply to prepare it for display.
 * The result of each device command call is stored in an associative array with the fields
 * named as specified in the list.
 */
struct get_dev_info_command_t dev_report_cmd_list[] = {

    /* CMD, num returned args, {named assoc pair to store values}, callback to extract values  */
    {"config", 3, {FLD_DEVICE_ID, FLD_DEVICE_PWD, FLD_SIM_PIN}, NULL},
    {"dlrec", 2, {FLD_LOGGED_NUM, FLD_LOGGED_DATES}, extract_logged_num_and_dates},
    {"comm", 10, {FLD_COMM_SELECT, FLD_SMS_BASE, FLD_CSD_BASE, FLD_GPRS_APN, FLD_GPRS_USER, FLD_GPRS_PWD, FLD_GPRS_SERVER_IP, FLD_GPRS_SERVER_PORT, FLD_GPRS_KEEP_ALIVE, FLD_GPRS_DNS}, NULL},
    {"roam",1,{FLD_ROAM}, NULL},
    {"track",7,{FLD_TRACK_MODE, FLD_TRACK_TIMER, FLD_TRACK_DIST, FLD_TRACK_NUMBER, FLD_TRACK_GPS_FIX, FLD_TRACK_COMMSELECT, FLD_TRACK_HEADING}, NULL},
    {"lowbatt", 2, {FLD_LOWBATT_REPORT, FLD_LOWBATT_VIP}, NULL},
    {"phone", 2, {FLD_VLOC_STATUS, FLD_VLOC_VIP}, NULL},
    {"imei", 1, {FLD_IMEI}, NULL},
    {"sim", 1, {FLD_SIM_ID}, NULL},
    {"vip", 5, {FLD_VIP1,FLD_VIP2,FLD_VIP3,FLD_VIP4,FLD_VIP5}, NULL},
    {"ps", 7, {FLD_PS_MODE,FLD_PS_SLEEP_INTERVAL,FLD_PS_WAKEUP_ACTION,FLD_PS_VIP,FLD_PS_TIMER1,FLD_PS_TIMER2,FLD_PS_TIMER3}, NULL},
    {"mswitch", 2, {FLD_MSWITCH_ACTION, FLD_MSWITCH_VIP}, NULL},
    {"ver", 2, {FLD_SW_VER, FLD_HW_VER}, NULL},
    {"led", 1, {FLD_LED}, NULL},
    {"sms", 1, {FLD_SMS_MODE}, NULL},
    {"tz", 3, {FLD_TZ_SIGN, FLD_TZ_H, FLD_TZ_M}, NULL},
    {"sleep", 1, {FLD_SLEEP_ACTION}, NULL},
    {"rec", 6, {FLD_REC_MODE, FLD_REC_TIMER, FLD_REC_DIST, FLD_REC_NUMBER, FLD_REC_GPS_FIX, FLD_REC_HEADING}, NULL},        
    {"sens", 1, {FLD_GSENS}, NULL},
    {"gfen", 5, {FLD_GFEN_STATUS,FLD_GFEN_RADIUS,FLD_GFEN_ZONE,FLD_GFEN_ACTION,FLD_GFEN_VIP}, NULL},
    {"test", 2, {FLD_TEST_RESULT, FLD_TEST_BATT}, NULL},
    {NULL, 0, {NULL}, NULL}
    
};

/**
 * All responses collected from the device is stored in the associative info array
 */
assoc_array_t device_info = NULL;

/**
 * The title of the device as shown in the page header on each page in the report
 */
char report_header_title[128] = {0};


// Silent gcc about unused args and vars in the callback functions
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

/**
 * Send the specified command to the device and get the replies. The reply is splitted in the
 * different fields in the reply.
 * @param cli_info Client context
 * @param dev_srvcmd The command to send to the device
 * @param flds The returned reply splitted in the different fields
 * @param event_id Only used when the command is GFEVT and in that case hold the event id
 * @return 0 on success, -1 on faiure
 */
int
send_command_get_replies(struct client_info *cli_info, char *dev_srvcmd, struct splitfields *flds, size_t event_id) {
    logmsg(LOG_DEBUG,"Running send_command_get_replies(%s) for PDF report",dev_srvcmd);
    char reply[1024];
    if( event_id ) {
        snprintf(reply,sizeof(reply),"%zu",event_id);
    }
    if( -1 == send_cmdquery_reply(cli_info, dev_srvcmd, reply, sizeof(reply)) ) {
        logmsg(LOG_DEBUG,"Failed send_cmdquery_reply()");
        return 0;
    }
    _Bool isok;
    char cmdname[16];
    char devtag[16];
    int rc = extract_devcmd_reply(reply, &isok, cmdname, devtag, flds);    
    if (rc) {        
        logmsg(LOG_ERR, "Incomplete reply from device command (%s): \"%s\" ", dev_srvcmd, reply);
        rc = -1;
    } else {

        logmsg(LOG_DEBUG,"Extracted fields: cmdname=%s, devtag=%s",cmdname, devtag);
        
        if (!isok) {
            // Device responded with an error code
            int errcode = xatoi(flds->fld[0]);
            char *errstr;
            device_strerr(errcode, &errstr);
            logmsg(LOG_ERR, "Device error reply for %s: (%d : %s)", dev_srvcmd, errcode, errstr);
            rc = -1;
        } else {    
            rc = 0;
        }
    }
    return rc;
}

/**
 * Make the returned dates and number of location logged on the device  more
 * human friendly to read
 * @param flds
 * @param idx
 * @return 0 on success, -1 on failure
 */
int
extract_logged_num_and_dates(struct splitfields *flds, size_t idx) {
    // $OK:DLREC[+TAG]=17254(20140107231903-20140109231710)  
    char *ptr=flds->fld[0];
    char number[8], *nptr;
    char dates[128], *dptr;
    nptr=number;

    // Exatrct the number of logs
    while( *ptr && *ptr!='(' ) {
        *nptr++ = *ptr++;
    }
    *nptr='\0';
    if( '(' == *ptr ) {
        // Extract the dates. We insert a space between the date and times to make
        // it more readable
        ptr++;
        dptr=dates;      
        int cnt=0;
        while( *ptr && *ptr!=')' ) {
            *dptr++ = *ptr++;
            if( cnt==3 || cnt==5 || cnt==18 || cnt==20 ) {
                *dptr++ = '-';
            }
            if( cnt==9 || cnt==11 || cnt==24 || cnt==26 ) {
                *dptr++ = ':';
            }
            if( cnt==7 || cnt==13 || cnt==14 || cnt==22 ) {
                *dptr++ = ' ';
            }
            cnt++;
        }
        *dptr='\0';
        if( *ptr != ')' ) {
            return -1;
        }
        else {
            assoc_put(device_info,dev_report_cmd_list[idx].fld_names[0],number);
            assoc_put(device_info,dev_report_cmd_list[idx].fld_names[1],dates);            
            return 0;
        }
    } else {
        return  -1;
    }        
}

//#define USE_FAKE_DEVICE_DATA_FOR_REPORT 0

/**
 * Initialize the model by reading all necessary values from the connected device
 * @param tag Table tag which is actually the client_info
 * @return 0 on success, -1 otherwise
 */
int
init_model_from_device(void *tag) {
    
    logmsg(LOG_DEBUG,"Running run_config_cmd() for PDF report");
    struct client_info *cli_info = (struct client_info *)tag;
    char reply[128];

    device_info = assoc_new(128);
    
//    if( USE_FAKE_DEVICE_DATA_FOR_REPORT ) {
//        assoc_import_from_json_file(device_info,"/tmp/export.json");
//        return 0;
//    }
    
    size_t i=0;
    struct splitfields flds;
    size_t percent=0; // Percent finished
    _writef(cli_info->cli_socket,"[%zu%%].",percent);
    
    while( dev_report_cmd_list[i].cmd_name ) {
        
        logmsg(LOG_DEBUG,"Running \"%s\"", dev_report_cmd_list[i].cmd_name);

        int rc = send_command_get_replies(cli_info, dev_report_cmd_list[i].cmd_name, &flds,0);

        if( 0 == rc ) {
            // Device normal response            
                        
            if( dev_report_cmd_list[i].cb_extract ) {
                // We need some special processing to extract the wanted information
                // The callback is responsible for extracting and adding the necessary fields
                // to the associative array
                dev_report_cmd_list[i].cb_extract(&flds,i);
            } else {
                
                if( flds.nf ==  dev_report_cmd_list[i].num_reply_fields ) {
                    // Extract all responses and store them i the assoc array
                    for( size_t j=0; j < flds.nf; j++) {
                        logmsg(LOG_DEBUG,"Storing ( %s : %s )",dev_report_cmd_list[i].fld_names[j],flds.fld[j]);
                        assoc_put(device_info,dev_report_cmd_list[i].fld_names[j],flds.fld[j]);
                    }                
                }else {
                    logmsg(LOG_ERR,"Expected %zu fields in reply from %s but found %zu",
                            dev_report_cmd_list[i].num_reply_fields,dev_report_cmd_list[i].cmd_name,flds.nf);
                }
            }
            
        } else {
            logmsg(LOG_ERR,"Failed command %s in report generation",dev_report_cmd_list[i].cmd_name);
            _writef(cli_info->cli_socket,"\nFaile to extract information from device. Please try again!");
            assoc_destroy(device_info);
            return -1;
        }
                
        if( 0==strcmp("test",dev_report_cmd_list[i].cmd_name) ) {
            usleep(5000);
        } else {
            usleep(2000);
        }
        
        i++;
        if( 0 == i%7  && percent < 90 ) {
            percent += 10;
            _writef(cli_info->cli_socket,"[%zu%%].",percent);
        }

    }
    
    // -----------------------
    // Extract all GFEVT.
    // -----------------------
    char cmdbuf[32];
    char namebuf[64];
    for(size_t event_id=50; event_id < 100; event_id++) {
        snprintf(cmdbuf,sizeof(cmdbuf),"gfevt");
        int rc = send_command_get_replies(cli_info, cmdbuf, &flds, event_id);

        if( 0 == rc ) {
            
            if( 8 == flds.nf ) {
                snprintf(namebuf,sizeof(namebuf),"%s%zu",FLD_GFEN_EVENT_ID, event_id);
                assoc_put(device_info,namebuf,flds.fld[0]);
                
                snprintf(namebuf,sizeof(namebuf),"%s%zu",FLD_GFEN_EVENT_STATUS, event_id);
                assoc_put(device_info,namebuf,flds.fld[1]);
                
                snprintf(namebuf,sizeof(namebuf),"%s%zu",FLD_GFEN_EVENT_LON, event_id);
                assoc_put(device_info,namebuf,flds.fld[2]);

                snprintf(namebuf,sizeof(namebuf),"%s%zu",FLD_GFEN_EVENT_LAT, event_id);
                assoc_put(device_info,namebuf,flds.fld[3]);

                snprintf(namebuf,sizeof(namebuf),"%s%zu",FLD_GFEN_EVENT_RADIUS, event_id);
                assoc_put(device_info,namebuf,flds.fld[4]);

                snprintf(namebuf,sizeof(namebuf),"%s%zu",FLD_GFEN_EVENT_ZONE, event_id);
                assoc_put(device_info,namebuf,flds.fld[5]);

                snprintf(namebuf,sizeof(namebuf),"%s%zu",FLD_GFEN_EVENT_ACTION, event_id);
                assoc_put(device_info,namebuf,flds.fld[6]);

                snprintf(namebuf,sizeof(namebuf),"%s%zu",FLD_GFEN_EVENT_VIP, event_id);
                assoc_put(device_info,namebuf,flds.fld[7]);

            } else {
                
                logmsg(LOG_ERR,"Expected %d fields in reply from %s but found %zu",8,cmdbuf,flds.nf);
                
            }
            
        } else {
            
            logmsg(LOG_ERR,"Failed command %s in report generation",cmdbuf);
            
        }
        
        if( 0 == (event_id-49)%7  && percent < 90 ) {
            percent += 10;
            _writef(cli_info->cli_socket,"[%zu%%].",percent);
        }
      
    }
    
    static char date_buf[64];
    time_t t = time(NULL);
    ctime_r(&t,date_buf);
    date_buf[strlen(date_buf)-2]='\0'; // Get rid of the '\n'
    assoc_put(device_info,"generated_time",date_buf);

    _writef(cli_info->cli_socket,"[100%%]\n");
    
    return 0;
}

/**
 * Export the associative array to a text files 
 * @param fname Filename to store the exported values
 * @return  0 on success, -1 on failure
 */
int
export_model_to_json(char *fname) {
    char filename[256];
    const size_t maxlen=100*1024;
    char *buf=calloc(maxlen,sizeof(char));
    if( NULL==buf )
        return -1;
    assoc_sort(device_info);
    assoc_export_to_json(device_info,buf,maxlen);    
    
    snprintf(filename,sizeof(filename),"%s_%s.json",fname,assoc_get(device_info,FLD_DEVICE_ID));
    FILE *fp=fopen(filename,"w");
    if( fp==NULL ) {
        logmsg(LOG_ERR,"Cannot open file for JSON export \"%s\" (%d : %s)",fname,errno,strerror(errno));
        free(buf);
        return -1;
    }
    fprintf(fp,"%s\n",buf);
    fclose(fp);
    logmsg(LOG_DEBUG,"Saved device info to \"%s\"",filename);    
    free(buf);
    return 0;

}

/**
 * Cleanup all dynamic memory used for extracting the information from the device
 */
void
cleanup_and_free_model(void) {
    assoc_destroy(device_info);
}

/**
 * Table cell value callback used in table to get the header title
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return 
 */
char *
cb_header_title(void *tag, size_t r, size_t c) {
    static char buf[128];
    if( *report_header_title && strnlen(report_header_title,sizeof(report_header_title)) > 1 )
        snprintf(buf,sizeof(buf),"%s",report_header_title);
    else
        snprintf(buf,sizeof(buf),"%s","Device report");
    return buf;
}

/* ===========================================================================
 * Section of cell value callbacks used with the view to get what value
 * to display in the different cells in the tables
 * 
 */

/**
 * Table cell value callback to get the device ID
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_DEVICE_ID(void *tag, size_t r, size_t c) {
    return GET(FLD_DEVICE_ID);
}

/**
 * Table cell value callback to get the device nick name from the database if
 * it exists. Empty value otherwise.
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_DEVICE_nick(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char nick[16];
    if( 0 == db_get_nick_from_devid(assoc_get(device_info, FLD_DEVICE_ID), nick) ) {
        return nick;
    } else {
        return "";
    }    
}

/**
 * Table cell value callback to get the device PIN as used in each command
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_DEVICE_PIN(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_DEVICE_PWD);
}

/**
 * Table cell value callback to return the SW version in the device
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_DEVICE_SW_VER(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_SW_VER);
}

/**
 * Table cell value callback to get the timezone value
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_DEVICE_TZ(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%s%s:%s",GET(FLD_TZ_SIGN),GET(FLD_TZ_H),GET(FLD_TZ_M));
    return buf;
}

/**
 * Table cell value callback to status of the LED indicator light on the device
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
_Bool
cb_DEVICE_LED(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return atoi(GET(FLD_LED));
}

/**
 * Table cell value callback to get the status of the RA (remove/Attach) micro switch
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_DEVICE_RA(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%s,%s",GET(FLD_MSWITCH_ACTION),GET(FLD_MSWITCH_VIP));
    return buf;
}

/**
 * Table cell value callback to get the value of the accelerometer sensitive
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a positive integer
 */
size_t
cb_DEVICE_GSENS(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return atoi(GET(FLD_GSENS));
}

/**
 * Table cell value callback to get the value of the last device test
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_DEVICE_TEST(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[32];
    if( 0==cmd_test_reply_to_text(GET(FLD_TEST_RESULT), buf, sizeof(buf)) ) {
        assoc_update(device_info,FLD_TEST_RESULT,buf);
        return buf;
    } else {
        return FLD_ERR_STR;
    }
}

/**
 * Table cell value callback to get all the VIP numbers. The VIP number to extract is
 * identified with the column number.
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_VIP_no(void *tag, size_t r, size_t c) {
    switch( c ) {
        case 0:
            return GET(FLD_VIP1);
            break;
        case 1:
            return GET(FLD_VIP2);
            break;
        case 2:
            return GET(FLD_VIP2);
            break;
        case 3:
            return GET(FLD_VIP3);
            break;
        case 4:
            return GET(FLD_VIP5);
            break;            
        default:
            return FLD_ERR_STR;
    }    
}

/**
 * Table cell value callback to get the SIM id
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_SIM_ID(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_SIM_ID);
}

/**
 * Table cell value callback to get the SIM pin number
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_SIM_PIN(void *tag, size_t r, size_t c) {
    return GET(FLD_SIM_PIN);
}

/**
 * Table cell value callback to setting of the Roaming
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a boolean (True if roaming is enabled)
 */
_Bool
cb_SIM_ROAMING(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_ROAM);
}

/**
 * Table cell value callback to setting of the power mode
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_POWER_MODE(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[128];
    translate_cmd_argval_to_string("ps", 0, GET(FLD_PS_MODE), buf, sizeof(buf));
    assoc_update(device_info,FLD_PS_MODE,buf);
    return buf;
}

/**
 * Table cell value callback to get the wakeup interval
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_POWER_INTERVAL(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_PS_SLEEP_INTERVAL);
}

/**
 * Table cell value callback to set the VIP numbers to notify
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_POWER_VIP(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_PS_VIP);
}
/**
 * Table cell value callback to get the different timer values. The timer to
 * get is identified with the column id
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_POWER_TIMER(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    if( 0==c )
        return GET(FLD_PS_TIMER1);
    else if( 1==c )
        return GET(FLD_PS_TIMER2);
    else 
        return GET(FLD_PS_TIMER3);    
}

/**
 * Table cell value callback to get the wakeup report setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_POWER_WAKEUP(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_PS_WAKEUP_ACTION);
}

/**
 * Table cell value callback to get the sleep report setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_POWER_SLEEP(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_SLEEP_ACTION);
}

/**
 * Table cell value callback to get the battery voltage
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_BATTERY_VOLTAGE(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%sV",GET(FLD_TEST_BATT));
    return buf;    
}

/**
 * Table cell value callback to extract the battery percentage of remaining energy
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a float
 */
float
cb_BATTERY_PERCENT(void *tag, size_t r, size_t c) {
    // Maximum voltage is 4.2 and batery low warning is triggered at 3.7
    // We therefore assume that the possible range for the battery is 3.6 - 4.2
    // where 3.6 corresponds to 0% and 4.2 to 100%
    struct client_info *cli_info = (struct client_info *)tag;
    double volt = atof(GET(FLD_TEST_BATT));
    double maxvolt = 4.2;
    double minvolt = 3.6;
    return (float)((volt-minvolt)/(maxvolt-minvolt));
}

/**
 * Table cell value callback to get the low battery report setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_BATTERY_LOW(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%s,%s",GET(FLD_LOWBATT_REPORT),GET(FLD_LOWBATT_VIP));
    return buf;
}


/**
 * Table cell value callback to get the GSM communication mode setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GSM_MODE(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[128];
    translate_cmd_argval_to_string("comm", 0, GET(FLD_COMM_SELECT), buf, sizeof(buf));
    assoc_update(device_info,FLD_COMM_SELECT,buf);
    return buf;
}

/**
 * Table cell value callback to get SMS mode (PDU or Text)
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GSM_SMS(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[64];
    translate_cmd_argval_to_string("sms", 0, GET(FLD_SMS_MODE), buf, sizeof(buf));
    assoc_update(device_info,FLD_SMS_MODE,buf);
    return buf;        
}

/**
 * Table cell value callback to get SMS base number (of specified)
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GSM_SMS_NBR(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_SMS_BASE);
}

/**
 * Table cell value callback to get CSD base number
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GSM_CSD_NBR(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_CSD_BASE);
}

/**
 * Table cell value callback to get the status of location by phone call
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GSM_LOCATION(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%s,%s",GET(FLD_VLOC_STATUS),GET(FLD_VLOC_VIP));
    return buf;
}

/**
 * Table cell value callback to get the IMEI number
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GSM_IMEI(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_IMEI);
}


/**
 * Table cell value callback to get the APN of the GPRS setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GPRS_APN(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_APN);
}

/**
 * Table cell value callback to get the name of the server to send back information to
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GPRS_server(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_SERVER_IP);
}
/**
 * Table cell value callback to get server TCIP/IP port to send back info to
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GPRS_port(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_SERVER_PORT);
}

/**
 * Table cell value callback to get GPRS user
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GPRS_user(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_USER);
}

/**
 * Table cell value callback to get GPRS password
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GPRS_pwd(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_PWD);
}

/**
 * Table cell value callback to get the DNS server used
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GPRS_DNS(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_DNS);
}

/**
 * Table cell value callback to get keep alive interval
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_GPRS_keep_alive(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_KEEP_ALIVE);
}

/**
 * Table cell value callback to get location logging setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LLOG_mode(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[128];
    translate_cmd_argval_to_string("rec", 0, GET(FLD_REC_MODE), buf, sizeof(buf));
    assoc_update(device_info,FLD_REC_MODE,buf);
    return buf;
}

/**
 * Table cell value callback to get location logging timer
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LLOG_timer(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_REC_TIMER);
}

/**
 * Table cell value callback to get distance trigger setting for location logging
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LLOG_dist(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_REC_DIST);
}

/**
 * Table cell value callback to get number of saved location
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LLOG_number(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_REC_NUMBER);
}

/**
 * Table cell value callback to get heading trigger setting for location logging
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LLOG_heading(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_REC_HEADING);
}

/**
 * Table cell value callback to get setting of waiting for GPS fix before logging
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a boolean value
 */
_Bool
cb_LLOG_waitGPS(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return atoi(GET(FLD_REC_GPS_FIX));
}


/**
 * Table cell value callback to get location logging setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LTRACK_mode(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[128];
    translate_cmd_argval_to_string("track", 0, GET(FLD_TRACK_MODE), buf, sizeof(buf));
    assoc_update(device_info,FLD_TRACK_MODE,buf);
    return buf;
}

/**
 * Table cell value callback to get communication setting for location tracking
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LTRACK_commselect(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[128];
    translate_cmd_argval_to_string("track", 5, GET(FLD_TRACK_COMMSELECT), buf, sizeof(buf));
    assoc_update(device_info,FLD_TRACK_COMMSELECT,buf);
    return buf;
}

/**
 * Table cell value callback to get location tracking timer
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LTRACK_timer(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TRACK_TIMER);
}

/**
 * Table cell value callback to get the distance trigger value for location tracking
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LTRACK_dist(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TRACK_DIST);
}

/**
 * Table cell value callback to get the number of location points to track
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LTRACK_number(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TRACK_NUMBER);
}

/**
 * Table cell value callback to get the heading trigger value for location tracking
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a string
 */
char *
cb_LTRACK_heading(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TRACK_HEADING);
}

/**
 * Table cell value callback to get setting of waiting for GPS fix before tracking
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a boolean value
 */
_Bool
cb_LTRACK_waitGPS(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return atoi(GET(FLD_TRACK_GPS_FIX));
}

/**
 * Table cell value callback to get the Geo-fence status setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a boolean value
 */
_Bool
cb_GFENCE_status(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[128];
    translate_cmd_argval_to_string("gfen", 0, GET(FLD_GFEN_STATUS), buf, sizeof(buf));
    assoc_update(device_info,FLD_GFEN_STATUS,buf);    
    return atoi(GET(FLD_GFEN_STATUS));
}

char *
cb_GFENCE_lat(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "N/A";
}

char *
cb_GFENCE_lon(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "N/A";
}

/**
 * Table cell value callback to get the Geo-fence radius setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a boolean value
 */
char *
cb_GFENCE_radius(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GFEN_RADIUS);
}

/**
 * Table cell value callback to get the Geo-fence zone setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a boolean value
 */
char *
cb_GFENCE_zone(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[128];
    translate_cmd_argval_to_string("gfen", 2, GET(FLD_GFEN_ZONE), buf, sizeof(buf));
    assoc_update(device_info,FLD_GFEN_ZONE,buf);    
    return buf;    
}

/**
 * Table cell value callback to get the Geo-fence action setting
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a boolean value
 */
char *
cb_GFENCE_action(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%s,%s",GET(FLD_GFEN_ACTION),GET(FLD_GFEN_VIP));
    return buf;    
}

/**
 * Table cell value callback to get the Geo-fence VIP numbers
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a boolean value
 */
char *
cb_GFENCE_vip(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GFEN_VIP);
}

/**
 * Table cell value callback to get the number of saved locations in the device
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a boolean value
 */
char *
cb_LOGGED_number(void *tag, size_t r, size_t c) {
    return GET(FLD_LOGGED_NUM);
}

/**
 * Table cell value callback to get the start and end dates for the stored locations
 * on the device
 * @param tag Table tag
 * @param r Row where value will be place
 * @param c Column where value will be place
 * @return value as a boolean value
 */
char *
cb_LOGGED_dates(void *tag, size_t r, size_t c) {
    size_t *event_id = (size_t *)tag;
    return GET(FLD_LOGGED_DATES);
}

char *
cb_GFENCE_EVENT_ID(void *tag, size_t r, size_t c) {
    size_t *event_id = (size_t *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%zu",*event_id);
    return buf;
}

_Bool
cb_GFENCE_EVENT_status(void *tag, size_t r, size_t c) {
    size_t *event_id = (size_t *)tag;
    static char buf[32];
    snprintf(buf,sizeof(buf),"%s%zu",FLD_GFEN_EVENT_STATUS,*event_id);    
    return atoi(GET(buf));
}

char *
cb_GFENCE_EVENT_lat(void *tag, size_t r, size_t c) {
    size_t *event_id = (size_t *)tag;
    static char buf[32];
    snprintf(buf,sizeof(buf),"%s%zu",FLD_GFEN_EVENT_LAT,*event_id);    
    return GET(buf);
}

char *
cb_GFENCE_EVENT_lon(void *tag, size_t r, size_t c) {
    size_t *event_id = (size_t *)tag;
    static char buf[32];
    snprintf(buf,sizeof(buf),"%s%zu",FLD_GFEN_EVENT_LON,*event_id);    
    return GET(buf);

}

char *
cb_GFENCE_EVENT_radius(void *tag, size_t r, size_t c) {
    size_t *event_id = (size_t *)tag;
    static char buf[32];
    snprintf(buf,sizeof(buf),"%s%zu",FLD_GFEN_EVENT_RADIUS,*event_id);    
    return GET(buf);

}

char *
cb_GFENCE_EVENT_zone(void *tag, size_t r, size_t c) {
    size_t *event_id = (size_t *)tag;
    static char buf[32];
    snprintf(buf,sizeof(buf),"%s%zu",FLD_GFEN_EVENT_ZONE,*event_id);    
    
    static char buf2[128];
    translate_cmd_argval_to_string("gfen", 2, GET(buf), buf2, sizeof(buf2));
    assoc_update(device_info,buf,buf2);    
    return buf2;
}

char *
cb_GFENCE_EVENT_action(void *tag, size_t r, size_t c) {
    size_t *event_id = (size_t *)tag;
    static char buf1[32];
    snprintf(buf1,sizeof(buf1),"%s%zu",FLD_GFEN_EVENT_ACTION,*event_id);    
    static char buf2[32];
    snprintf(buf2,sizeof(buf2),"%s%zu",FLD_GFEN_EVENT_VIP,*event_id);           
    static char res[16];
    snprintf(res,sizeof(res),"%s,%s",GET(buf1),GET(buf2));
    return res;        
}


/***/

char  *
cb_footer_text(void) {
    static char buf[256];
    snprintf(buf,sizeof(buf),"Generated by %s. Free software released under GPL 3.0. All rights reserved.",PACKAGE_STRING);
    return buf;
}

char *
cb_logo_text(void) {
    static char buf[256];
    snprintf(buf,sizeof(buf),"GM7 Device Report");
    return buf;
}

#pragma GCC diagnostic pop
