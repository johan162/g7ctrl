/* =========================================================================
 * File:        G7CONFIG.C
 * Description: Read and handle config settings from the ini file
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: g7config.c 1061 2015-09-08 05:16:38Z ljp $
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

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <stdint.h>
#include <locale.h>
#include <signal.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>


#include "config.h"
#include "g7config.h"
#include "utils.h"
#include "g7ctrl.h"
#include "logger.h"
#include "xstr.h"
#include "build.h"

/* The iniparser library used is dependent on the configuration. It can either be the
 * system wide existing library or the daemon can use an internal version. Hence we need
 * to adjust which ini file we will be reading. The controlling variable is defined in the
 * generated config.h file.
 */
#ifdef HAVE_LIBINIPARSER
#include <iniparser.h>
#else
#include "libiniparser/iniparser.h"
#include "xstr.h"
#endif


/*
 * The value of the following variables are read from the ini-file.
 * They hold various run time limits and settings that
 * the user can adjust. Some of these values can also be overridden by being
 * given as options when the daemon starts
 */

// Should we run as a daemon or nothing
int daemonize = -1;

// Logfile details
int verbose_log;
char logfile_name[MAX_LOGFILE_NAME_LEN] = {'\0'};

// Names of the ini file and the db file used
char daemon_config_file[INIFILE_LEN];

// Names of the PID lockfile file given as on the command line
char pidfile[PIDFILE_LEN];
int pidfile_fd=-1;

#ifdef __APPLE__
int cu_usbmodem_list[MAX_OSX_USBMODEM];
#else
// Start index of STTY device for USB port on Linux
int stty_device_startidx;
#endif

// TCP/IP Port to listen to and expect the device to connect
unsigned int tcpip_device_port=0;

// TCP/IP Port to listen to and expect commands
unsigned int tcpip_cmd_port=0;

// Is password required when connecting on the command channel
_Bool require_client_pwd;
char client_pwd[MAX_PASSWORD_LEN];

// Maximum number of clients allowed to connect to us
size_t max_clients = 10;

// Maximum idel time we allow after a device has connected but haven't sent
// any data. Specified in seconds.
unsigned max_device_idle_time = DEFAULT_DEVICE_IDLE_TIME;

// Max idle time in seconds before automatic logout for command (Default 30min))
unsigned max_idle_time = DEFAULT_CLIENT_IDLE_TIME;

// What user the daemon should be running as
char run_as_user[32];

// Is it allowed to send raw commands directly to the device ("$WP+ ...)
int enable_raw_device_commands;

// Automatic GFEN handling
_Bool enable_gfen_tracking;
int gfen_tracking_interval;

// Google API key for lookup web services
char google_api_key[64];

/*
 * Mail setting. Determine if we should send mail on errors and other events and what address
 * to use. Read from the configuration file normally.
 */
// Should mails be enabled at all
int enable_mail;

// Enable event sending when a position event is received
int send_mail_on_event;

// From address used in sent mail
char daemon_email_from[64] = {'\0'};

// What address(es) should we send mail to
char send_mailaddress[64] = {'\0'};

// Prefix in the subject header
char mail_subject_prefix[128] = {'\0'};

// Should the mails be sent in HTML format
int use_html_mail;

// Use SMTP server to send mail
int smtp_use;

// IP address for SMTP server to use
char smtp_server[64];

// User for the SMTP server
char smtp_user[64];

// Password for SMTP user
char smtp_pwd[64];

// What port the SMTP server is using
int smtp_port;

// Force mail for all events (except REC)
int force_mail_on_all_events;

// Translate coordinates to approx. address
_Bool use_address_lookup ;

// Proximity for cache hit
int address_lookup_proximity;

// The directory for the static data files
char data_dir[MAX_DATA_DIR_LEN];

// The directory for files that change during execution the database and log files
char db_dir[MAX_DB_DIR_LEN];

// Compression method for attachments when mailing GPX exports
char attachment_compression[8];

// Holds the read dictionary from the inifile
dictionary *dict=NULL;

// Minimum time between locations to consider them to be i different tracks
// or segments
int track_split_time;
int trackseg_split_time;

/// Should the reply from device be translated to plaintext
_Bool translateDeviceReply;

// Notification on new tracker connection
_Bool mail_on_tracker_conn;

// Script on new tracker connection
_Bool script_on_tracker_conn;

_Bool include_minimap ;

int minimap_overview_zoom;
int minimap_detailed_zoom;
int minimap_width;
int minimap_height;

_Bool use_short_devid ;

_Bool pdfreport_geoevent_newpage ;
_Bool pdfreport_hide_empty_geoevent ;
char pdfreport_dir[256] ;


/*
 * Utility macros to read from init-file
 */
#define INIT_INISTR(KEY,VAR,DEF) {strncpy(VAR, iniparser_getstring(dict, KEY, DEF), sizeof(VAR)-1); VAR[sizeof(VAR)-1] = '\0';}
#define INIT_INIBOOL(KEY,VAR,DEF) {VAR = iniparser_getboolean(dict,KEY, DEF);}
#define INIT_INIINT(KEY,VAR,DEF,MINV,MAXV) {VAR = validate(MINV,MAXV,KEY,iniparser_getint(dict, KEY, DEF));}


void
close_inifile(void) {
    if( dict ) {
        iniparser_freedict(dict);
        dict=NULL;
    }
}
/**
 * Setup the dictionary file (ini-file) name. Check if it is specified on
 * the command line otherwise check common locations.
 */
void
setup_inifile(void) {

    // Check for inifile at common locations
    if (*daemon_config_file) {
        // Specified on the command line. Overrides the default
        dict = iniparser_load(daemon_config_file);
    } else {
        snprintf(daemon_config_file, INIFILE_LEN - 1, "%s/%s/%s", CONFDIR, PACKAGE, INIFILE_NAME);
        daemon_config_file[INIFILE_LEN - 1] = '\0';
        dict = iniparser_load(daemon_config_file);
    }

    if (dict == NULL) {
        _vsyslogf(LOG_ERR, "Could not open configuration file \"%s\"\n", INIFILE_NAME);
        fprintf(stderr, "Could not open configuration file \"%s\"\n", INIFILE_NAME);
        exit(EXIT_FAILURE);
    }

}

/**
 * Read inisettings that are only read once at start of the daemon
 */
void
read_inisettings_startup(void) {

    /*--------------------------------------------------------------------------
     * STARTUP Section
     * --------------------------------------------------------------------------
     */
    INIT_INISTR("startup:run_as_user", run_as_user, DEFAULT_RUN_AS_USER);
    INIT_INIINT("startup:device_port", tcpip_device_port, DEFAULT_DEVICE_PORT, 1025, 60000);
    INIT_INIINT("startup:cmd_port", tcpip_cmd_port, DEFAULT_CMD_PORT, 1025, 60000);
    
}


/**
 * Get common master values from the ini file
 * Since iniparser is not reentrant we must do it here and not individually
 * in each thread. Since all of these values are read only before threads
 * are created there is no need to protect these with a mutex
 */
void
read_inisettings(void) {

    /*--------------------------------------------------------------------------
     * CONFIG Section
     *--------------------------------------------------------------------------
     */

    INIT_INIINT("config:max_clients", max_clients, DEFAULT_MAXCLIENTS, 2, 100);
#ifdef __APPLE__
    char tmpBuff[128];
    struct splitfields sfields;
    INIT_INISTR("config:stty_device", tmpBuff, DEFAULT_STTY_DEVICE);
    if( 0 == xstrsplitfields(tmpBuff, ',', &sfields) ) {
        if( sfields.nf >= MAX_OSX_USBMODEM ) {
            logmsg(LOG_ERR,"Too many STTY devices specified in config file");
            exit(EXIT_FAILURE);                    
        }
        for( size_t i=0; i < sfields.nf; i++) {
            cu_usbmodem_list[i] = xatoi(sfields.fld[i]);
            if( cu_usbmodem_list[i] < 1000 || cu_usbmodem_list[i] > 9999 ) {
                logmsg(LOG_ERR,"Index for STTY devices specified in STTY list in config file is out of range");
                exit(EXIT_FAILURE);                                
            }
        }
        for( size_t i=sfields.nf; i < MAX_OSX_USBMODEM; i++) {
            cu_usbmodem_list[i] = -1;
        }
    } else {
        logmsg(LOG_ERR,"Invalid configuration for stty_device list");
        exit(EXIT_FAILURE);        
    }
            
#else
    INIT_INIINT("config:stty_device", stty_device_startidx, DEFAULT_STTY_DEVICE, 0, 9);
#endif
    INIT_INIINT("config:client_idle_time", max_idle_time, DEFAULT_CLIENT_IDLE_TIME, 60, 12*3600);
    INIT_INIINT("config:device_idle_time", max_device_idle_time, DEFAULT_DEVICE_IDLE_TIME, 60, 600);

    INIT_INIINT("config:trackseg_split_time", trackseg_split_time, DEFAULT_TRACKSEG_SPLIT_TIME,-1,99999);
    INIT_INIINT("config:track_split_time", track_split_time, DEFAULT_TRACK_SPLIT_TIME,-1,99999);

    if( track_split_time > 0 && trackseg_split_time > 0 ) {
      if( track_split_time <= trackseg_split_time ) {
        logmsg(LOG_ERR,"Invalid configuration: track_split_time <= trackseg_split_time");
        exit(EXIT_FAILURE);
      }
    }

    INIT_INIBOOL("config:enable_raw_device_commands", enable_raw_device_commands, DEFAULT_RAW_DEVICE_COMMAND);
    INIT_INIBOOL("config:require_client_pwd", require_client_pwd, DEFAULT_REQUIRE_PASSWORD);
    INIT_INIBOOL("config:translate_device_reply", translateDeviceReply, DEFAULT_TRANSLATE_DEVICE_REPLY);
    INIT_INISTR("config:client_pwd", client_pwd, "");
    if (require_client_pwd && '\0' == *client_pwd) {
        logmsg(LOG_ERR, "Password required but no password specified in INI file!");
        exit(EXIT_FAILURE);
    }

    INIT_INISTR("config:attachment_compression", attachment_compression, DEFAULT_ATTACHMENT_COMPRESSION);

    INIT_INIINT("config:verbose_log", verbose_log, VERBOSE_LOG, 0, 3);

    INIT_INIBOOL("config:enable_gfen_tracking", enable_gfen_tracking, DEFAULT_ENABLE_GFEN_TRACKING);
    INIT_INIINT("config:gfen_tracking_interval", gfen_tracking_interval, DEFAULT_GFEN_TRACKING_INTERVAL,10,3600);

    INIT_INIBOOL("config:mail_on_tracker_conn", mail_on_tracker_conn, DEFAULT_MAIL_ON_TRACKER_CONN);
    INIT_INIBOOL("config:script_on_tracker_conn", script_on_tracker_conn, DEFAULT_SCRIPT_ON_TRACKER_CONN);

    INIT_INIBOOL("config:use_address_lookup",use_address_lookup,DEFAULT_USE_ADDRESS_LOOKUP);
    INIT_INIINT("config:address_lookup_proximity",address_lookup_proximity,DEFAULT_ADDRESS_LOOKUP_PROXIMITY,0,200);
    
    INIT_INISTR("config:google_api_key", google_api_key, DEFAULT_GOOGLE_API_KEY);
    
    /*--------------------------------------------------------------------------
     * Report section
     *-------------------------------------------------------------------------- 
     */
    INIT_INIBOOL("report:geoevent_newpage", pdfreport_geoevent_newpage, DEFAULT_PDFREPORT_GEOEVENT_NEWPAGE);
    INIT_INIBOOL("report:geoevent_hide_empty", pdfreport_hide_empty_geoevent, DEFAULT_PDFREPORT_GEOEVENT_HIDE_EMPTY);
    INIT_INISTR("report:pdfreport_dir", pdfreport_dir, DEFAULT_PDFREPORT_DIR);
    
    
    /*--------------------------------------------------------------------------
     * MAIL Section
     *--------------------------------------------------------------------------
     */
    INIT_INIBOOL("mail:enable_mail", enable_mail, 0);
    INIT_INISTR("mail:subject_prefix", mail_subject_prefix, DEFAULT_MAIL_SUBJECT_PREFIX);
    INIT_INIBOOL("mail:sendmail_on_event", send_mail_on_event, DEFAULT_SENDMAIL_ON_EVENT);
    INIT_INIBOOL("mail:use_html", use_html_mail, DEFAULT_USE_HTML_MAIL);
    INIT_INIBOOL("mail:smtp_use", smtp_use, 0)
    INIT_INISTR("mail:smtp_server", smtp_server, "");
    INIT_INISTR("mail:sendmail_address", send_mailaddress, "");
    INIT_INISTR("mail:daemon_email_from", daemon_email_from, "");
    INIT_INIINT("mail:smtp_port", smtp_port, -1, -1, 60000);
    INIT_INISTR("mail:smtp_user", smtp_user, "");
    INIT_INISTR("mail:smtp_pwd", smtp_pwd, "");
    INIT_INIBOOL("mail:force_mail_on_all_events",force_mail_on_all_events,DEFAULT_FORCE_MAIL_ON_EVENT);
    INIT_INIBOOL("mail:use_short_devid",use_short_devid,DEFAULT_USE_SHORT_DEVID);
    INIT_INIBOOL("mail:include_minimap",include_minimap,DEFAULT_INCLUDE_MINIMAP);
    INIT_INIINT("mail:minimap_overview_zoom", minimap_overview_zoom, DEFAULT_MINIMAP_OVERVIEW_ZOOM, 1, 20);
    INIT_INIINT("mail:minimap_detailed_zoom", minimap_detailed_zoom, DEFAULT_MINIMAP_DETAILED_ZOOM, 1, 25);
    INIT_INIINT("mail:minimap_width", minimap_width, DEFAULT_MINIMAP_WIDTH, 50, 500);
    INIT_INIINT("mail:minimap_height", minimap_height, DEFAULT_MINIMAP_HEIGHT, 50, 500);
    

}


/* EOF */
