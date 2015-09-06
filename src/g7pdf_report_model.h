/* =========================================================================
 * File:        G7CTRL_PDF_MODEL.H
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


#ifndef G7CTRL_PDF_VIEW_H
#define	G7CTRL_PDF_VIEW_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char report_header_title[128];

int
init_model_from_device(void *tag);

void
cleanup_and_free_model(void);

int
export_model_to_json(char *fname);


char *
cb_header_title(void *tag, size_t r, size_t c);

/* ===============================================
 * DEVICE TABLE AND CALLBACKS
 */

char *
cb_DEVICE_ID(void *tag, size_t r, size_t c);
char *
cb_DEVICE_nick(void *tag, size_t r, size_t c);
char *
cb_DEVICE_PIN(void *tag, size_t r, size_t c);
char *
cb_DEVICE_SW_VER(void *tag, size_t r, size_t c);
char *
cb_DEVICE_TZ(void *tag, size_t r, size_t c);
_Bool
cb_DEVICE_LED(void *tag, size_t r, size_t c);
char *
cb_DEVICE_RA(void *tag, size_t r, size_t c);
size_t
cb_DEVICE_GSENS(void *tag, size_t r, size_t c);
char *
cb_DEVICE_TEST(void *tag, size_t r, size_t c);

/***/


char *
cb_SIM_ID(void *tag, size_t r, size_t c);
char *
cb_SIM_PIN(void *tag, size_t r, size_t c);
_Bool
cb_SIM_ROAMING(void *tag, size_t r, size_t c);

/***/

char *
cb_POWER_MODE(void *tag, size_t r, size_t c);
char *
cb_POWER_INTERVAL(void *tag, size_t r, size_t c);
char *
cb_POWER_VIP(void *tag, size_t r, size_t c);
char *
cb_POWER_TIMER(void *tag, size_t r, size_t c);
char *
cb_POWER_WAKEUP(void *tag, size_t r, size_t c);
char *
cb_POWER_SLEEP(void *tag, size_t r, size_t c);

/***/


char *
cb_BATTERY_VOLTAGE(void *tag, size_t r, size_t c);
float
cb_BATTERY_PERCENT(void *tag, size_t r, size_t c);
char *
cb_BATTERY_LOW(void *tag, size_t r, size_t c);
//char *
//cb_BATTERY_VIP(void *tag, size_t r, size_t c);

/***/
char *
cb_VIP_no(void *tag, size_t r, size_t c); 

/***/


char *
cb_GSM_MODE(void *tag, size_t r, size_t c);
char *
cb_GSM_SMS(void *tag, size_t r, size_t c);
char *
cb_GSM_SMS_NBR(void *tag, size_t r, size_t c);
char *
cb_GSM_CSD_NBR(void *tag, size_t r, size_t c);
char *
cb_GSM_LOCATION(void *tag, size_t r, size_t c);
char *
cb_GSM_IMEI(void *tag, size_t r, size_t c);
/***/


char *
cb_GPRS_APN(void *tag, size_t r, size_t c);
char *
cb_GPRS_server(void *tag, size_t r, size_t c);
char *
cb_GPRS_port(void *tag, size_t r, size_t c);
char *
cb_GPRS_user(void *tag, size_t r, size_t c);
char *
cb_GPRS_pwd(void *tag, size_t r, size_t c);
char *
cb_GPRS_DNS(void *tag, size_t r, size_t c);
char *
cb_GPRS_keep_alive(void *tag, size_t r, size_t c);


/***/


char *
cb_LLOG_mode(void *tag, size_t r, size_t c);
char *
cb_LLOG_timer(void *tag, size_t r, size_t c);
char *
cb_LLOG_dist(void *tag, size_t r, size_t c);
char *
cb_LLOG_number(void *tag, size_t r, size_t c);
char *
cb_LLOG_heading(void *tag, size_t r, size_t c);
_Bool
cb_LLOG_waitGPS(void *tag, size_t r, size_t c);

/***/


char *
cb_LTRACK_mode(void *tag, size_t r, size_t c);
char *
cb_LTRACK_commselect(void *tag, size_t r, size_t c);
char *
cb_LTRACK_timer(void *tag, size_t r, size_t c);
char *
cb_LTRACK_dist(void *tag, size_t r, size_t c);
char *
cb_LTRACK_number(void *tag, size_t r, size_t c);
char *
cb_LTRACK_heading(void *tag, size_t r, size_t c);
_Bool
cb_LTRACK_waitGPS(void *tag, size_t r, size_t c);

/***/


_Bool
cb_GFENCE_status(void *tag, size_t r, size_t c);
char *
cb_GFENCE_lat(void *tag, size_t r, size_t c);
char *
cb_GFENCE_lon(void *tag, size_t r, size_t c);
char *
cb_GFENCE_radius(void *tag, size_t r, size_t c);
char *
cb_GFENCE_zone(void *tag, size_t r, size_t c);
char *
cb_GFENCE_action(void *tag, size_t r, size_t c);

/***/

char *
cb_LOGGED_number(void *tag, size_t r, size_t c);

char *
cb_LOGGED_dates(void *tag, size_t r, size_t c);

/***/

char *
cb_GFENCE_EVENT_ID(void *tag, size_t r, size_t c);
_Bool
cb_GFENCE_EVENT_status(void *tag, size_t r, size_t c);
char *
cb_GFENCE_EVENT_lat(void *tag, size_t r, size_t c);
char *
cb_GFENCE_EVENT_lon(void *tag, size_t r, size_t c);
char *
cb_GFENCE_EVENT_radius(void *tag, size_t r, size_t c);
char *
cb_GFENCE_EVENT_zone(void *tag, size_t r, size_t c);
char *
cb_GFENCE_EVENT_action(void *tag, size_t r, size_t c);


/***/

char  *
cb_footer_text(void);

char *
cb_logo_text(void);


#ifdef	__cplusplus
}
#endif

#endif	/* G7CTRL_PDF_VIEW_H */

