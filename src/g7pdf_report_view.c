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

#ifdef __APPLE__
#include <sys/utsname.h>
#else
#include <linux/utsname.h>
#endif

// These two includes should always be used
#include "libhpdftbl/hpdf_table.h"
#include "libhpdftbl/hpdf_errstr.h"
#include "libhpdftbl/hpdf_table_widget.h"
#include "g7pdf_report_model.h"
#include "g7pdf_report_view.h"
#include "g7ctrl.h"
#include "config.h"
#include "logger.h"
#include "g7config.h"

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

/* ===============================================
 * DEVICE TABLE AND CALLBACKS
 */


void
cb_DEVICE_LED_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    hpdf_table_widget_slide_button(doc, page, xpos, ypos, width, height, cb_DEVICE_LED(tag,r,c));
}

void
cb_DEVICE_RA_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    hpdf_table_widget_slide_button(doc, page, xpos, ypos, width, height, cb_DEVICE_RA(tag,r,c));
}


static _Bool
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

    const HPDF_REAL meter_width = width/3.5;
    const HPDF_REAL meter_height = height/1.8;
    const HPDF_REAL meter_xpos = xpos+width/3;
    const HPDF_REAL meter_ypos = ypos+4;

    hpdf_table_widget_strength_meter(doc, page,meter_xpos, meter_ypos, meter_width, meter_height,
                                     num_segments, green, num_on_segments);
}

void
cb_DEVICE_post_processing(hpdf_table_t t) {
    hpdf_table_set_cell_canvas_callback(t,1,1,cb_DEVICE_LED_draw_slide_button);
    hpdf_table_set_cell_canvas_callback(t,1,2,cb_DEVICE_RA_draw_slide_button);
    hpdf_table_set_cell_canvas_callback(t,1,3,cb_DEVICE_draw_gsens);
}

static int
_tbl_device(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,1,"ID:",cb_DEVICE_ID,cb_DEVICE_ID_style},
        {0,1,1,1,"Nick-name:",cb_DEVICE_nick,cb_DEVICE_ID_style},
        {0,2,1,1,"PIN:",cb_DEVICE_PIN,NULL},
        {0,3,1,1,"SW Ver:",cb_DEVICE_SW_VER,NULL},
        {1,0,1,1,"TZ:",cb_DEVICE_TZ,NULL},
        {1,1,1,1,"LED:",NULL,NULL},
        {1,2,1,1,"Removal Alert:",NULL,NULL},
        {1,3,1,1,"G-Sensitivity:",NULL,NULL},

        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Device", 2, 4,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,                            /* A pointer to the specification of each row in the table */
        cb_DEVICE_post_processing         /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);

}

/* ===============================================
 * SIM TABLE AND CALLBACKS
 * ===============================================
 */

static int
_tbl_SIM(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,2,"ID:",cb_SIM_ID,NULL},
        {0,2,1,1,"PIN:",cb_SIM_PIN,NULL},
        {1,0,1,3,"Roaming:",cb_SIM_ROAMING,NULL},
        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "SIM", 2, 3,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        NULL               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);

}

/* ===============================================
 * POWER SAVING TABLE AND CALLBACKS
 * ===============================================
 */

static int
_tbl_power_handling(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,1,"Mode:",cb_PH_MODE,NULL},
        {0,1,1,1,"Interval:",cb_PH_INTERVAL,NULL},
        {0,2,1,1,"VIP:",cb_PH_VIP,NULL},
        {0,3,1,1,"Wake report:",cb_PH_REPORT_WAKE,NULL},
        {1,0,1,1,"Timer 1:",cb_PH_TIMER,NULL},
        {1,1,1,1,"Timer 2:",cb_PH_TIMER,NULL},
        {1,2,1,1,"Timer 3:",cb_PH_TIMER,NULL},
        {1,3,1,1,"Sleep report:",cb_PH_REPORT_SLEEP,NULL},
        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Power handling", 2, 4,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        NULL               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}


/* ===============================================
 * POWER REPORT TABLE AND CALLBACKS
 * ===============================================
 */

void
cb_BATTERY_draw_segment(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {

    const HPDF_RGBColor red = HPDF_COLOR_FROMRGB(255,69,0);
    const HPDF_RGBColor yellow = HPDF_COLOR_FROMRGB(255,215,0);
    const HPDF_RGBColor green = HPDF_COLOR_FROMRGB(60,179,113);

    const size_t num_segments=10;
    const float capacity = round(cb_BATTERY_PERCENT(tag, r, c)*10.0)/10.0;
    const size_t num_on_segments = lround(capacity*num_segments);

    HPDF_RGBColor color;
    if( capacity >= 0.55 ) {
        color=green;
    } else if( capacity > 0.25 ) {
        color=yellow;
    } else {
        color=red;
    }

    const HPDF_REAL segment_tot_width = width/3;
    const HPDF_REAL segment_height = height/3;
    const HPDF_REAL segment_xpos = xpos+width/2;
    const HPDF_REAL segment_ypos = ypos+height/2;

    const _Bool use_segment_meter = TRUE;


    if( use_segment_meter ) {
        hpdf_table_widget_segment_hbar(doc, page,segment_xpos, segment_ypos, segment_tot_width, segment_height,
                                       num_segments, color, num_on_segments);
    } else {
        hpdf_table_widget_hbar(doc, page,segment_xpos, segment_ypos, segment_tot_width, segment_height,
                               color, capacity,FALSE);
    }

}

void
cb_BATTERY_post_processing(hpdf_table_t t) {
    //hpdf_table_set_cell_canvas_callback(t,0,0,cb_BATTERY_draw_meter);
    hpdf_table_set_cell_canvas_callback(t,0,0,cb_BATTERY_draw_segment);
}


static int
_tbl_battery(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,2,"Voltage:",cb_BATTERY_VOLTAGE,NULL},
        {1,0,1,1,"Low warn:",cb_BATTERY_LOW,NULL},
        {1,1,1,1,"VIP:",cb_BATTERY_VIP,NULL},
        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Battery", 2, 2,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_BATTERY_post_processing           /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}


/* ===============================================
 * GSM TABLE AND CALLBACKS
 * ===============================================
 */

void
cb_GSM_LOCATION_draw_slide_button(HPDF_Doc doc, HPDF_Page page, void *tag, size_t r, size_t c,
                     HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width, HPDF_REAL height) {
    hpdf_table_widget_slide_button(doc, page, xpos, ypos, width, height, cb_GSM_LOCATION(tag,r,c));
}

void
cb_GSM_post_processing(hpdf_table_t t) {
    hpdf_table_set_cell_canvas_callback(t,2,1,cb_GSM_LOCATION_draw_slide_button);
}

static int
_tbl_GSM(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,2,"Mode:",cb_GSM_MODE,NULL},
        {1,0,1,1,"SMS No:",cb_GSM_SMS_NBR,NULL},
        {1,1,1,1,"CSD No:",cb_GSM_CSD_NBR,NULL},
        {2,0,1,1,"SMS mode:",cb_GSM_SMS,NULL},
        {2,1,1,1,"Location on call:",NULL,NULL},
        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "GSM", 3, 2,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_GSM_post_processing               /* Post processing callback */
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

static int
_tbl_GPRS(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,3,"APN:",cb_GPRS_APN,NULL},
        {0,3,1,3,"Server:",cb_GPRS_server_port,NULL},
        {0,6,1,2,"DNS:",cb_GPRS_DNS,NULL},
        /* {0,2,1,1,"Port:",cb_GPRS_port,NULL},*/
        {1,0,1,3,"User:",cb_GPRS_user,NULL},
        {1,3,1,3,"PWD:",cb_GPRS_pwd,NULL},
        {1,6,1,2,"Keep alive interval:",cb_GPRS_keep_alive,NULL},
        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "GPRS", 2, 8,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        NULL               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}

/* ===============================================
 * VIP TABLE AND CALLBACKS
 * ===============================================
 */

static char *
cb_VIP_no(void *tag, size_t r, size_t c) {
    static char buf[64];
    snprintf(buf,sizeof(buf),"VIP no %zu",c+1);
    return buf;
}

static int
_tbl_VIP(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,1,"No 1:",cb_VIP_no,NULL},
        {0,1,1,1,"No 2:",cb_VIP_no,NULL},
        {1,0,1,1,"No 3:",cb_VIP_no,NULL},
        {1,1,1,1,"No 4:",cb_VIP_no,NULL},
        {2,0,1,2,"No 5:",cb_VIP_no,NULL},
        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "VIP Numbers", 3, 2,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        NULL               /* Post processing callback */
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
    hpdf_table_widget_slide_button(doc, page, xpos, ypos, width, height, cb_LLOG_waitGPS(tag,r,c));
}

void
cb_LLOG_post_processing(hpdf_table_t t) {
    hpdf_table_set_cell_canvas_callback(t,1,2,cb_LLOG_waitGPS_draw_slide_button);
}

static int
_tbl_llog(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,1,"Mode:",cb_LLOG_mode,NULL},
        {0,1,1,1,"Timer:",cb_LLOG_timer,NULL},
        {0,2,1,1,"Dist:",cb_LLOG_dist,NULL},
        {1,0,1,1,"Limit:",cb_LLOG_number,NULL},
        {1,1,1,1,"Heading:",cb_LLOG_heading,NULL},
        {1,2,1,1,"waitGPS:",NULL,NULL},
        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Locations logging", 2, 3,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_LLOG_post_processing               /* Post processing callback */
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
    hpdf_table_widget_slide_button(doc, page, xpos, ypos, width, height, cb_LTRACK_waitGPS(tag,r,c));
}

void
cb_LTRACK_post_processing(hpdf_table_t t) {
    hpdf_table_set_cell_canvas_callback(t,1,2,cb_LTRACK_waitGPS_draw_slide_button);
}

static int
_tbl_ltrack(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,1,"Mode:",cb_LTRACK_mode,NULL},
        {0,1,1,1,"Timer:",cb_LTRACK_timer,NULL},
        {0,2,1,1,"Dist:",cb_LTRACK_dist,NULL},
        {1,0,1,1,"Limit:",cb_LTRACK_number,NULL},
        {1,1,1,1,"Heading:",cb_LTRACK_heading,NULL},
        {1,2,1,1,"waitGPS:",NULL,NULL},
        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Locations tracking", 2, 3,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        cb_LTRACK_post_processing               /* Post processing callback */
    };

    return stroke_g7ctrl_report_table(tbl);
}


/* ===============================================
 * GEO FENCE TABLE AND CALLBACKS
 * ===============================================
 */

static int
_tbl_gfence(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {
    // Specified the layout of each row
    hpdf_table_data_spec_t cells[] = {
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,1,"Status:",cb_GFENCE_status,NULL},
        {0,1,1,1,"Lat:",cb_GFENCE_lat,NULL},
        {0,2,1,1,"Lon:",cb_GFENCE_lon,NULL},
        {0,3,1,1,"Radius:",cb_GFENCE_radius,NULL},
        {0,4,1,1,"Type:",cb_GFENCE_type,NULL},
        {0,5,1,1,"Action:",cb_GFENCE_action,NULL},
        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Geo fence", 1, 6,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        NULL               /* Post processing callback */
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

static int
_tbl_gfence_event(HPDF_REAL xpos, HPDF_REAL ypos, HPDF_REAL width) {

    const size_t num_events = 15;

    hpdf_table_data_spec_t *cells = calloc(num_events*7+1,sizeof(hpdf_table_data_spec_t));

    for(size_t idx=0; idx < num_events; idx++) {
        cells[0+idx*7] = (hpdf_table_data_spec_t){idx,0,1,1,"ID:",cb_GFENCE_EVENT_ID,NULL};
        cells[1+idx*7] = (hpdf_table_data_spec_t){idx,1,1,1,"Status:",cb_GFENCE_EVENT_status,NULL};
        cells[2+idx*7] = (hpdf_table_data_spec_t){idx,2,1,1,"Lat:",cb_GFENCE_EVENT_lat,NULL};
        cells[3+idx*7] = (hpdf_table_data_spec_t){idx,3,1,1,"Lon:",cb_GFENCE_EVENT_lon,NULL};
        cells[4+idx*7] = (hpdf_table_data_spec_t){idx,4,1,1,"Radius:",cb_GFENCE_EVENT_radius,NULL};
        cells[5+idx*7] = (hpdf_table_data_spec_t){idx,5,1,1,"Type:",cb_GFENCE_EVENT_type,NULL};
        cells[6+idx*7] = (hpdf_table_data_spec_t){idx,6,1,1,"Action:",cb_GFENCE_EVENT_action,NULL};
    }
    cells[num_events*7] = (hpdf_table_data_spec_t){0,0,0,0,NULL,NULL,NULL};

    // Overall table layout
    hpdf_table_spec_t tbl = {
        "Geo fence events", num_events, 7,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        NULL               /* Post processing callback */
    };

    int ret =  stroke_g7ctrl_report_table(tbl);
    free(cells);
    return ret;
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
static int total_pages=2;

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
        // row,col,rowspan,colspan,lable-string,content-callback
        {0,0,1,3,"Title:",cb_header_title,cb_header_style},
        {0,3,1,3,"Generated:",cb_header_date_time,cb_header_style},
        {0,6,1,1,"Page:",cb_header_pgnum,cb_header_style},
        {0,0,0,0,NULL,NULL,NULL}  /* Sentinel to mark end of data */
    };

    // Overall table layout
    hpdf_table_spec_t tbl = {
        NULL, 1, 7,      /* Title, rows, cols   */
        xpos, ypos,         /* xpos, ypos          */
        width, 0,          /* width, height       */
        cells,             /* A pointer to the specification of each row in the table */
        NULL               /* Post processing callback */
    };

    return  stroke_g7ctrl_report_table(tbl);
}

static int
stroke_g7ctrl_report_table(hpdf_table_spec_t table_spec) {
    hpdf_table_theme_t *theme = hpdf_table_get_default_theme();
    theme->title_style->background = HPDF_COLOR_FROMRGB(0x88,0x8a,0x63);
    theme->title_style->color = HPDF_COLOR_FROMRGB(0xf0,0xf0,0xf0);

    theme->label_style->color = HPDF_COLOR_FROMRGB(0,0,140);
    int ret=hpdf_table_stroke_from_data(pdf_doc, pdf_page, table_spec, theme);
    if( -1 == ret ) {
        char *buf;
        int r,c;
        int tbl_err = hpdf_table_get_last_errcode(&buf,&r,&c);
        fprintf(stderr,"*** ERROR in creating table from data. ( %d : \"%s\" ) @ [%d,%d]\n",tbl_err,buf,r,c);
    }
    hpdf_table_destroy_theme(theme);
    return 0;
}

int
report_page_footer(HPDF_REAL xpos, HPDF_REAL ypos) {

    const HPDF_RGBColor text_color = HPDF_COLOR_FROMRGB(160,160,160);
    const HPDF_RGBColor divider_line_color = HPDF_COLOR_FROMRGB(160,160,160);

    HPDF_Page_SetRGBStroke(pdf_page,divider_line_color.r, divider_line_color.g, divider_line_color.b);
    HPDF_Page_SetLineWidth(pdf_page,0.7);
    HPDF_Page_MoveTo(pdf_page,xpos,ypos+11);
    HPDF_Page_LineTo(pdf_page,xpos+50,ypos+11);
    HPDF_Page_Stroke(pdf_page);

    HPDF_Page_SetRGBFill(pdf_page, text_color.r, text_color.g, text_color.b);
    HPDF_Page_SetTextRenderingMode(pdf_page, HPDF_FILL);
    HPDF_Page_SetFontAndSize(pdf_page, HPDF_GetFont(pdf_doc, HPDF_FF_HELVETICA_ITALIC, HPDF_TABLE_DEFAULT_TARGET_ENCODING), 9);

    HPDF_Page_BeginText(pdf_page);
    hpdf_table_encoding_text_out(pdf_page, xpos, ypos, cb_footer_text());
    HPDF_Page_EndText(pdf_page);

    return 0;
}

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
 * @return
 */
int
layout_g7ctrl_report(void) {

    const HPDF_REAL left_margin=85;
    const HPDF_REAL right_margin=65;
    const HPDF_REAL top_margin=75;
    const HPDF_REAL hmargin=0;
    const HPDF_REAL vmargin=15;

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
    const HPDF_REAL report_half_width = report_full_width/2.0;


    tbl_spec_t table_spec[] = {
        // Page 1: Row 1 of tables
        {NRP_SAME,report_full_width, _tbl_device},

        // Row 2 of tables
        {NRP_ROW,report_half_width, _tbl_SIM},
        {NRP_SAME,report_half_width, _tbl_battery},

        // Row 3 of tables
        {NRP_ROW, report_half_width, _tbl_GSM},
        {NRP_SAME,report_half_width, _tbl_VIP},

        // Row 4 of tables
        {NRP_ROW, report_full_width, _tbl_GPRS},

        // Row 5 of tables
        {NRP_ROW, report_full_width, _tbl_power_handling},

        // Row 6 of tables
        {NRP_ROW, report_half_width, _tbl_llog},
        {NRP_SAME,report_half_width, _tbl_ltrack},

        // Row 7 of tables
        {NRP_ROW, report_full_width, _tbl_gfence},

        // Page 2: Row 1 of tables
        {NRP_PAGE, report_full_width, _tbl_gfence_event},

        {0,0,NULL},
    };

    HPDF_REAL xpos = left_margin;
    HPDF_REAL ypos = top_margin;
    const HPDF_REAL footer_ypos = 35;
    const HPDF_REAL logo_margin = 5;

    stroke_g7ctrl_logo(xpos,page_height-top_margin+logo_margin);
    report_page_header(xpos, ypos, report_full_width);
    report_page_footer(xpos,footer_ypos);

    HPDF_REAL aheight;
    (void)hpdf_table_get_last_auto_height(&aheight);
    ypos += (aheight + vmargin*2);
    page_num++;

    size_t idx=0;
    tbl_spec_t *tbl = &table_spec[idx];
    while( tbl->tbl_func ) {

        if( NRP_ROW == tbl->new_row_page ) {
            // New row of tables
            xpos = left_margin;
            (void)hpdf_table_get_last_auto_height(&aheight);
            ypos += aheight + vmargin;
        } else if( NRP_PAGE == tbl->new_row_page ) {
            // New page
            add_a4page();
            xpos = left_margin;
            ypos = top_margin;
            stroke_g7ctrl_logo(xpos, page_height-top_margin+logo_margin);
            report_page_header(xpos, ypos, report_full_width);
            report_page_footer(xpos,footer_ypos);
            (void)hpdf_table_get_last_auto_height(&aheight);
            ypos += (aheight + vmargin*2);
            page_num++;
        }

        tbl->tbl_func(xpos,ypos,tbl->width);
        xpos += (tbl->width + hmargin);
        tbl = &table_spec[++idx];
    }

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
 * 
 * 
 * Create PDF report and export to named file
 * @param filename PDF report filename
 * @return 0 on success, -1 on failure
 */
int
export_g7ctrl_report(struct client_info *cli_info, char *filename) {
    if (setjmp(_g7report_env)) {
        HPDF_Free (pdf_doc);
        return -1;
    }

    // Setup the PDF document
    pdf_doc = HPDF_New(error_handler, NULL);    
    HPDF_SetCompressionMode(pdf_doc, HPDF_COMP_ALL);
    
    logmsg(LOG_DEBUG,"HPDF: Intenal doc created");
    
    char buf[256];
    snprintf(buf,sizeof(buf),"%s",PACKAGE_STRING);
    HPDF_SetInfoAttr (pdf_doc,HPDF_INFO_CREATOR, buf);    
    HPDF_SetInfoAttr (pdf_doc,HPDF_INFO_TITLE,"GM7 Device Report");

    logmsg(LOG_DEBUG,"HPDF: Attributes created");
    
    hpdf_table_set_origin_top_left(TRUE);
    add_a4page();

    logmsg(LOG_DEBUG,"HPDF: Starting report layout");
    layout_g7ctrl_report();

    logmsg(LOG_DEBUG,"HPDF: Trying to save PDF document to file: \"%s\"",filename);
    HPDF_SaveToFile (pdf_doc, filename);
    HPDF_Free (pdf_doc);

    return 0;
}

#pragma GCC diagnostic pop

/* EOF */