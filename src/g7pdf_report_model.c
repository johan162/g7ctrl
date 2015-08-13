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

#include "config.h"
#include "g7pdf_report_model.h"

char report_header_title[128] = {0};

// Silent gcc about unused args in the callback functions
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

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


/***/


char *
cb_DEVICE_ID(void *tag, size_t r, size_t c) {
    return "3000000001";
}

char *
cb_DEVICE_nick(void *tag, size_t r, size_t c) {
    return "nick-name";
}

char *
cb_DEVICE_PIN(void *tag, size_t r, size_t c) {
    return "pin";
}

char *
cb_DEVICE_SW_VER(void *tag, size_t r, size_t c) {
    return "sw ver";
}

char *
cb_DEVICE_TZ(void *tag, size_t r, size_t c) {
    return "tz";
}

_Bool
cb_DEVICE_LED(void *tag, size_t r, size_t c) {
    return 0;
}

char *
cb_DEVICE_RA(void *tag, size_t r, size_t c) {
    static char buf[8];
    snprintf(buf,sizeof(buf),"%s","2,4");
    return buf;
}

size_t
cb_DEVICE_GSENS(void *tag, size_t r, size_t c) {
    return 3;
}

char *
cb_DEVICE_TEST(void *tag, size_t r, size_t c) {
    return "PASSED";
}

/***/


char *
cb_SIM_ID(void *tag, size_t r, size_t c) {
    return "SIM id";
}

char *
cb_SIM_PIN(void *tag, size_t r, size_t c) {
    return "SIM pin";
}

_Bool
cb_SIM_ROAMING(void *tag, size_t r, size_t c) {
    return 1;
}


/***/


char *
cb_PH_MODE(void *tag, size_t r, size_t c) {
    return "PS mode";
}

char *
cb_PH_INTERVAL(void *tag, size_t r, size_t c) {
    return "PS interval";
}

char *
cb_PH_VIP(void *tag, size_t r, size_t c) {
    return "PS VIP";
}

char *
cb_PH_TIMER(void *tag, size_t r, size_t c) {
    return "PS Timer";
}

char *
cb_PH_REPORT_WAKE(void *tag, size_t r, size_t c) {
    return "PS wake";
}

char *
cb_POWER_SLEEP(void *tag, size_t r, size_t c) {
    return "1,2";
}


/***/


char *
cb_BATTERY_VOLTAGE(void *tag, size_t r, size_t c) {
    return "3.97V";
}

float
cb_BATTERY_PERCENT(void *tag, size_t r, size_t c) {
    return 0.6;
}

char *
cb_BATTERY_LOW(void *tag, size_t r, size_t c) {
    return "1,5";
}

//char *
//cb_BATTERY_VIP(void *tag, size_t r, size_t c) {
//    return "BATT VIP";
//}


/***/


char *
cb_GSM_MODE(void *tag, size_t r, size_t c) {
    return "GSM mode";
}

char *
cb_GSM_SMS(void *tag, size_t r, size_t c) {
    return "GSM sms";
}

char *
cb_GSM_SMS_NBR(void *tag, size_t r, size_t c) {
    return "GSM sms nbr";
}

char *
cb_GSM_CSD_NBR(void *tag, size_t r, size_t c) {
    return "GSM csd nbr";
}

char *
cb_GSM_LOCATION(void *tag, size_t r, size_t c) {
    return "1,3";
}


/***/


char *
cb_GPRS_APN(void *tag, size_t r, size_t c) {
    return "GPRS apn";
}

char *
cb_GPRS_server(void *tag, size_t r, size_t c) {
    return "aditus.asuscomm.com";
}

char *
cb_GPRS_port(void *tag, size_t r, size_t c) {
    return "9999";
}

char *
cb_GPRS_user(void *tag, size_t r, size_t c) {
    return "GPRS user";
}

char *
cb_GPRS_pwd(void *tag, size_t r, size_t c) {
    return "GPRS pwd";
}

char *
cb_GPRS_DNS(void *tag, size_t r, size_t c) {
    return "192.34.34.34";
}

char *
cb_GPRS_keep_alive(void *tag, size_t r, size_t c) {
    return "GPRS keep";
}


/***/


char *
cb_LLOG_mode(void *tag, size_t r, size_t c) {
    return "LLOG mode";
}

char *
cb_LLOG_timer(void *tag, size_t r, size_t c) {
    return "LLOG timer";
}

char *
cb_LLOG_dist(void *tag, size_t r, size_t c) {
    return "LLOG dist";
}

char *
cb_LLOG_number(void *tag, size_t r, size_t c) {
    return "LLOG lim";
}

char *
cb_LLOG_heading(void *tag, size_t r, size_t c) {
    return "LLOG head";
}

_Bool
cb_LLOG_waitGPS(void *tag, size_t r, size_t c) {
    return 1;
}


/***/


char *
cb_LTRACK_mode(void *tag, size_t r, size_t c) {
    return "LTRACK mode";
}

char *
cb_LTRACK_timer(void *tag, size_t r, size_t c) {
    return "LTRACK timer";
}

char *
cb_LTRACK_dist(void *tag, size_t r, size_t c) {
    return "LTRACK dist";
}

char *
cb_LTRACK_number(void *tag, size_t r, size_t c) {
    return "LTRACK lim";
}

char *
cb_LTRACK_heading(void *tag, size_t r, size_t c) {
    return "LTRACK head";
}

_Bool
cb_LTRACK_waitGPS(void *tag, size_t r, size_t c) {
    return 0;
}


/***/


char *
cb_GFENCE_status(void *tag, size_t r, size_t c) {
    return "GFENCE stat";
}

char *
cb_GFENCE_lat(void *tag, size_t r, size_t c) {
    return "GFENCE lat";
}

char *
cb_GFENCE_lon(void *tag, size_t r, size_t c) {
    return "GFENCE lon";
}

char *
cb_GFENCE_radius(void *tag, size_t r, size_t c) {
    return "GFENCE rad";
}

char *
cb_GFENCE_type(void *tag, size_t r, size_t c) {
    return "GFENCE type";
}

char *
cb_GFENCE_action(void *tag, size_t r, size_t c) {
    return "GFENCE act";
}


/***/


char *
cb_GFENCE_EVENT_ID(void *tag, size_t r, size_t c) {
    return "GFEVT ID";
}

char *
cb_GFENCE_EVENT_status(void *tag, size_t r, size_t c) {
    return "GFEVT stat";
}

char *
cb_GFENCE_EVENT_lat(void *tag, size_t r, size_t c) {
    return "GFEVT lat";
}

char *
cb_GFENCE_EVENT_lon(void *tag, size_t r, size_t c) {
    return "GFEVT lon";
}

char *
cb_GFENCE_EVENT_radius(void *tag, size_t r, size_t c) {
    return "GFEVT rad";
}

char *
cb_GFENCE_EVENT_type(void *tag, size_t r, size_t c) {
    return "GFEVT type";
}

char *
cb_GFENCE_EVENT_action(void *tag, size_t r, size_t c) {
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
