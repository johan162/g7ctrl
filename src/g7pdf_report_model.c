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

#include "config.h"
#include "g7ctrl.h"
#include "logger.h"
#include "xstr.h"
#include "g7sendcmd.h"
#include "g7cmd.h"
#include "g7pdf_report_model.h"
#include "assocarray.h"
#include "nicks.h"

char report_header_title[128] = {0};

// Silent gcc about unused args and vars in the callback functions
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

/***/

char *
cb_header_title(void *tag, size_t r, size_t c) {
    static char buf[128];
    if( *report_header_title && strnlen(report_header_title,sizeof(report_header_title)) > 4 )
        snprintf(buf,sizeof(buf),"%s",report_header_title);
    else
        snprintf(buf,sizeof(buf),"%s","Device report");
    return buf;
}


int
send_command_get_replies(struct client_info *cli_info, char *dev_cmd, struct splitfields *flds) {
    logmsg(LOG_DEBUG,"Running send_command_get_replies(%s) for PDF report",dev_cmd);
    char reply[1024];
    if( -1 == send_cmdquery_reply(cli_info, dev_cmd, reply, sizeof(reply)) ) {
        logmsg(LOG_DEBUG,"Failed send_cmdquery_reply()");
        return 0;
    }
    _Bool isok;
    char cmdname[16];
    char devtag[16];
    int rc = extract_devcmd_reply(reply, &isok, cmdname, devtag, flds);    
    if (rc) {        
        logmsg(LOG_ERR, "Incomplete reply from device command (%s): \"%s\" ", dev_cmd, reply);
        rc = -1;
    } else {

        logmsg(LOG_DEBUG,"Extracted fields: cmdname=%s, devtag=%s",cmdname, devtag);
        
        if (!isok) {
            // Device responded with an error code
            int errcode = xatoi(flds->fld[0]);
            char *errstr;
            device_strerr(errcode, &errstr);
            logmsg(LOG_ERR, "Device error reply for %s: (%d : %s)", dev_cmd, errcode, errstr);
            rc = -1;
        } else {    
            rc = 0;
        }
    }
    return rc;
}


typedef int (*cb_extract_fields_t)(struct splitfields *, size_t);

struct get_dev_info_command_t {
    char *cmd_name;
    size_t num_reply_fields;
    char *fld_names[10];
    cb_extract_fields_t cb_extract;
};



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
#define FLD_GFEN_INOUT "gfen_inout"
#define FLD_GFEN_ACTION "gfen_action"
#define FLD_GFEN_VIP "gfen_vip"

#define FLD_ERR_STR "??"

int
extract_logged_num_and_dates(struct splitfields *flds, size_t idx);

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
    {"gfen", 5, {FLD_GFEN_STATUS,FLD_GFEN_RADIUS,FLD_GFEN_INOUT,FLD_GFEN_ACTION,FLD_GFEN_VIP}, NULL},
    {"test", 2, {FLD_TEST_RESULT, FLD_TEST_BATT}, NULL},
    {NULL, 0, {NULL}, NULL}
    
};

// All responses collected in the associative info array
struct assoc_array_t *device_info = NULL;

int
extract_logged_num_and_dates(struct splitfields *flds, size_t idx) {
    // $OK:DLREC[+TAG]=17254(20140107231903-20140109231710)  
    char *ptr=flds->fld[0];
    char number[8], *nptr;
    char dates[64], *dptr;
    nptr=number;

    while( *ptr && *ptr!='(' ) {
        *nptr++ = *ptr++;
    }
    *nptr='\0';
    if( '(' == *ptr ) {
        ptr++;
        dptr=dates;        
        while( *ptr && *ptr!=')' ) {
            *dptr++ = *ptr++;
        }
        dptr='\0';
        if( *ptr != ')' ) 
            return -1;
        else {
            assoc_put(device_info,dev_report_cmd_list[idx].fld_names[0],number);
            assoc_put(device_info,dev_report_cmd_list[idx].fld_names[1],dates);            
            return 0;
        }
    } else {
        return  -1;
    }        
}


/**
 * Initialize the model by rading all necessary values from the connected device
 * @param tag Table tag which is actually the client_info
 * @return 0 on success, -1 otherwise
 */
int
init_model_from_device(void *tag) {
    logmsg(LOG_DEBUG,"Running run_config_cmd() for PDF report");
    struct client_info *cli_info = (struct client_info *)tag;
    char reply[128];

    device_info = assoc_new(128);
    
    size_t i=0;
    while( dev_report_cmd_list[i].cmd_name ) {
        
        logmsg(LOG_DEBUG,"Running \"%s\"", dev_report_cmd_list[i].cmd_name);

        struct splitfields flds;
        int rc = send_command_get_replies(cli_info, dev_report_cmd_list[i].cmd_name, &flds);

        if( 0 == rc ) {
            // Device normal response            
                        
            if( dev_report_cmd_list[i].cb_extract ) {
                // We need some special processing to extract the wanted information
                // The callback is reposnible for extracting and adding the necessary fields
                // to the associative array
                dev_report_cmd_list[i].cb_extract(&flds,i);
            } else {
                
                if( flds.nf ==  dev_report_cmd_list[i].num_reply_fields ) {
                    // Extract all responses and store them i the assocarray
                    for( size_t j=0; j < flds.nf; j++) {
                        assoc_put(device_info,dev_report_cmd_list[i].fld_names[j],flds.fld[j]);
                    }                
                }else {
                    logmsg(LOG_ERR,"Expected %zu fields in reply from %s but found %zu",
                            dev_report_cmd_list[i].num_reply_fields,dev_report_cmd_list[i].cmd_name,flds.nf);
                }
            }
            
        } else {
            logmsg(LOG_ERR,"Failed command %s in report generation",dev_report_cmd_list[i].cmd_name);
        }
        
        i++;
        
    }
    const size_t maxlen=10*1024;
    char *buf=calloc(maxlen,sizeof(char));
    assoc_to_json(device_info,buf,maxlen);
    //logmsg(LOG_DEBUG,"%s",buf);
    char fname[128];
    snprintf(fname,sizeof(fname),"/tmp/dev_%s.json",assoc_get(device_info,FLD_DEVICE_ID));
    FILE *fp=fopen(fname,"w");
    fprintf(fp,"%s\n",buf);
    fclose(fp);
    logmsg(LOG_DEBUG,"Saved JSON device info \"%s\"",fname);
    free(buf);
    return 0;
}

#define GET(f) assoc_get2(device_info, f, FLD_ERR_STR)

char *
cb_DEVICE_ID(void *tag, size_t r, size_t c) {
    return GET(FLD_DEVICE_ID);
}

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

char *
cb_DEVICE_PIN(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_DEVICE_PWD);
}

char *
cb_DEVICE_SW_VER(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_SW_VER);
}

char *
cb_DEVICE_TZ(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%s%s:%s",GET(FLD_TZ_SIGN),GET(FLD_TZ_H),GET(FLD_TZ_M));
    return buf;
}

_Bool
cb_DEVICE_LED(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_LED);
}

char *
cb_DEVICE_RA(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%s,%s",GET(FLD_MSWITCH_ACTION),GET(FLD_MSWITCH_VIP));
    return buf;
}

size_t
cb_DEVICE_GSENS(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return atoi(GET(FLD_GSENS));
}

char *
cb_DEVICE_TEST(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TEST_RESULT);
}

/***/

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

/***/
char *
cb_SIM_ID(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_SIM_ID);
}

char *
cb_SIM_PIN(void *tag, size_t r, size_t c) {
    return GET(FLD_SIM_PIN);
}

_Bool
cb_SIM_ROAMING(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_ROAM);
}


/***/


char *
cb_PH_MODE(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_PS_MODE);
}

char *
cb_PH_INTERVAL(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_PS_SLEEP_INTERVAL);
}

char *
cb_PH_VIP(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_PS_VIP);
}

char *
cb_PH_TIMER(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    if( 0==c )
        return GET(FLD_PS_TIMER1);
    else if( 1==c )
        return GET(FLD_PS_TIMER2);
    else 
        return GET(FLD_PS_TIMER3);    
}

char *
cb_POWER_WAKEUP(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "1,12";
}

char *
cb_POWER_SLEEP(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "1,2";
}


/***/


char *
cb_BATTERY_VOLTAGE(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TEST_BATT);
}

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

char *
cb_BATTERY_LOW(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%s,%s",GET(FLD_LOWBATT_REPORT),GET(FLD_LOWBATT_VIP));
    return buf;
}


/***/


char *
cb_GSM_MODE(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_COMM_SELECT);
}

char *
cb_GSM_SMS(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_SMS_MODE);
}

char *
cb_GSM_SMS_NBR(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_SMS_BASE);
}

char *
cb_GSM_CSD_NBR(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_CSD_BASE);
}

char *
cb_GSM_LOCATION(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    static char buf[16];
    snprintf(buf,sizeof(buf),"%s,%s",GET(FLD_VLOC_STATUS),GET(FLD_VLOC_VIP));
    return buf;
}

char *
cb_GSM_IMEI(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_IMEI);
}

/***/

char *
cb_GPRS_APN(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_APN);
}

char *
cb_GPRS_server(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_SERVER_IP);
}

char *
cb_GPRS_port(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_SERVER_PORT);
}

char *
cb_GPRS_user(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_USER);
}

char *
cb_GPRS_pwd(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_PWD);
}

char *
cb_GPRS_DNS(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_DNS);
}

char *
cb_GPRS_keep_alive(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GPRS_KEEP_ALIVE);
}


/***/


char *
cb_LLOG_mode(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_REC_MODE);
}

char *
cb_LLOG_timer(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_REC_TIMER);
}

char *
cb_LLOG_dist(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_REC_DIST);
}

char *
cb_LLOG_number(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_REC_NUMBER);
}

char *
cb_LLOG_heading(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_REC_HEADING);
}

_Bool
cb_LLOG_waitGPS(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return atoi(GET(FLD_REC_GPS_FIX));
}


/***/


char *
cb_LTRACK_mode(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TRACK_MODE);
}

char *
cb_LTRACK_timer(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TRACK_TIMER);
}

char *
cb_LTRACK_dist(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TRACK_DIST);
}

char *
cb_LTRACK_number(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TRACK_NUMBER);
}

char *
cb_LTRACK_heading(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_TRACK_HEADING);
}

_Bool
cb_LTRACK_waitGPS(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return atoi(GET(FLD_TRACK_GPS_FIX));
}


/***/


char *
cb_GFENCE_status(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GFEN_STATUS);
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

char *
cb_GFENCE_radius(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GFEN_RADIUS);
}

char *
cb_GFENCE_type(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GFEN_INOUT);
}

char *
cb_GFENCE_action(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return GET(FLD_GFEN_ACTION);
}


/***/

char *
cb_LOGGED_number(void *tag, size_t r, size_t c) {
    return GET(FLD_LOGGED_NUM);
}

char *
cb_LOGGED_dates(void *tag, size_t r, size_t c) {
    return GET(FLD_LOGGED_DATES);
}


/***/


char *
cb_GFENCE_EVENT_ID(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "GFEVT ID";
}

char *
cb_GFENCE_EVENT_status(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "GFEVT stat";
}

char *
cb_GFENCE_EVENT_lat(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "GFEVT lat";
}

char *
cb_GFENCE_EVENT_lon(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "GFEVT lon";
}

char *
cb_GFENCE_EVENT_radius(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "GFEVT rad";
}

char *
cb_GFENCE_EVENT_type(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "GFEVT type";
}

char *
cb_GFENCE_EVENT_action(void *tag, size_t r, size_t c) {
    struct client_info *cli_info = (struct client_info *)tag;
    return "GFEVT act";
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
