/* =========================================================================
 * File:        G7CONFIG.H
 * Description: Read and handle config settings from the ini file.
 *              The header files defines the available variables as read
 *              from the inifile.
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

#ifndef G7CONFIG_H
#define	G7CONFIG_H

#ifdef	__cplusplus
extern "C" {
#endif

/* The iniparser library used is dependent on the configuration. It can either be the
 * system wide existing library or the daemon can use an internal version. Hence we need
 * to adjust which ini file we will be reading. The controlling variable is defined in the
 * generated config.h file.
 */
#ifdef HAVE_LIBINIPARSER
#include <iniparser.h>
#else
#include "libiniparser/iniparser.h"
#endif

#define DO_PRAGMA(x) _Pragma (#x)
#define TODO(x) DO_PRAGMA(message ("TODO - " #x))

/*
 * All defines below gives the default values for various ini file settings
 * which are used in the case that the setting wasn't found in the ini-file.
 */

/**
 * DEFAULT_DEVICE_PORT
 * Default TCP/IP port that the daemon is listening on and expecting the device to
 * send tracking information on. This can be configured in the ini file.
 */
#define DEFAULT_DEVICE_PORT 3400

/**
 * DEFAULT_CMD_PORT
 * Default TCP/IP port that the daemon is listening on and expecting to receive commands on
 * This can be configured in the ini file.
 */
#define DEFAULT_CMD_PORT 3100

/**
 * DEFAULT_REQUIRE_PASSWORD
 * Default value for password to access the command port
 */
#define DEFAULT_REQUIRE_PASSWORD 1

/**
 * INIFILE_NAME string
 * Name and path of inifile
 */
#define INIFILE_NAME "g7ctrl.conf"

/**
 * DEFAULT_MAXCLIENTS int
 * Maximu number of clients we allow to connect
 */
#define DEFAULT_MAXCLIENTS 5

/**
 * DEFAULT_CLIENT_IDLE_TIME int
 * Default time for client command connection timeout (20 min)
 */
#define DEFAULT_CLIENT_IDLE_TIME 1200

/**
 * DEFAULT_DEVICE_IDLE_TIME int
 * Default time for device tracker connection timeout (3 min)
 * The operator is usually very aggressive in killing of dynamic
 * IP addresses even if we have KEEP_ALIVE packages every 30s
 */
#define DEFAULT_DEVICE_IDLE_TIME 180

/**
 * DEFAULT_STTY string
 * Deafult index for USB ttyACM<n> device
 */
#ifdef __APPLE__
#define DEFAULT_STTY_DEVICE "1411"
#else
#define DEFAULT_STTY_DEVICE 0
#endif

/**
 * DEFAULT_DAEMONIZE bool
 * If we should run as a daemon, this setting is the default if not specified on the command
 * line or in the ini-filename
 */
#define DEFAULT_DAEMONIZE 1

/**
 * MAX_PASSWORD_LEN int
 * Maximum size of password
 */
#define MAX_PASSWORD_LEN 32

/**
 * LOGFILE_NAME string
 * Optional logfile name (full path).
 */
#define LOGFILE_SYSLOG "syslog"

/**
 * DEFAULT_LOG_DIR
 * Default directory for log files. Note that if the daemon is not
 * started as root then the logfile name must be given as argument on
 * startup.
 */
#define DEFAULT_LOG_FILE "/var/log/g7ctrl/g7ctrl.log"

/**
 * VERBOSE_LOG bool
 * Should the log be more verbose
 */
#define VERBOSE_LOG 1

/**
 * DEFAULT_SENDMAIL_ON_EVENT bool
 * Should an email be sent when we receive a device event
 */
#define DEFAULT_SENDMAIL_ON_EVENT 0

/**
 * DEFAULT_USE_HTML_MAIL bool
 * Should mail be sent in dual HTML/TEXT format
 */
#define DEFAULT_USE_HTML_MAIL 1

/**
 * DEFAULT_MAIL_SUBJECT_PREFIX string
 * Default prefix in subject header for mails sent from the daemon
 */    
#define DEFAULT_MAIL_SUBJECT_PREFIX "GM7 "
    
/**
 * DEFAULT_RUN_AS_USER string
 * Which user to run as
 */
#define DEFAULT_RUN_AS_USER DEFAULT_USER

/**
 * DEFAULT_RAW_DEVICE_COMMAND string
 * Should we allow user to enter "raw" device commands
 */
#define DEFAULT_RAW_DEVICE_COMMAND 0

/**
 * DEFAULT_FORCE_MAIL_ON_EVENT bool
 * Generate mail for all events except REC
 */
#define DEFAULT_FORCE_MAIL_ON_EVENT 0

/**
 * DEFAULT_TRACKSEG_SPLIT_TIME int
 * The time in minutes between location updates to consider them
 * to be different tracks segments.
 * Defaults to 60min
 */
#define DEFAULT_TRACKSEG_SPLIT_TIME -1

/**
 * DEFAULT_TRACK_SPLIT_TIME int
 * The time in minutes between location updates to consider them
 * to be different tracks (e.g. create a new track)
 * Defaults to 4h
 */
#define DEFAULT_TRACK_SPLIT_TIME 240

/**
 * Default name for DB file
 */
#define DEFAULT_TRACKER_DB "gm7tracker_db.sqlite3"

/**
 * Default size for geolocation cache vectors
 */
#define DEFAULT_GEOCACHE_ADDRESS_SIZE 10000
#define DEFAULT_GEOCACHE_MINIMAP_SIZE 20000
        
/**
 * Default file name for storing the geocache
 */
#define DEFAULT_ADDRESS_GEOCACHE_FILE "geoloc_addrcache.txt"

/**
 * Default file name for backup of saved geoloc addr cache
 */
#define DEFAULT_ADDRESS_GEOCACHE_BACKUPFILE "geoloc_addrcache_backup.txt"

/**
 * Default file name for storing the geocache
 */
#define DEFAULT_MINIMAP_GEOCACHE_FILE "geoloc_minimapcache.txt"

/**
 * Default file name for backup of saved geoloc minimap cache
 */
#define DEFAULT_MINIMAP_GEOCACHE_BACKUPFILE "geoloc_minimapcache_backup.txt"

/**
 * Default file name for stored hit/miss statistics between invocations of daemon
 */    
#define DEFAULT_GEOCACHE_SAVEDSTAT_FILE "geoloc_cachestat.txt"

/**
 * Default directory where all minimap images are stored under
 * the "DEFAULT_DB_DIR"
 */    
#define DEFAULT_MINIMAP_GEOCACHE_DIR "map_cache"
    
/**
 * Filename where we also store the last downloaded
 * locations (this happens when the DLREC command is given.
 */
#define LAST_DLREC_FILE "/tmp/gm7_last_dlrec.txt"

/**
 * DEFAULT_ATTACHMENT_COMPRESSION string
 * Default attachment compression when mailing GPX exports
 */
#define DEFAULT_ATTACHMENT_COMPRESSION "gzip"

/**
 * DEFAULT_TRANSLATE_DEVICE_REPLY bool
 * Default attachment compression when mailing GPX exports
 */
#define DEFAULT_TRANSLATE_DEVICE_REPLY 1

/**
 * DEFAULT_ENABLE_GFEN_TRACKING bool
 * Default value for enabling automatic tracking after receiving a
 * GFEN event
 */
#define DEFAULT_ENABLE_GFEN_TRACKING 1

/**
 * DEFAULT_GFEN_TRACKING_INTERVAL int
 * Default value for automatic GFEN tracking interval in seconds
 */
#define DEFAULT_GFEN_TRACKING_INTERVAL 60

/**
 * DEFAULT_MAIL_ON_TRACKER_CONN bool
 * Default value for flag if mail should be sent on a new tracker connection
 */
#define DEFAULT_MAIL_ON_TRACKER_CONN 0

/**
 * DEFAULT_USE_SHORT_TRACKERID bool
 * Default value for flag to determine if short version of tracker ID should
 * be used
 */
#define DEFAULT_USE_SHORT_DEVID 0
    
    
/**
 * DEFAULT_SCRIPT_ON_TRACKER_CONN bool
 * Default value for flag if script should be executed on a new tracker
 * connection
 */
#define DEFAULT_SCRIPT_ON_TRACKER_CONN 0

/**
 * USE_ADDRESS_LOOKUP_IN_MAIL bool
 * Use Google service reverse lookup to translate coordinates to an
 * approximate address
 */
#define DEFAULT_USE_ADDRESS_LOOKUP 0

/**
 * ADDRESS_LOOKUP_PROXIMITY int
 * How close (in meters) a lookup must be in order to be considered a hit among
 * the cached addresses.
 */
#define DEFAULT_ADDRESS_LOOKUP_PROXIMITY 20

/**
 * DEFAULT_MAIL_WITH_MINIMAP bool
 * Determine of a minimap (overview & detail) should be included in the event
 * mail.
 */
#define DEFAULT_INCLUDE_MINIMAP 0

/**
 * DEFAULT_MINIMAP_OVERVIEW_ZOOM int
 * Deafult google zoom factor for overview minimap
 */    
#define DEFAULT_MINIMAP_OVERVIEW_ZOOM 9

/**
 * DEFAULT_MINIMAP_DETAILED_ZOOM int
 * Deafult google zoom factor for detailed minimap
 */    
#define DEFAULT_MINIMAP_DETAILED_ZOOM 15
    
/**
 * DEFAULT_MINIMAP_WIDTH int
 * Default width (in pixels) of map
 */
#define DEFAULT_MINIMAP_WIDTH 200

/**
 * DEFAULT_MINIMAP_HEIGHT int
 * Default height (in pixels) of map
 */
#define DEFAULT_MINIMAP_HEIGHT 200

/**
 * DEFAULT_GOOGLE_API_KEY string
 * Optional Google API key to allow higher rate of API calls to
 * Google address lookup
 */
#define DEFAULT_GOOGLE_API_KEY ""

/// Should the reply from device be translated to plaintext
extern _Bool translateDeviceReply;

/// TCP/IP Port to listen to and expect the device to connect
extern unsigned int tcpip_device_port;

/// TCP/IP Port to listen to and expect commands
extern unsigned int tcpip_cmd_port;

/// Max idle time in seconds before automatic logout
extern unsigned max_idle_time ;

/// Maximum number of clients allowed to connect to us
extern size_t max_clients;

/// Maximum idle time after a device have connected to us
extern unsigned max_device_idle_time;

/// Should we run as a daemon or not
extern int daemonize;

/// What username are we running as
extern char run_as_user[];

/// Is it allowed to send raw commands directly to the device ("$WP+ ...)
extern int enable_raw_device_commands;

/// Index for default USB device
#ifdef __APPLE__
#define MAX_OSX_USBMODEM 8
extern int cu_usbmodem_list[];
#else
// Start index of STTY device for USB port on Linux
extern int stty_device_startidx;
#endif

/// Is password needed when connecting on command channel
extern _Bool require_client_pwd;
extern char client_pwd[];

/// Name of inifile
#define INIFILE_LEN 256
extern char daemon_config_file[];

/// Name of pidfile
#define PIDFILE_LEN 256
extern char pidfile[];
extern int pidfile_fd;


/// Logfile details
#define MAX_LOGFILE_NAME_LEN 512
extern int verbose_log;
extern char logfile_name[];

#define MAX_DATA_DIR_LEN 512
// Directory for static data
extern char data_dir[];

#define MAX_DB_DIR_LEN 512
// Directory for dynamic data (i.e. data that changes as the program is run)
extern char db_dir[];

// Automatic GFEN handling
extern int gfen_tracking_interval;
extern _Bool enable_gfen_tracking;

// Splitting time when exporting
extern int track_split_time;
extern int trackseg_split_time;

extern char attachment_compression[];

extern _Bool use_address_lookup ;
extern int address_lookup_proximity;

extern char google_api_key[64];

extern char mail_subject_prefix[128];

/**
 * Mail setting. Determine if we should send mail on errors and other events and what address
 * to use. Read from the inifile normally.
 */
extern int enable_mail;
extern char daemon_email_from[64] ;
extern int  send_mail_on_event ;
extern char send_mailaddress[64] ;
extern int  use_html_mail ;
extern int  smtp_use ;
extern char smtp_server[64] ;
extern char smtp_user[64] ;
extern char smtp_pwd[64] ;
extern int  smtp_port ;
extern int force_mail_on_all_events ;
extern _Bool include_minimap ; 
    
/**
 * Handling of included mini maps in mail
 */
extern unsigned minimap_overview_zoom;
extern unsigned minimap_detailed_zoom;
extern unsigned minimap_width;
extern unsigned minimap_height;

/**
 * Geolocation cache size
 */
extern unsigned geocache_address_size;
extern unsigned geocache_minimap_size;


extern _Bool script_on_tracker_conn ;
extern _Bool mail_on_tracker_conn ;

extern _Bool use_short_devid ;

/**
 * DEFAULT_PDFREPORT_GEOEVENT_NEWPAGE bool
 * Default option for starting the geo-fence event tables on a new page in the PDF report
 */
#define DEFAULT_PDFREPORT_GEOEVENT_NEWPAGE 0

/**
 * DEFAULT_PDFREPORT_GEOEVENT_HIDE_EMPTY bool
 * Default option for hiding empty geo-fence event tables in the PDF report
 */
#define DEFAULT_PDFREPORT_GEOEVENT_HIDE_EMPTY 1

extern _Bool pdfreport_geoevent_newpage ;
extern _Bool pdfreport_hide_empty_geoevent ;

/**
 * DEFAULT_PDFREPORT_DIR string
 */
#define DEFAULT_PDFREPORT_DIR "/tmp"
extern char pdfreport_dir[1024] ;

/**
 * DEFAULT_MIN_BATT_VOLTAGE double
 */
#define DEFAULT_MIN_BATT_VOLTAGE 3.60
extern double min_battery_voltage ;

/**
 * DEFAULT_MAX_BATT_VOLTAGE double
 */
#define DEFAULT_MAX_BATT_VOLTAGE 4.20
extern double max_battery_voltage ;

/**
 * Handle to dictionary which holds all variables read from the inifile
 */
extern dictionary *inifile_dict;

void
close_inifile(void);

void
setup_inifile(void);

void
read_inisettings(void);

void
read_inisettings_startup(void);

/** Delimiter used in location data*/
#define LOC_DELIM ','

// Symbolic constants for the index of the different data fields in the
// location update sent from the device.
#define GM7_LOC_DEVID 0
#define GM7_LOC_DATE 1
#define GM7_LOC_LON 2
#define GM7_LOC_LAT 3
#define GM7_LOC_SPEED 4
#define GM7_LOC_HEADING 5
#define GM7_LOC_ALT 6
#define GM7_LOC_SAT 7
#define GM7_LOC_EVENTID 8
#define GM7_LOC_VOLT 9
#define GM7_LOC_DETACH 10


/** 
 * Subject for mail sent 
 */

// Subject for mails when a new tracker connection is made.
// The parameters are: mail_subject_prefix, short_devid
#define SUBJECT_NEWCONNECTION "%s[ID:%s] New connection"

// Subject for notification of maile rate limit exceeded
// The parameters are: mail_subject_prefix
#define SUBJECT_LOCRATEEXCEEDED "%sGoogle rate limit exceeded"

// Subject in mail with exported DB
// The parameters are: mail_subject_prefix
#define SUBJECT_EXPORTEDDB  "%sExported database" 

// Subject in mail with last location
// The parameters are: mail_subject_prefix
#define SUBJECT_LASTLOCATION "%s[ID:%s] Last location in DB"

// Subject in mails with event updated
// The parameters are: mail_subject_prefix, Device ID and Event description
#define SUBJECT_EVENTMAIL  "%s[ID:%s] Event: \"%s\""


// Name for the events. The names are used in the mails sent when an event is received
#define EVENTDESC_EVENT_GETLOC "Position data"
#define EVENTDESC_EVENT_REC "Logging data"
#define EVENTDESC_EVENT_TRACK "Position update"
#define EVENTDESC_EVENT_TIMER "Timer report"
#define EVENTDESC_EVENT_WAKEUP "Wake Up Report"
#define EVENTDESC_EVENT_SLEEP "Enter Sleeping Report"
#define EVENTDESC_EVENT_LOWBATT "Internal Battery Low Alert"
#define EVENTDESC_EVENT_GFEN "Virtual fence crossing"
#define EVENTDESC_EVENT_SETRA "Unit Detaching Report"


// Misc. messages

// Invalid command
// Parameters: The command given
#define INVALID_CMD_MSG_GET "Command \"get %s\" is not valid. Try help."

// Invalid set command
// Parameters: The command given
#define INVALID_CMD_MSG_SET "Command \"set %s\" is not valid. Try help."

// Invalid do command
// Parameters: The command given
#define INVALID_CMD_MSG_DO "Command \"do %s\" is not valid. Try help."

// Invalid binary command
// Parameters: The command given
#define INVALID_BINARY_CMD "Command \"%s\" is not valid. Try help."

// Invalid user authentication
#define INVALID_AUTHENTICATION "Authentication failed. Connection closed."

#ifdef	__cplusplus
}
#endif

#endif	/* G7CONFIG_H */

