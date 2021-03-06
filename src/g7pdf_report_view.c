/* =========================================================================
 * File:        G7CTRL_PDF_VIEW.C
 * Description: PDF Device report. View formatting module.
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
#include <unistd.h>
#include <math.h>
#include <setjmp.h>
#include <hpdf.h>
#include <time.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#ifdef __APPLE__
#include <sys/utsname.h>
#else
#include <linux/utsname.h>
#endif

#include "libhpdftbl/hpdf_table.h"
#include "libhpdftbl/hpdf_errstr.h"
#include "libhpdftbl/hpdf_table_widget.h"
#include "g7pdf_report_model.h"
#include "g7pdf_report_view.h"
#include "g7ctrl.h"
#include "config.h"
#include "logger.h"
#include "g7config.h"
#include "libxstr/xstr.h"

// The output after running the program will be written to this file

#define TRUE 1
#define FALSE 0

static int
stroke_g7ctrl_report_table(hpdf_table_spec_t table_spec);

// Silent gcc about unused args in the callback functions
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// For simulated exception handling
jmp_buf _g7report_env;

// Global handlers to the HPDF document and page
HPDF_Doc pdf_doc;
HPDF_Page pdf_page;

static const HPDF_REAL slide_width=38;
static const HPDF_REAL slide_height=11;


void
cb_widget_draw_VIP_state(HPDF_Doc doc, HPDF_Page page, HPDF_REAL xpos, HPDF_REAL ypos, size_t VIP_idx) {

        const HPDF_RGBColor white = HPDF_COLOR_FROMRGB(255,255,255);
        const HPDF_RGBColor darkorange = HPDF_COLOR_FROMRGB(235,120,0);
        const HPDF_RGBColor grey = HPDF_COLOR_FROMRGB(170,170,170);
        //const HPDF_RGBColor darkgrey = HPDF_COLOR_FROMRGB(70,70,70);
        HPDF_RGBColor VIP_on_background = darkorange;
        HPDF_RGBColor VIP_off_background = grey;
        HPDF_RGBColor on_color = white;
        HPDF_RGBColor off_color = grey;

        char VIP_buf[4];
        snprintf(VIP_buf,sizeof(VIP_buf),"%zu",VIP_idx);
        const HPDF_REAL VIP_width=12;
        const HPDF_REAL VIP_height=12;
        const HPDF_REAL fsize=9;
        _Bool state[]={VIP_idx > 0};
        hpdf_table_widget_letter_buttons(doc, page, xpos, ypos, VIP_width, VIP_height,
                on_color, off_color,
                VIP_on_background, VIP_off_background,
                fsize,
                VIP_buf, state);

}


// G7ctrl specific combination widget to show the state of a property that needs
// to indicate wheter an "on" state sends back to server, logs message or both
void
cb_widget_draw_LS_VIP_state(HPDF_Doc doc, HPDF_Page page,
                     HPDF_REAL xpos, HPDF_REAL ypos, size_t device_status, size_t VIP_idx) {

    const HPDF_RGBColor green = HPDF_COLOR_FROMRGB(60,179,113);
    //const HPDF_RGBColor red = HPDF_COLOR_FROMRGB(210,42,0);
    const HPDF_RGBColor white = HPDF_COLOR_FROMRGB(255,255,255);
    //const HPDF_RGBColor black = HPDF_COLOR_FROMRGB(0,0,0);
    const HPDF_RGBColor grey = HPDF_COLOR_FROMRGB(180,180,180);
    //const HPDF_RGBColor lightgrey = HPDF_COLOR_FROMRGB(240,240,240);
    const HPDF_RGBColor darkgrey = HPDF_COLOR_FROMRGB(70,70,70);


    hpdf_table_widget_slide_button(doc, page, xpos+4, ypos+4, slide_width, slide_height, device_status);

    if( device_status ) {
        const HPDF_RGBColor on_background = green;
        const HPDF_RGBColor off_background = grey;
        const HPDF_RGBColor on_color = white;
        const HPDF_RGBColor off_color = darkgrey;

        const char *letters = "LS";
        _Bool state[2];
        state[0] = device_status & 1;
        state[1] = device_status & 2;
        const HPDF_REAL LS_width=26;
        const HPDF_REAL LS_height=12;
        const HPDF_REAL fsize=8;
        hpdf_table_widget_letter_buttons(doc, page, xpos+slide_width+10, ypos+4, LS_width, LS_height,
                on_color, off_color,
                on_background, off_background,
                fsize,
                letters, state);

        if( VIP_idx > 0 ) {
            cb_widget_draw_VIP_state(doc, page, xpos+slide_width+10+LS_width+8, ypos+4, VIP_idx);
        }

    }
}



/*
 * This holds holds the client information structure for the client that currently is
 * connected. It is used to set the "tag" information in the table to be able to pass
 * along the current client in the actual model callbacks (which fetches the data from 
 * the device).
 * This means that it is NOT thread safe and the report can only be called by one
 * client at a time. However, the edge case of multiple clients calling the report
 * function is very, very unlikely and this reduction in flexibility greatly simplifies
 * the overall code.
 */
static struct client_info *cli_info_for_callback;

/**
 * Callback function that is called just after a table has been created statically but befpre
 * the data is populated.
 * @param t The table
 */
static void 
cb_tbl_set_tag(hpdf_table_t t) {
    hpdf_table_set_tag(t,(void *)cli_info_for_callback);
}

/* ===============================================
 * DEVICE TABLE AND CALLBACKS
 */


void
cb_DEVICE_LED_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    hpdf_table_widget_slide_button(doc, page, xpos+10, ypos+4, slide_width, slide_height, cb_DEVICE_LED(tag,r,c));
}

void
cb_DEVICE_RA_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    const char *stat=cb_DEVICE_RA(tag,r,c);
    const size_t dev_stat = (stat[0]-'0');
    const size_t vip_idx = (stat[2]-'0');
    cb_widget_draw_LS_VIP_state(doc, page, xpos+10, ypos, dev_stat, vip_idx);
}

_Bool
cb_DEVICE_ID_style(void *tag, size_t r, size_t c, hpdf_text_style_t *style) {
    style->color = HPDF_COLOR_FROMRGB(150,60,0);
    style->font = HPDF_FF_COURIER_BOLD;
    style->fsize = 11;
    return TRUE;
}


void
cb_DEVICE_draw_gsens(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {

    const HPDF_RGBColor green = HPDF_COLOR_FROMRGB(60,179,113);

    const size_t num_segments=5;
    const size_t num_on_segments = cb_DEVICE_GSENS(tag, r, c);

    const HPDF_REAL meter_width = 35;
    const HPDF_REAL meter_height = height/1.8;
    const HPDF_REAL meter_xpos = xpos+width/3;
    const HPDF_REAL meter_ypos = ypos+4;

    hpdf_table_widget_strength_meter(doc, page,meter_xpos, meter_ypos, meter_width, meter_height,
                                     num_segments, green, num_on_segments);
}

void
cb_BATTERY_draw_segment(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {

    const HPDF_RGBColor red = HPDF_COLOR_FROMRGB(255,69,0);
    const HPDF_RGBColor yellow = HPDF_COLOR_FROMRGB(255,215,0);
    const HPDF_RGBColor green = HPDF_COLOR_FROMRGB(60,179,113);

    const size_t num_segments=10;
    double val_percent = round(cb_BATTERY_PERCENT(tag, r, c)*10.0)/10.0;

    // Sanity check 
    if( val_percent > 1.0 ) {
        logmsg(LOG_ERR,"Battery percent (%d%%) cannot be higher than 100%%, Check min/max values in config file",(int)floor(val_percent*100));
        val_percent = 0;
    }
     
    HPDF_RGBColor color;
    if( val_percent > 0.55 ) {
        color=green;
    } else if( val_percent > 0.25 ) {
        color=yellow;
    } else {
        color=red;
    }

    const HPDF_REAL segment_tot_width = 50;
    const HPDF_REAL segment_height = 10;
    const HPDF_REAL segment_xpos = xpos+45;//width/2;
    const HPDF_REAL segment_ypos = ypos+4;//height/2;

    const _Bool use_segment_meter = TRUE;


    if( use_segment_meter ) {
        hpdf_table_widget_segment_hbar(doc, page,segment_xpos, segment_ypos, segment_tot_width, segment_height,
                                       num_segments, color, val_percent, FALSE);
    } else {
        hpdf_table_widget_hbar(doc, page,segment_xpos, segment_ypos, segment_tot_width, segment_height,
                               color, val_percent, FALSE);
    }

}


void
cb_BATTERY_draw_low_warning(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    const char *stat=cb_BATTERY_LOW(tag,r,c);
    const size_t warning_stat = (stat[0]-'0');
    const size_t vip_idx = (stat[2]-'0');
    cb_widget_draw_LS_VIP_state(doc, page, xpos+10, ypos, warning_stat, vip_idx);

}

static int
_tbl_device(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback,content-style-callback, cell-callback
        {0,0,1,1,"Dev ID:",cb_DEVICE_ID,cb_DEVICE_ID_style,NULL},
        {0,1,1,1,"Nick name:",cb_DEVICE_nick,cb_DEVICE_ID_style,NULL},
        {0,2,1,1,"PIN:",cb_DEVICE_PIN,NULL,NULL},
        {0,3,1,2,"SW Ver:",cb_DEVICE_SW_VER,NULL,NULL},
        {1,0,1,2,"Removal alert:",NULL,NULL,cb_DEVICE_RA_draw_slide_button},
        {1,2,1,1,"Test:",cb_DEVICE_TEST,NULL,NULL},
        {1,3,1,1,"TZ:",cb_DEVICE_TZ,NULL,NULL},
        {1,4,1,1,"LED:",NULL,NULL,cb_DEVICE_LED_draw_slide_button},
        {2,0,1,2,"Battery low alert:",NULL,NULL,cb_BATTERY_draw_low_warning},
        {2,2,1,2,"Voltage:",cb_BATTERY_VOLTAGE,NULL,cb_BATTERY_draw_segment},
        {2,4,1,1,"G-Sensitivity:",NULL,NULL,cb_DEVICE_draw_gsens},        
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Device", 3, 5,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,                            /* A pointer to the specification of each row in the table */
        cb_tbl_set_tag         /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);

}



/* ===============================================
 * POWER SAVING TABLE AND CALLBACKS
 * ===============================================
 */


void
cb_POWER_sleep_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    const char *stat=cb_POWER_SLEEP(tag,r,c);
    const size_t dev_stat = (stat[0]-'0');
   cb_widget_draw_LS_VIP_state(doc, page, xpos+10, ypos, dev_stat, -1);
}

void
cb_POWER_wakeup_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    const char *stat=cb_POWER_WAKEUP(tag,r,c);
    const size_t dev_stat = (stat[0]-'0');
    const size_t vip_idx = (stat[2]-'0');
    cb_widget_draw_LS_VIP_state(doc, page, xpos+10, ypos, dev_stat, vip_idx);
}

_Bool
cb_POWER_mode_style(void *tag, size_t r, size_t c, hpdf_text_style_t *style) {    
    style->font = HPDF_FF_COURIER_BOLD;
    style->fsize = 9;
    return TRUE;
}

static int
_tbl_POWER(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback,content-style-callback, cell-callback
        {0,0,1,4,"Mode:",cb_POWER_MODE,cb_POWER_mode_style,NULL},
        {0,4,1,2,"Wakeup report:",NULL,NULL,cb_POWER_wakeup_draw_slide_button},        
        {1,0,1,1,"Interval:",cb_POWER_INTERVAL,NULL,NULL},
        {1,1,1,1,"Timer 1:",cb_POWER_TIMER,NULL,NULL},
        {1,2,1,1,"Timer 2:",cb_POWER_TIMER,NULL,NULL},
        {1,3,1,1,"Timer 3:",cb_POWER_TIMER,NULL,NULL},
        {1,4,1,2,"Sleep report:",NULL,NULL,cb_POWER_sleep_draw_slide_button},        
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Power handling", 2, 6,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_tbl_set_tag               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}


/* ===============================================
 * GSM TABLE AND CALLBACKS
 * ===============================================
 */

void
cb_GSM_LOCATION_draw_slide_vip(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    const char *stat=cb_GSM_LOCATION(tag,r,c);
    const size_t location_stat = (stat[0]-'0');
    const size_t vip_idx = (stat[2]-'0');

    hpdf_table_widget_slide_button(doc, page, xpos+5, ypos+4, slide_width, slide_height, location_stat);
    cb_widget_draw_VIP_state(doc, page, xpos+slide_width+5+5, ypos+4, vip_idx);
}

void
cb_GSM_ROAMING_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    hpdf_table_widget_slide_button(doc, page, xpos+10, ypos+4, slide_width, slide_height, cb_SIM_ROAMING(tag,r,c));
}

static int
_tbl_GSM(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback,content-style-callback, cell-callback
        {0,0,1,1,"Comm select:",cb_GSM_MODE,NULL,NULL},
        {0,1,1,1,"SMS PDU/Text:",cb_GSM_SMS,NULL,NULL},
        {0,2,1,1,"SMS Base No:",cb_GSM_SMS_NBR,NULL,NULL},
        {0,3,1,1,"CSD Base No:",cb_GSM_CSD_NBR,NULL,NULL},
        {1,0,1,2,"Location By Call:",NULL,NULL,cb_GSM_LOCATION_draw_slide_vip},
        {1,2,1,1,"Roaming:",NULL,NULL,cb_GSM_ROAMING_draw_slide_button},
        {1,3,1,1,"SIM Pin:",cb_SIM_PIN,NULL,NULL},
        {2,0,1,2,"IMEI:",cb_GSM_IMEI,NULL,NULL},
        {2,2,1,2,"SIM ID:",cb_SIM_ID,NULL,NULL},        
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "GSM", 3, 4,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_tbl_set_tag               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}

/* ===============================================
 * GSM TABLE AND CALLBACKS
 * ===============================================
 */

char *
cb_GPRS_server_port(void *tag, size_t r, size_t c) {
    static char buf[128];
    snprintf(buf,sizeof(buf),"%s:%s",cb_GPRS_server(tag,r,c),cb_GPRS_port(tag,r,c));
    return buf;
}

_Bool
cb_GPRS_txt_style(void *tag, size_t r, size_t c, hpdf_text_style_t *style) {    
    style->font = HPDF_FF_COURIER_BOLD;
    style->fsize = 9;
    return TRUE;
}


static int
_tbl_GPRS(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,3,"APN:",cb_GPRS_APN,cb_GPRS_txt_style,NULL},
        {0,3,1,3,"Server:",cb_GPRS_server_port,cb_GPRS_txt_style,NULL},
        {0,6,1,2,"DNS:",cb_GPRS_DNS,cb_GPRS_txt_style,NULL},
        {1,0,1,3,"User:",cb_GPRS_user,NULL,NULL},
        {1,3,1,3,"Password:",cb_GPRS_pwd,NULL,NULL},
        {1,6,1,2,"Keep alive:",cb_GPRS_keep_alive,NULL,NULL},
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "GPRS", 2, 8,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_tbl_set_tag               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}

/* ===============================================
 * VIP TABLE AND CALLBACKS
 * ===============================================
 */

static int
_tbl_VIP(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback,content-style-callback, cell-callback
        {0,0,1,1,"No 1:",cb_VIP_no,NULL,NULL},
        {0,1,1,1,"No 2:",cb_VIP_no,NULL,NULL},
        {0,2,1,1,"No 3:",cb_VIP_no,NULL,NULL},
        {0,3,1,1,"No 4:",cb_VIP_no,NULL,NULL},
        {0,4,1,1,"No 5:",cb_VIP_no,NULL,NULL},
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "VIP Numbers", 1, 5,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_tbl_set_tag               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}

/* ===============================================
 * LOCATION LOGGING TABLE AND CALLBACKS
 * ===============================================
 */

void
cb_LLOG_waitGPS_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    hpdf_table_widget_slide_button(doc, page, xpos+10, ypos+4, slide_width, slide_height, cb_LLOG_waitGPS(tag,r,c));
}

static int
_tbl_llog(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback,content-style-callback, cell-callback
        {0,0,1,5,"Mode:",cb_LLOG_mode,NULL,NULL},
        {1,0,1,1,"Timer:",cb_LLOG_timer,NULL,NULL},
        {1,1,1,1,"Dist:",cb_LLOG_dist,NULL,NULL},
        {1,2,1,1,"Limit:",cb_LLOG_number,NULL,NULL},
        {1,3,1,1,"Heading:",cb_LLOG_heading,NULL,NULL},
        {1,4,1,1,"Wait for GPS:",NULL,NULL,cb_LLOG_waitGPS_draw_slide_button},
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Location logging", 2, 5,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_tbl_set_tag               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}


/* ===============================================
 * LOCATION TRACKING TABLE AND CALLBACKS
 * ===============================================
 */

void
cb_LTRACK_waitGPS_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    hpdf_table_widget_slide_button(doc, page, xpos+10, ypos+4, slide_width, slide_height, cb_LTRACK_waitGPS(tag,r,c));
}

static int
_tbl_ltrack(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback,content-style-callback, cell-callback
        {0,0,1,3,"Mode:",cb_LTRACK_mode,NULL,NULL},
        {0,3,1,2,"Comm select:",cb_LTRACK_commselect,NULL,NULL},
        {1,0,1,1,"Timer:",cb_LTRACK_timer,NULL,NULL},
        {1,1,1,1,"Dist:",cb_LTRACK_dist,NULL,NULL},
        {1,2,1,1,"Limit:",cb_LTRACK_number,NULL,NULL},
        {1,3,1,1,"Heading:",cb_LTRACK_heading,NULL,NULL},
        {1,4,1,1,"Wait for GPS:",NULL,NULL,cb_LTRACK_waitGPS_draw_slide_button},
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Location tracking", 2, 5,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_tbl_set_tag               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}


/* ===============================================
 * GEO FENCE TABLE AND CALLBACKS
 * ===============================================
 */
void
cb_GEOFENCE_status_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    hpdf_table_widget_slide_button(doc, page, xpos+10, ypos+4, slide_width, slide_height, cb_GFENCE_status(tag,r,c));
}

void
cb_GFENCE_action_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    const char *stat=cb_GFENCE_action(tag,r,c);
    const size_t dev_stat = (stat[0]-'0');
    const size_t vip_idx = (stat[2]-'0');
    if( dev_stat <= 3 && vip_idx <= 5 ) {
        cb_widget_draw_LS_VIP_state(doc, page, xpos+10, ypos, dev_stat, vip_idx);
    } else {
        logmsg(LOG_ERR,"Values for gfen out of range (%c, %c)",stat[0],stat[2]);
    }
}

static int
_tbl_gfence(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback,content-style-callback, cell-callback
        {0,0,1,1,"Status:",NULL,NULL,cb_GEOFENCE_status_draw_slide_button},
        {0,1,1,1,"Radius:",cb_GFENCE_radius,NULL,NULL},
        {0,2,1,2,"Zone control:",cb_GFENCE_zone,NULL,NULL},
        {0,4,1,3,"Action:",NULL,NULL,cb_GFENCE_action_draw_slide_button},
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Geo fence", 1, 7,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_tbl_set_tag               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}

/* ====================================================================
 * Information on stored location on device
 */
static int
_tbl_LOGGED(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback,content-style-callback, cell-callback
        {0,0,1,1,"Number of locations:",cb_LOGGED_number,NULL,NULL},
        {0,1,1,2,"Start time:",cb_LOGGED_dates_start,NULL,NULL},
        {0,3,1,2,"End time:",cb_LOGGED_dates_end,NULL,NULL},
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Stored Locations", 1, 5,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_tbl_set_tag               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}


/*
 * TODO: Add tracking mode
 *
static int
_tbl_location_tracking_mode(void) {
    return 0;
}
*/

/* ===============================================
 * GEO FENCE EVENT TABLE AND CALLBACKS
 * ===============================================
 */
static size_t current_geofence_eventid=0;

static void 
cb_tbl_set_geofence_event_tag(hpdf_table_t t) {
    hpdf_table_set_tag(t,(void *)&current_geofence_eventid);
}

void
cb_GEOFENCE_EVENT_status_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    hpdf_table_widget_slide_button(doc, page, xpos+10, ypos+4, slide_width, slide_height, cb_GFENCE_EVENT_status(tag,r,c));
}

void
cb_GFENCE_EVENT_action_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    const char *stat=cb_GFENCE_EVENT_action(tag,r,c);
    const size_t dev_stat = (stat[0]-'0');
    const size_t vip_idx = (stat[2]-'0');
    if( dev_stat <= 3 && vip_idx <= 5 ) {
        cb_widget_draw_LS_VIP_state(doc, page, xpos+10, ypos, dev_stat, vip_idx);
    } else {
        logmsg(LOG_ERR,"Values for gfen event %zu out of range (%c, %c)",*((size_t *)tag), stat[0],stat[2]);
    }
}


static int
_tbl_gfence_event(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, size_t event_id, char *title) {

    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback,content-style-callback, cell-callback
        {0,0,1,1,"ID:",cb_GFENCE_EVENT_ID,NULL,NULL},
        {0,1,1,1,"Status:",NULL,NULL,cb_GEOFENCE_EVENT_status_draw_slide_button},
        {0,2,1,1,"Latitude:",cb_GFENCE_EVENT_lat,NULL,NULL},
        {0,3,1,1,"Longitude:",cb_GFENCE_EVENT_lon,NULL,NULL},
        {0,4,1,1,"Radius:",cb_GFENCE_EVENT_radius,NULL,NULL},
        {1,0,1,3,"Zone control:",cb_GFENCE_EVENT_zone,NULL,NULL},
        {1,3,1,2,"Action:",NULL,NULL,cb_GFENCE_EVENT_action_draw_slide_button},
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };
    
    // Overall table layout
    hpdf_table_spec_t tbl = {
        title, 
        2, 5,               /* rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_tbl_set_geofence_event_tag               /* Post processing callback */
    };

    current_geofence_eventid = event_id;
    return stroke_g7ctrl_report_table(tbl);
    /* free(cells);
    return ret; */
}

/* *==========================================================================
 * Report header (on top of each page)
 */

static _Bool
cb_header_style(void *tag, size_t r, size_t c, hpdf_text_style_t *style) {
    style->background = HPDF_COLOR_FROMRGB(248,248,248);
    style->color = HPDF_COLOR_FROMRGB(70,70,70);
    style->font = HPDF_FF_COURIER_BOLD;
    style->fsize = 11;
    return TRUE;
}

static int page_num=1;
static int total_pages=0;

static char *
cb_header_date_time(void *tag, size_t r, size_t c) {
    static char buf[64];
    time_t t = time(NULL);
    ctime_r(&t,buf);
    return buf;
}

static char *
cb_header_pgnum(void *tag, size_t r, size_t c) {
    static char buf[64];
    snprintf(buf,sizeof(buf),"%d(%d)",page_num,total_pages);
    return buf;
}


static int
report_page_header(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback,content-style-callback, cell-callback
        {0,0,1,3,"Title:",cb_header_title,cb_header_style,NULL},
        {0,3,1,3,"Generated:",cb_header_date_time,cb_header_style,NULL},
        {0,6,1,1,"Page:",cb_header_pgnum,cb_header_style,NULL},
        {0,0,0,0,NULL,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        NULL, 1, 7,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_tbl_set_tag               /* Post processing callback */
    };

    return  stroke_g7ctrl_report_table(tbl);
}




/* ====================================================================
 * 
 */
static int
stroke_g7ctrl_report_table(hpdf_table_spec_t table_spec) {
    hpdf_table_theme_t *theme = hpdf_table_get_default_theme();
    theme->title_style->background = HPDF_COLOR_FROMRGB(0x88,0x8a,0x63);
    theme->title_style->color = HPDF_COLOR_FROMRGB(0xf8,0xf8,0xf8);

    theme->label_style->color = HPDF_COLOR_FROMRGB(0,0,140);
    theme->content_style->font = HPDF_FF_COURIER_BOLD;
    int ret=hpdf_table_stroke_from_data(pdf_doc, pdf_page, table_spec, theme);
    if( -1 == ret ) {
        char *buf;
        int r,c;
        int tbl_err = hpdf_table_get_last_errcode(&buf,&r,&c);
        logmsg(LOG_ERR,"HPDF Error in creating table from data. ( %d : \"%s\" ) @ [%d,%d]\n",tbl_err,buf,r,c);
    }
    hpdf_table_destroy_theme(theme);
    return 0;
}

int
report_page_footer(HPDF_REAL xpos, HPDF_REAL ypos) {

    const HPDF_RGBColor text_color = HPDF_COLOR_FROMRGB(120,120,120);
    const HPDF_RGBColor divider_line_color = HPDF_COLOR_FROMRGB(160,160,160);

    HPDF_Page_SetRGBStroke(pdf_page,divider_line_color.r, divider_line_color.g, divider_line_color.b);
    HPDF_Page_SetLineWidth(pdf_page,0.7);
    HPDF_Page_MoveTo(pdf_page,xpos,ypos+11);
    HPDF_Page_LineTo(pdf_page,xpos+50,ypos+11);
    HPDF_Page_Stroke(pdf_page);

    HPDF_Page_SetRGBFill(pdf_page, text_color.r, text_color.g, text_color.b);
    HPDF_Page_SetTextRenderingMode(pdf_page, HPDF_FILL);
    HPDF_Page_SetFontAndSize(pdf_page, HPDF_GetFont(pdf_doc, HPDF_FF_TIMES_ITALIC, HPDF_TABLE_DEFAULT_TARGET_ENCODING), 10);

    HPDF_Page_BeginText(pdf_page);
    hpdf_table_encoding_text_out(pdf_page, xpos, ypos, cb_footer_text());
    HPDF_Page_EndText(pdf_page);

    return 0;
}

// The name of the logo image in the assets folder
#define TRACKER_IMAGE_LOGO "gm7_tracker_small.jpg"

int
stroke_g7ctrl_logo(HPDF_REAL xpos, HPDF_REAL ypos) {

    const HPDF_RGBColor text_color = HPDF_COLOR_FROMRGB(120,20,20);

    HPDF_Page_SetRGBFill(pdf_page, text_color.r, text_color.g, text_color.b);
    HPDF_Page_SetTextRenderingMode(pdf_page, HPDF_FILL);
    HPDF_Page_SetFontAndSize(pdf_page, HPDF_GetFont(pdf_doc, HPDF_FF_HELVETICA_BOLD, HPDF_TABLE_DEFAULT_TARGET_ENCODING), 20);

    HPDF_Page_BeginText(pdf_page);
    HPDF_Page_TextOut(pdf_page, xpos+39, ypos+3, cb_logo_text());
    HPDF_Page_EndText(pdf_page);

    char buf[256];
    snprintf(buf,sizeof(buf),"%s/assets/%s", data_dir, TRACKER_IMAGE_LOGO);

    // Check if logo exists
    struct stat filestat;
    if (-1 == stat(buf, &filestat)) {
        logmsg(LOG_WARNING, "HPDF: Cannot access Logo file '%s'. (%d : %s)", buf, errno, strerror(errno));
        return -1;
    }

    HPDF_Image logo = HPDF_LoadJpegImageFromFile (pdf_doc,buf);
    HPDF_Page_DrawImage(pdf_page,logo,xpos,ypos-2,37,35);

    return 0;
}

// Setup a PDF document with one page
static void
add_a4page(void) {
    pdf_page = HPDF_AddPage (pdf_doc);
    HPDF_Page_SetSize (pdf_page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
}

/**
 * Create a device report
 * @param include_geofevt TRUE to include all gefence event in report
 * @return 0 on success, -1 on failure
 */
int
layout_g7ctrl_report(_Bool include_geofevt) {

    const HPDF_REAL left_margin=70;     // Left side margin
    const HPDF_REAL right_margin=45;    // Right side margin
    const HPDF_REAL top_margin=60;      // Top side margin
    const HPDF_REAL hmargin=0;          // Horizontal margin between two table on the same table-row
    const HPDF_REAL vmargin=15;         // Vertical margin between tables
    const HPDF_REAL footer_ypos = 40;   // Absolute vert. position from bottom
    const HPDF_REAL logo_margin = 5;    // Margin between logo and title

    typedef int (*tbl_func_t)(HPDF_REAL,HPDF_REAL,HPDF_REAL);
    typedef enum {NRP_SAME, NRP_ROW, NRP_PAGE} new_row_page_t ;
    typedef struct tbl_spec {
        new_row_page_t new_row_page;
        HPDF_REAL width;
        tbl_func_t tbl_func;
    } tbl_spec_t;

    const HPDF_REAL page_width = HPDF_Page_GetWidth(pdf_page);
    const HPDF_REAL page_height = HPDF_Page_GetHeight(pdf_page);
    const HPDF_REAL report_full_width = round(page_width - left_margin - right_margin);
    //const HPDF_REAL report_half_width = report_full_width/2.0;

   
    tbl_spec_t table_spec[] = {
        // Page 1: Row 1 of tables
        {NRP_SAME,report_full_width, _tbl_device},

        // Row 2  of tables
        {NRP_ROW,report_full_width, _tbl_VIP},

        // Row 3 of tables
        {NRP_ROW, report_full_width, _tbl_GSM},

        // Row 4 of tables
        {NRP_ROW, report_full_width, _tbl_GPRS},

        // Row 5 of tables
        {NRP_ROW, report_full_width, _tbl_POWER},

        // Row 6 of tables
        {NRP_ROW, report_full_width, _tbl_llog},
        
        // Row 7 of tables
        {NRP_ROW,report_full_width, _tbl_ltrack},

        // Row 7 of tables
        {NRP_PAGE, report_full_width, _tbl_LOGGED},

        // Page 2: Row 1 of tables
        {NRP_ROW, report_full_width, _tbl_gfence},
                
        {0,0,NULL},
    };

    HPDF_REAL xpos = left_margin;
    HPDF_REAL ypos = top_margin;

        
    // The total number of pages will depend on the number of geo fence event we
    // will display and also if they start on a new page or continue directly
    // after the last table
    
    // How many events are there that are defined
    total_pages = 2;
    size_t num_defined_events=0;
    if( include_geofevt ) {
        
        for (size_t event_id = 50; event_id < 100; event_id++) {

            char *res_lat = cb_GFENCE_EVENT_lat((void *) &event_id, 0, 0);
            if ((pdfreport_hide_empty_geoevent && 0 == strcmp(res_lat, "0.000000")) || 0 == strcmp(res_lat, FLD_ERR_STR)) {
                // Ignore empty events or events that couldn't be read from the device
                continue;
            }

            num_defined_events++;

        }
    }
    
    if( pdfreport_geoevent_newpage ) {        
        total_pages += ceil((double)num_defined_events/7.0);
    } else {
        if( num_defined_events > 6 ) {
            total_pages += ceil((double)(num_defined_events-6)/7.0);
        } 
    }

    stroke_g7ctrl_logo(xpos,page_height-top_margin+logo_margin);
    report_page_header(xpos, ypos, report_full_width);
    report_page_footer(xpos,footer_ypos);

    HPDF_REAL aheight;
    (void)hpdf_table_get_last_auto_height(&aheight);
    ypos += (aheight + vmargin*2);

    size_t idx=0;
    tbl_spec_t *tbl=&table_spec[idx];
    
    while( tbl->tbl_func ) {
                
        if( NRP_ROW == tbl->new_row_page ) {
            
            // New row of tables
            xpos = left_margin;
            (void)hpdf_table_get_last_auto_height(&aheight);
            ypos += aheight + vmargin;
            
        } else if( NRP_PAGE == tbl->new_row_page ) {
            
            // New page
            page_num++;
            add_a4page();
            xpos = left_margin;
            ypos = top_margin;
            stroke_g7ctrl_logo(xpos, page_height-top_margin+logo_margin);
            report_page_header(xpos, ypos, report_full_width);
            report_page_footer(xpos,footer_ypos);
            (void)hpdf_table_get_last_auto_height(&aheight);
            ypos += (aheight + vmargin*2);
            
        }

        tbl->tbl_func(xpos,ypos,tbl->width);
        xpos += (tbl->width + hmargin);
        idx++;
        tbl = &table_spec[idx];
        
    }
    
    
    if( include_geofevt ) {

        /*
          * Draw the Geo-Fence Event tables. There are two major options that
          * controls the layout of the event tables.
          * "start_geofence_event_new_page" is set to true to start the tables
          * at a new page
          * "ignore_unset_events" will not print tables with undefined events. 
          * AN underfined event is a Geo-Fence Event with no lat,lon set
          */
                    
        if( pdfreport_geoevent_newpage ) {
            // New page
            page_num++;
            add_a4page();
            xpos = left_margin;
            ypos = top_margin;
            stroke_g7ctrl_logo(xpos, page_height-top_margin+logo_margin);
            report_page_header(xpos, ypos, report_full_width);
            report_page_footer(xpos,footer_ypos);
            (void)hpdf_table_get_last_auto_height(&aheight);
            ypos += (aheight + vmargin*2);        
        } else {
            xpos = left_margin;
            (void)hpdf_table_get_last_auto_height(&aheight);
            ypos += aheight + vmargin;                
        }

        char title_buf[64];
        // Add all the gfence event tables
        const size_t start_event=50;
        const size_t last_event=99;

        for( size_t event_id=start_event; event_id <= last_event; event_id++ ) {

            // Use the latitude value as a way to tell of 1) This event has been specified and
            // 2) It exists , i.e. it has been successfully read from the device
            char *res_lat = cb_GFENCE_EVENT_lat((void *)&event_id,0,0);
            if( (pdfreport_hide_empty_geoevent && 0==strcmp(res_lat,"0.000000")) || 0==strcmp(res_lat,FLD_ERR_STR) ) {
                // Ignore empty events or events that couldn't be read from the device
                continue;
            }

            // New row of tables
            if( ypos > 700 ) {
                // Need a new page
                page_num++;
                add_a4page();
                xpos = left_margin;
                ypos = top_margin;
                stroke_g7ctrl_logo(xpos, page_height-top_margin+logo_margin);
                report_page_header(xpos, ypos, report_full_width);
                report_page_footer(xpos,footer_ypos);
                (void)hpdf_table_get_last_auto_height(&aheight);
                ypos += (aheight + vmargin*2);                
            }

            snprintf(title_buf,sizeof(title_buf),"Geo Fence Event : %zu",event_id);
            _tbl_gfence_event(xpos,ypos,report_full_width,event_id,title_buf);

            xpos = left_margin;
            (void)hpdf_table_get_last_auto_height(&aheight);
            ypos += aheight + vmargin;        

        }
        
    } // if( include_geofevt )
    
    return 0;
}

// A standard hpdf error handler which also translates the hpdf error code to a human
// readable string
static void
error_handler (HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data) {
    logmsg(LOG_ERR,"PDF ERROR: \"%s\", [0x%04X : %d]\n", hpdf_errstr(error_no), (unsigned int) error_no, (int) detail_no);
    longjmp (_g7report_env, 1);
}


/**
 * @brief Controller in the model-view-controller that structures the PDF report
 *
 * Create PDF report and export to named file.
 *
 * @param cli_info Client information to get hold of device reference
 * @param filename PDF report filename
 * @param report_title The title set as a PDF attribute to the document
 * @param read_from_file TRUE if device data should be read from a previous stored JSON file insteaf of from the device. Mainly use for special
 * documentation purposes an debugging. This is normally triggered by the command ".freport"
 * @param include_geofevt TRUE to onclude the geofence events in the report
 * @return 0 on success, -1 on failure
 */
int
export_g7ctrl_report(struct client_info *cli_info, char *filename, size_t maxfilename, 
                     char *report_title, _Bool read_from_file, _Bool include_geofevt) {
    if (setjmp(_g7report_env)) {
        HPDF_Free (pdf_doc);
        return -1;
    }
    
    // Initialize the page number shown in the header
    page_num = 1;
    
    logmsg(LOG_DEBUG,"Initializing the model from device");
    if( -1 == init_model_from_device( (void *)cli_info, read_from_file, include_geofevt) ) {
        logmsg(LOG_ERR,"Failed to extract information from the device. Cannot generate report");
        return -1;
    }
    
    char buf[2048];
    
    // Setup the PDF document
    pdf_doc = HPDF_New(error_handler, NULL);
    HPDF_SetCompressionMode(pdf_doc, HPDF_COMP_ALL);
    
    snprintf(buf,sizeof(buf),"%s",PACKAGE_STRING);
    HPDF_SetInfoAttr (pdf_doc,HPDF_INFO_CREATOR, buf);
    
    // Remember the client info structure for the table callbacks
    cli_info_for_callback = cli_info;

    snprintf(buf,sizeof(buf),"GM7 Device Report : %u",cli_info->target_deviceid);
    HPDF_SetInfoAttr (pdf_doc,HPDF_INFO_TITLE,buf);

   
    
    if( report_title ) {
        strncpy(report_header_title,report_title,sizeof(report_header_title)-1);
    } else {
        *report_header_title='\0';
    }
    
    hpdf_table_set_origin_top_left(TRUE);
    add_a4page();
    
    logmsg(LOG_DEBUG,"Starting report layout");
    layout_g7ctrl_report(include_geofevt);

    snprintf(buf,sizeof(buf),"%s/%s_%s",pdfreport_dir,filename,get_model_deviceid());    
    
    export_model_to_json(buf); 
    
    snprintf(filename,maxfilename,"%s.pdf",buf);
    logmsg(LOG_DEBUG,"Exporting report as PDF to file: \"%s\"",filename);        
    HPDF_SaveToFile (pdf_doc, filename);
    HPDF_Free (pdf_doc);
    
    cleanup_and_free_model();

    return 0;
}

#pragma GCC diagnostic pop

/* EOF */
