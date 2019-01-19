/* =========================================================================
 * File:        export.c
 * Description: Handle export of DB to GPX, KML and CSV format
 * Author:      Johan Persson (johan162@gmail.com)
 *
 * Copyright (C) 2014 Johan Persson
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
#include <sqlite3.h>
#include <sys/syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <math.h>
#include <signal.h>

#include "config.h"
#include "g7config.h"
#include "logger.h"
#include "g7ctrl.h"
#include "utils.h"
#include "futils.h"
#include "libxstr/xstr.h"
#include "dbcmd.h"
#include "mailutil.h"
#include "export.h"



// Standard XML ingress for KML export
#define KML_XML_HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"\
"<kml xmlns=\"http://www.opengis.net/kml/2.2\"\n"\
"creator=\"g7ctrl http://www.sourceforge.com/p/g7ctrl\">\n"

// Standard XML ingress for GPX export
#define GPX_XML_HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<gpx version=\"1.1\" \n\
creator=\"g7ctrl http://www.sourceforge.com/p/g7ctrl\" \n\
xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n\
xmlns=\"http://www.topografix.com/GPX/1/1\"\n\
xsi:schemaLocation=\"http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd\">\n"

#define PROP_XML_HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<g7ctr version=\"1.0\" \n"\
"exportdate=\"%d-%02d-%02dT%02d:%02d:%02dZ\" \n"\
"creator=\"g7ctrl http://www.sourceforge.com/p/g7ctrl\" \n"\
"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"\
"xmlns=\"http://www.aditus.nu/G7CTRLDB/1.0\"\n"\
"xsi:schemaLocation=\"http://www.aditus.nu/G7CTRLDB/1.0 http://www.aditus.nu/G7CTRLDB/1.0/g7ctrl.rng\">\n"

#define EXPORTDB_BASE_NAME "g7db_export"

// Initial size of internal memory list for export. We also use this
// as the chink size when the list needs to grow
#define INTERNAL_LIST_CHUNKSIZE 50000

/** Determine lat/long bounding box */
char minlat[32], maxlat[32],
minlon[32], maxlon[32];

/** Length of the current result set read from the DB. Needs to
 * be global since it is used by a callback routine
 */
size_t resSetLength = 0;

/** The current maximum allocated size for th internal memory list which
 is created when prepare for the db export
 */
size_t g7locListSize = 0;

/**
 * Internal list to hold the exported DB in memory as an
 * intermediate step before writing the exported file in the
 * chosen format. Maximum size is 500,000 location points
 */
struct g7loc_t *g7loc_list = NULL;

// Silent gcc about unused "arg"in the callbacks
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/**
 * Result callback from sqlite3_exec(). This callback is called for each row
 * in the result set. This is called when we do the SQL for an export
 *
 * @param NotUsed   Required argument for callback but not used
 * @param nColumns  Number of columns in row
 * @param azSolVals    Column values
 * @param azColName  Name of each column
 * @return 0 to continue to the next row, -1 to abort
 */
static int
sqres_export_callback(void *NotUsed, int nColumns, char **azSolVals, char **azColName) {

    if (nColumns != 11) {
        logmsg(LOG_ERR, "SELECT statement returned wrong number of columns!");
        return 1; // Abort
    }

    if (resSetLength >= g7locListSize) {
        logmsg(LOG_DEBUG, "Increasing internal memory buffer with %d bytes", INTERNAL_LIST_CHUNKSIZE);
        // We need to expand the internal dataset to make room for the entire database
        g7loc_list = (struct g7loc_t *) realloc(g7loc_list, g7locListSize + INTERNAL_LIST_CHUNKSIZE);
        if (NULL == g7loc_list) {
            logmsg(LOG_CRIT, "Out of memory when exporting DB");
            return 1;
        }
        g7locListSize += INTERNAL_LIST_CHUNKSIZE;
    }

    for (int i = 0; i < nColumns; ++i) {

        if (NULL == azSolVals[i]) {
            logmsg(LOG_CRIT, "SQL SELECT returned a NULL valued column (index=%d).", i);
            return 1;
        }

    }
    struct g7loc_t *p = &g7loc_list[resSetLength];
    strncpy(p->lat, azSolVals[0], 15);
    strncpy(p->lon, azSolVals[1], 15);
    strncpy(p->approxaddr, azSolVals[2], 127);
    strncpy(p->date, convert_to_utc(azSolVals[3]), 31);

    // Convert the datetime to a timestamp t be able to compare locations
    // easier during export
    struct tm tm;
    CLEAR(tm); // strptime() is dangerous since it will not initialize tm
    strptime(azSolVals[2], "%Y%m%d%H%M%S", &tm);
    p->date_timestamp = mktime(&tm);

    strncpy(p->altitude, azSolVals[4], 7);
    strncpy(p->speed, azSolVals[5], 7);
    strncpy(p->voltage, azSolVals[6], 7);
    strncpy(p->event, azSolVals[7], 7);
    strncpy(p->heading, azSolVals[8], 7);
    strncpy(p->deviceid, azSolVals[9], 15);
    strncpy(p->satellite, azSolVals[10], 7);

    // Keep track of boundary lat/long values in set since this is needed
    // in the GPX format
    if (0 == resSetLength) {
        xstrlcpy(minlat, p->lat, sizeof(minlat));
        xstrlcpy(maxlat, p->lat, sizeof(maxlat));
        xstrlcpy(minlon, p->lon, sizeof(minlon));
        xstrlcpy(maxlon, p->lon, sizeof(maxlon));
    } else {
        if (strcmp(p->lat, minlat) < 0)
            xstrlcpy(minlat, p->lat, sizeof(minlat));
        if (strcmp(p->lon, minlon) < 0)
            xstrlcpy(minlon, p->lon, sizeof(minlon));

        if (strcmp(p->lat, maxlat) > 0)
            xstrlcpy(maxlat, p->lat, sizeof(maxlat));
        if (strcmp(p->lon, maxlon) > 0)
            xstrlcpy(maxlon, p->lon, sizeof(maxlon));
    }
    resSetLength++;
    return 0;
}

#pragma GCC diagnostic pop

/**
 * Free the internal export structure used to read the DB to memory
 */
void
free_internal_set(void) {
    if (g7loc_list) {
        free(g7loc_list);
        g7loc_list = NULL;
    }
}

/**
 * Interpret given datetime string as UTZ time. It is used to convert
 * a string from (for example) "20131201093042" to "2013-12-01T09:30:42Z"
 * Note that this does not do any timezone calculation but only a syntactic
 * change of the string.
 * @param datetime
 * @return A pointer to a static buffer that holds the formatted
 * time. NOT thread safe due to the static buffer.
 *
 * FIXME: Add real time conversion not just dummy placeholder!!
 */
char *
convert_to_utc(const char *datetime) {
    static char buff[32];
    const char *p = datetime;
    strncpy(buff, p, 4);
    p += 4;
    buff[4] = '-';
    buff[5] = *p++;
    buff[6] = *p++;
    buff[7] = '-';
    buff[8] = *p++;
    buff[9] = *p++;
    buff[10] = 'T';
    buff[11] = *p++;
    buff[12] = *p++;
    buff[13] = ':';
    buff[14] = *p++;
    buff[15] = *p++;
    buff[16] = ':';
    buff[17] = *p++;
    buff[18] = *p++;
    buff[19] = 'Z';
    buff[20] = '\0';
    return buff;
}

/**
 * Export the DB to intermediary in-memory structure.
 * The export is filtered according to the arguments given.
 * Note: It is the calling routines responsibility to call free_internal_set()
 * after the internal memory structure is no longer needed.
 * @param sqlDB     DB Handle
 * @param fromDate  From date and time
 * @param toDate    To date and time
 * @param deviceId  Device id
 * @param eventId   Event id
 * @return 0 on success, -2 if fromDate > toDate, -1 on other failure (most likely OOM)
 */
int
export_to_internal_set(sqlite3 *sqlDB, char *fromDate, char *toDate, char *deviceId, char *eventId) {

    char q[1024];
    snprintf(q, sizeof (q),
            "select fld_lat, fld_lon, fld_approxaddr, fld_datetime, fld_altitude, fld_speed,"
            "fld_voltage, fld_event, fld_heading, fld_deviceid, fld_satellite "
            "from %s ", DB_TABLE_LOC);


    char w[512];
    if (strcmp(fromDate, toDate) > 0) {
        logmsg(LOG_ERR, "export_to_internal_set() : fromDate > toDate");
        return -2;
    }

    *w = '\0';
    db_add_wcond(w, sizeof(w), "fld_datetime", ">=", fromDate);
    db_add_wcond(w, sizeof(w),"fld_datetime", "<=", toDate);
    db_add_wcond(w, sizeof(w),"fld_deviceid", "=", deviceId);
    db_add_wcond(w, sizeof(w),"fld_event", "=", eventId);
    xstrlcat(q, w, sizeof(q));
    xstrlcat(q, ";", sizeof(q));
    logmsg(LOG_DEBUG, "SQL: \"%s\"", q);
    // Pre-allocate space for result set
    // Assume result set < MAX_ENTRYLIST entries
    g7loc_list = _chk_calloc_exit(INTERNAL_LIST_CHUNKSIZE * sizeof (struct g7loc_t));
    g7locListSize = INTERNAL_LIST_CHUNKSIZE;

    resSetLength = 0;
    *minlat = '\0';
    *maxlat = '\0';
    *minlon = '\0';
    *maxlon = '\0';
    char *errMsg;

    int rc = sqlite3_exec(sqlDB, q, sqres_export_callback, (void *) 0, &errMsg);

    if (rc) {
        if (SQLITE_ABORT == rc) {
            logmsg(LOG_ERR, "Result set is too large to hold in memory");
            return -1;
        } else {
            logmsg(LOG_DEBUG, "Can execute SQL [\"%s\"]", errMsg);
            sqlite3_free(errMsg);
            return -1;
        }
    }
    return 0;
}

/**
 * Open/create file to export to and return the file handle.
 * We have to check the absolute path here. Three cases
 *  1. If user has supplied a relative path give an error
 *  2. If user has only supplied a filename then store the file in the DB directory
 *  3. If user has supplied absolute path name try to use that
 *
 * @param[in] fileName File to create
 * @param[out] newFileName If != NULL then the adjusted filename will be stored
 * in this buffer
 * @param[in] maxlen Maximum size for newFileName buffer
 * @return filehandle, < 0 on failure
 */
int
open_export_file(const char *fileName, char *newFileName, size_t maxlen) {

    char wfileName[512];
    if (strlen(fileName) > sizeof (wfileName))
        return -1;
    xstrlcpy(wfileName, fileName, sizeof(wfileName));
    xstrtrim(wfileName);
    _Bool isAbsPath = *wfileName == '/';
    const size_t wlen = strlen(wfileName);
    size_t i = 1;
    while (i < wlen && wfileName[i] != '/') {
        ++i;
    }
    _Bool isRelPath = i < wlen && !isAbsPath;

    if (isRelPath) {
        logmsg(LOG_DEBUG, "Cannot use relative paths in export filename \"%s\"", wfileName);
        return -1;
    }
    char adjFileName[512];
    if (!isAbsPath) {
        snprintf(adjFileName, sizeof (adjFileName), "%s/%s", data_dir, wfileName);
    } else {
        strncpy(adjFileName, wfileName, sizeof (adjFileName) - 1);
    }

    if (newFileName != NULL) {
        strncpy(newFileName, adjFileName, maxlen - 1);
    }

    logmsg(LOG_DEBUG, "Adjusted filename: \"%s\"", adjFileName);

    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    int fd = open(adjFileName, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) {
        logmsg(LOG_DEBUG, "Cannot open export file for writing. ( %d : %s )", errno, strerror(errno));
        return -1;
    }
    return fd;
}

/**
 * Export to the proprietary XML format to include all columns in the DB
 * @param fileName
 * @return  0 on success, -1 on failure
 */
int
export_to_prop_xml(const char *fileName) {
    int fd = open_export_file(fileName, NULL, 0);
    if (fd < 0) {
        return -1;
    }
    int y, m, d, h, mi, s;
    fromtimestamp(time(NULL), &y, &m, &d, &h, &mi, &s);
    _writef(fd, PROP_XML_HEADER, y, m, d, h, mi, s);
    _writef(fd, "<bounds minlat=\"%s\" minlon=\"%s\" maxlat=\"%s\" maxlon=\"%s\" />\n",
            minlat, minlon, maxlat, maxlon);
    for (size_t i = 0; i < resSetLength; i++) {
        _writef(fd, "  <event eventid=\"%s\" devid=\"%s\" lat=\"%s\" lon=\"%s\" alt=\"%s\">\n",
                g7loc_list[i].event, g7loc_list[i].deviceid, g7loc_list[i].lat, g7loc_list[i].lon, g7loc_list[i].altitude);
        _writef(fd, "    <address>%s</address>\n", g7loc_list[i].approxaddr);
        _writef(fd, "    <datetime>%s</datetime>\n", g7loc_list[i].date);
        _writef(fd, "    <speed>%s</speed>\n", g7loc_list[i].speed);
        _writef(fd, "    <voltage>%s</voltage>\n", g7loc_list[i].voltage);
        _writef(fd, "    <heading>%s</heading>\n", g7loc_list[i].heading);
        _writef(fd, "    <sat>%s</sat>\n", g7loc_list[i].satellite);
        _writef(fd, "  </event>\n");
    }
    _writef(fd, "</g7ctrl>\n");
    return 0;
}

/**
 * Dump internal set into GPX format
 * @param fileName File to export to
 * @return  0 on success, -1 on failure
 */
int
export_to_gpx(const char *fileName) {

    int fd = open_export_file(fileName, NULL, 0);
    if (fd < 0) {
        logmsg(LOG_ERR, "Cannot open export file! ( %d : %s)", errno, strerror(errno));
        return -1;
    }

    unsigned long prevDateTime = 0;
    size_t numTrack = 1;
    if (resSetLength > 0) {
        prevDateTime = g7loc_list[0].date_timestamp;
    }
    int y, m, d, h, mi, s;
    fromtimestamp(time(NULL), &y, &m, &d, &h, &mi, &s);
    _writef(fd, GPX_XML_HEADER);
    _writef(fd, "<metadata>\n");
    _writef(fd, "  <time>%d-%02d-%02dT%02d:%02d:%02dZ</time>\n", y, m, d, h, mi, s);
    _writef(fd, "</metadata>\n");
    _writef(fd, "<bounds minlat=\"%s\" minlon=\"%s\" maxlat=\"%s\" maxlon=\"%s\" />\n",
            minlat, minlon, maxlat, maxlon);
    _writef(fd, "<trk>\n");
    _writef(fd, "  <name>Track %02zd: GM7 Xtreme GPS tracker</name>\n", numTrack++);
    _writef(fd, "  <trkseg>\n");
    for (size_t i = 0; i < resSetLength; i++) {
        if (track_split_time > 0 && g7loc_list[i].date_timestamp - prevDateTime > (unsigned long) track_split_time * 60) {
            _writef(fd, "  </trkseg>\n");
            _writef(fd, "</trk>\n");
            _writef(fd, "<trk>\n");
            _writef(fd, "  <name>Track %02zd: GM7 Xtreme GPS tracker</name>\n", numTrack++);
            _writef(fd, "  <trkseg>\n");
            logmsg(LOG_DEBUG, "Splitting GPX TRACK at location %05zd (diff=%lu, prev=%lu, curr=%lu)", i,
                    g7loc_list[i].date_timestamp - prevDateTime,
                    prevDateTime,
                    g7loc_list[i].date_timestamp
                    );
        } else if (trackseg_split_time > 0 && g7loc_list[i].date_timestamp - prevDateTime > (unsigned long) trackseg_split_time * 60) {
            _writef(fd, "  </trkseg>\n");
            _writef(fd, "  <trkseg>\n");
            logmsg(LOG_DEBUG, "Splitting GPX TRACKSEG at location %05zd (diff=%lu, prev=%lu, curr=%lu)", i,
                    g7loc_list[i].date_timestamp - prevDateTime,
                    prevDateTime,
                    g7loc_list[i].date_timestamp
                    );
        }
        prevDateTime = g7loc_list[i].date_timestamp;

        _writef(fd, "    <trkpt lat=\"%s\" lon=\"%s\">\n"
                "      <ele>%s</ele>\n"
                "      <time>%s</time> <timestamp>%lu</timestamp>\n"
                "      <course>%s</course>\n"
                "      <speed>%s</speed>\n"
                "      <sat>%s</sat>\n"
                "    </trkpt>\n",
                g7loc_list[i].lat, g7loc_list[i].lon,
                g7loc_list[i].altitude,
                g7loc_list[i].date, g7loc_list[i].date_timestamp,
                g7loc_list[i].heading,
                g7loc_list[i].speed,
                g7loc_list[i].satellite);
    }
    _writef(fd, "  </trkseg>\n");
    _writef(fd, "</trk>\n");
    _writef(fd, "</gpx>\n");
    close(fd);
    return 0;
}

/**
 * Export DB using KML format
 * @param fileName File to export to
 * @return  0 on success, -1 on failure
 */
int
export_to_kml(const char *fileName) {

    int fd = open_export_file(fileName, NULL, 0);
    if (fd < 0) {
        logmsg(LOG_ERR, "Cannot open export file! ( %d : %s)", errno, strerror(errno));
        return -1;
    }

    int y, m, d, h, mi, s;
    fromtimestamp(time(NULL), &y, &m, &d, &h, &mi, &s);
    _writef(fd, KML_XML_HEADER);
    for (size_t i = 0; i < resSetLength; i++) {
        _writef(fd, "<placemark>\n");
        _writef(fd, "  <name>#%d</name>\n", (int) i);
        _writef(fd, "  <description>%s</description>\n", g7loc_list[i].date);
        _writef(fd, "  <point>\n");
        _writef(fd, "    <coordinates>%s,%s</coordinates>\n", g7loc_list[i].lat, g7loc_list[i].lon);
        _writef(fd, "  </point>\n");
        _writef(fd, "</placemark>\n");
    }
    _writef(fd, "</kml>\n");
    close(fd);
    return 0;
}

/**
 * Export DB using Comma Separated file (CSV). All fields are exported and the
 * first row has all the column names
 * @param fileName File to export to
 * @return  0 on success, -1 on failure
 */
int
export_to_csv(const char *fileName) {
    int fd = open_export_file(fileName, NULL, 0);
    if (fd < 0) {
        logmsg(LOG_ERR, "Cannot open export file! ( %d : %s)", errno, strerror(errno));
        return -1;
    }

    // First write out the name of the columns
    _writef(fd, "date,device,latitude,longitude,address,elevation,speed,heading,satellite,event,voltage\n");

    // The export all the data in the result set
    for (size_t i = 0; i < resSetLength; i++) {
        _writef(fd, "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%s,%s,%s,%s,%s,\"%s\"\n",
                g7loc_list[i].date,
                g7loc_list[i].deviceid,
                g7loc_list[i].lat, g7loc_list[i].lon,
                g7loc_list[i].approxaddr,
                g7loc_list[i].altitude,
                g7loc_list[i].speed,
                g7loc_list[i].heading,
                g7loc_list[i].satellite,
                g7loc_list[i].event,
                g7loc_list[i].voltage);
    }
    _writef(fd, "\n");
    close(fd);
    return 0;
}

/**
 * Export DB using JSON format.
 * The format is:
 * {
 *   "ver": <VERSION>,
 *   "exportdate": <DATE>,
 *   "creator": "g7ctrl http://www.sourceforge.com/p/g7ctrl",
 *   "boundingbox": {
 *       "minlat": <MINLAT>,
 *       "minlon": <MINLON>,
 *       "maxlat": <MAXLAT>,
 *       "maxlon": <MAXLON>
 *   },
 *   "positions": [
 *       [ <DATE>,<DEVICEID>,<LAT>,<LON>,<APPROXADDR>,<ALTITUDE>,<SPEED>,<HEADING>,<SATELLITES>,<EVENTID>,<VOLTAGE> ],
 *       [ ... ],
 *       [ ... ]
 *   ]
 * }
 *
 * @param fileName File to export to
 * @return  0 on success, -1 on failure
 */
int
export_to_json(const char *fileName) {
    int fd = open_export_file(fileName, NULL, 0);
    if (fd < 0) {
        logmsg(LOG_ERR, "Cannot open export file! ( %d : %s)", errno, strerror(errno));
        return -1;
    }
    int y, m, d, h, mi, s;
    fromtimestamp(time(NULL), &y, &m, &d, &h, &mi, &s);
    _writef(fd, "{");
    _writef(fd, "\"ver\":\"1.0\",\n");
    _writef(fd, "\"exportdate\":\"%d-%02d-%02dT%02d:%02d:%02dZ\",\n", y, m, d, h, mi, s);
    _writef(fd, "\"creator\":\"g7ctrl http://www.sourceforge.com/p/g7ctrl\",\n");
    _writef(fd, "\"bbox\": { \"minlat\" : %s, \"minlon\" : %s, \"maxlat\" : %s, \"maxlon\" : %s },\n", minlat, minlon, maxlat, maxlon);
    _writef(fd, "\"positions\" : [");

    for (size_t i = 0; i < resSetLength; i++) {
        _writef(fd, "["
                "\"%s\","
                "%s,"
                "%s,"
                "%s,"
                "\"%s\","
                "%s,"
                "%s,"
                "%s,"
                "%s,"
                "%s,"
                "%s]",
                g7loc_list[i].date,
                g7loc_list[i].deviceid,
                g7loc_list[i].lat,
                g7loc_list[i].lon,
                g7loc_list[i].approxaddr,
                g7loc_list[i].altitude,
                g7loc_list[i].speed,
                g7loc_list[i].heading,
                g7loc_list[i].satellite,
                g7loc_list[i].event,
                g7loc_list[i].voltage);
        if (i < resSetLength - 1)
            _writef(fd, ",\n");
    }
    _writef(fd, "]}\n");
    close(fd);
    return 0;
}

/**
 * Mail exported and compressed DB in the specified format
 * @param sockd
 * @param exportFormat
 * @return  0 on success, -1 on failure
 */
int
mail_as_attachment(struct client_info *cli_info, char *exportFormat) {
    
    const int sockd = cli_info->cli_socket;
    char tmpExportFile[256];
    snprintf(tmpExportFile, sizeof (tmpExportFile), "/tmp/%s.", EXPORTDB_BASE_NAME);
    strncat(tmpExportFile, exportFormat, sizeof(tmpExportFile)-1-strlen(tmpExportFile));

    char format[6];
    strncpy(format, exportFormat, 5);
    format[5] = '\0';

    char *fldVal[] = {
        "", /* entire command */
        "", /* db mail */
        "", /* devid */
        "",
        "", /* eventid */
        "",
        "", "", "", /* from */
        "", "", "",
        "", "", "", /* to */
        "", "", "",
        format, /* format */
        "",
        tmpExportFile /* filename */
    };

    exportdb_to_external_format(cli_info, 21, fldVal);

    // Double check that the file has been exported
    if (-1 == access(tmpExportFile, R_OK)) {
        logmsg(LOG_ERR, "There is no exported DB file at %s !!", tmpExportFile);
        return -1;
    } 

    // Compress the file and send it on its merry way
    char cmdBuff[1024];
    static char *bindirs[] = {"/usr/bin","/bin","/usr/local/bin"};
    const size_t MAXBINDIRS = sizeof(bindirs)/sizeof(char *);
    size_t cnt=0;
    int rc = 0;
    errno = 0;
    do {
        snprintf(cmdBuff, sizeof (cmdBuff), "%s/%s -f %s > /dev/null 2>&1", bindirs[cnt],attachment_compression, tmpExportFile);
        
        // Regarding the return code from system. The man page states that
        // The value returned is -1 on error (e.g., fork(2) failed), and the return status of the command  otherwise.   
        // This  latter return status is in the format specified in wait(2). Thus, the exit code of the command will be 
        // WEXITSTATUS(status).  In case /bin/sh could not be executed, the exit status will be that of a command that does exit(127).
        rc = system(cmdBuff);
        
        logmsg(LOG_DEBUG, "File compression: [rc=%d, errno=%d] trying to exec \"%s\"", WEXITSTATUS(rc), errno, cmdBuff);
        cnt++;
    } while(cnt < MAXBINDIRS && WEXITSTATUS(rc)==127);
    
    if( WEXITSTATUS(rc) || errno ) {
        logmsg(LOG_ERR, "Failed to compress exported DB using \"%s\" ( %d : %s )", attachment_compression, errno, strerror(errno));
        return -1;
    }
    
    
    if (*attachment_compression == 'g')
        xstrlcat(tmpExportFile, ".gz",sizeof(tmpExportFile));
    else if (*attachment_compression == 'x')
        xstrlcat(tmpExportFile, ".xz",sizeof(tmpExportFile));
    else if (*attachment_compression == 'b')
        xstrlcat(tmpExportFile, ".bz2",sizeof(tmpExportFile));
    else {
        logmsg(LOG_ERR, "Unknown compression method \"%s\"", attachment_compression);
        return -1;
    }

    // Double check that the file has been compressed
    if (-1 == access(tmpExportFile, R_OK)) {
        logmsg(LOG_ERR, "There is no compressed exported DB file at \"%s\"", tmpExportFile);
        return -1;
    } 

    char to[128];
    char subject[128];

    // Setup key replacements
    dict_t rkeys = new_dict();
    char valBuff[256];

    // Get full current time to include in mail
    time_t now = time(NULL);
    ctime_r(&now, valBuff);
    valBuff[strnlen(valBuff, sizeof (valBuff)) - 1] = 0; // Remove trailing newline
    add_dict(rkeys, "SERVERTIME", valBuff);

    // Include the server name in the mail
    gethostname(valBuff, sizeof (valBuff));
    valBuff[sizeof (valBuff) - 1] = '\0';
    add_dict(rkeys, "SERVERNAME", valBuff);
    add_dict(rkeys, "DAEMONVERSION", PACKAGE_VERSION);
    add_dict(rkeys, "FORMAT", exportFormat);

    // Add information on disk usage
    char ds_fs[64], ds_size[64], ds_avail[64], ds_used[64];
    int ds_use=0;
    if (0 == get_diskspace(data_dir, ds_fs, ds_size, ds_used, ds_avail, &ds_use)) {
        add_dict(rkeys, "DISK_SIZE", ds_size);
        add_dict(rkeys, "DISK_USED", ds_used);
        snprintf(valBuff, sizeof (valBuff), "%d", ds_use);
        add_dict(rkeys, "DISK_PERCENT_USED", valBuff);
    }

    char *templatename = "mail_with_export_attachment";
    xstrlcpy(to, send_mailaddress,sizeof(to));
    snprintf(subject, sizeof(subject), SUBJECT_EXPORTEDDB, mail_subject_prefix);

    rc = send_mail_template(subject, daemon_email_from, to, templatename, rkeys, tmpExportFile, 0, NULL);

    if (-1 == rc) {
        logmsg(LOG_ERR, "Cannot send mail with compressed GPX file ( %d : %s )", errno, strerror(errno));
        free_dict(rkeys);
        return -1;
    }

    if (-1 == unlink(tmpExportFile)) {
        logmsg(LOG_ERR, "Failed to removed temporary export file \"%s\"", tmpExportFile);
        free_dict(rkeys);
        return -1;
    } else {
        logmsg(LOG_DEBUG, "Removed temporary export file \"%s\"", tmpExportFile);
    }

    _writef(sockd, "Mail with attachment sent to \"%s\"", to);
    free_dict(rkeys);
    return 0;
}

/**
 * Command to mail a compressed attachment of the GPX export file to
 * the predefined mail address
 * @param sockd
 * @return  0 on success, -1 on failure
 *
 * FIXME: Add proper argument to make it possible to send to chosen address
 * and not just the predefined address.
 */
int
mail_gpx_attachment(struct client_info *cli_info) {
    const int sockd = cli_info->cli_socket;
    int rc = mail_as_attachment(cli_info, "gpx");
    if( rc ) {
        _writef(sockd, "[ERR] Failed to send compressed DB export.");
    }
    return rc;
}

/**
 * Command to mail a compressed attachment of the CSV export file to
 * the predefined mail address
 * @param sockd
 * @return  0 on success, -1 on failure
 *
 * FIXME: Add proper argument to make it possible to send to chosen address
 * and not just the predefined address.
 */
int
mail_csv_attachment(struct client_info *cli_info) {
    const int sockd = cli_info->cli_socket;
    int rc = mail_as_attachment(cli_info, "csv");
    if( rc ) {
        _writef(sockd, "[ERR] Failed to send compressed DB export.");
    }    
    return rc;
}

/**
 * Internal dispatcher function that is initially called for all exports.
 * Depending on the actual format requested it will then call the
 * appropriate format exporter function.
 * @param sockd Client socket to communicate on
 * @param nf Number of fields in the command
 * @param fields Each argument in the user given command
 * @return 0 on success, -1 on failure
 */
int
exportdb_to_external_format(struct client_info *cli_info, ssize_t nf, char **fields) {

    const int sockd = cli_info->cli_socket;
    sqlite3 *sqlDB;
    if (0 == db_setup(&sqlDB)) {

        char filename[256], from[16], to[16], format[8];
        char deviceid[16], eventid[8];

        // Default values
        snprintf(filename, sizeof (filename), "%s/%s", db_dir, EXPORTDB_BASE_NAME);
        xstrlcpy(format, "csv", sizeof(format));
        *deviceid = '\0'; // Match all devices
        *eventid = '\0'; // Match all events
        *from = '\0';
        *to = '\0';
        if (nf >= 8) {
            snprintf(eventid, sizeof (eventid), "%s", fields[4]);
            snprintf(deviceid, sizeof (deviceid), "%s", fields[2]);
            snprintf(from, sizeof (from), "%s%s%s000000", fields[6], fields[7], fields[8]);
            if (nf >= 15) {
                snprintf(from, sizeof (from), "%s%s%s%s%s00", fields[6], fields[7], fields[8],
                        fields[10][0] == '\0' ? "00" : fields[10],
                        fields[11][0] == '\0' ? "00" : fields[11]);
                if (nf >= 18) {
                    snprintf(to, sizeof (to), "%s%s%s%s%s59", fields[12], fields[13], fields[14],
                            fields[16][0] == '\0' ? "23" : fields[16],
                            fields[17][0] == '\0' ? "59" : fields[17]);
                } else {
                    snprintf(to, sizeof (to), "%s%s%s235959", fields[12], fields[13], fields[14]);
                }
                snprintf(format, sizeof (format), "%s", fields[18]);
                if (nf >= 21) {
                    snprintf(filename, sizeof (filename), "%s", fields[20]);
                }
            }
            // In case user didn't supply any date then the from and to will only contain
            // the time strings added above which of course are invalid so we replace them
            // with empty strings as they should be. We could add several conditions above but it's
            // cleaner just to check at the end like this.
            if (6 == strlen(from)) *from = '\0';
            if (6 == strlen(to)) *to = '\0';
        }

        logmsg(LOG_DEBUG, "Exporting DB to \"%s\" (using %s schema)", filename, format);
        int rc = export_to_internal_set(sqlDB, from, to, deviceid, eventid);
        db_close(sqlDB);
        if (rc < 0) {
            switch (rc) {
                case -2:
                    _writef(sockd, "\"From Date\" can not be larger than \"To Date\"\n");
                    break;
                default:
                    _writef(sockd, "Problem reading DB\n");
                    logmsg(LOG_ERR, "Export to internal set failed. Cannot ecport dataset.");
                    break;
            }
            free_internal_set();
            return -1;
        }

        _Bool hasExt = 0 == xstrfext(filename, NULL);

        // Dispatch to correct external format
        if (0 == strcmp(format, "gpx")) {
            if (!hasExt)
                xstrlcat(filename, ".gpx",sizeof(filename));
            rc = export_to_gpx(filename);
        } else if (0 == xstricmp(format, "csv")) {
            if (!hasExt)
                xstrlcat(filename, ".csv",sizeof(filename));
            rc = export_to_csv(filename);
        } else if (0 == xstricmp(format, "kml")) {
            if (!hasExt)
                xstrlcat(filename, ".kml",sizeof(filename));
            rc = export_to_kml(filename);
        } else if (0 == xstricmp(format, "xml")) {
            if (!hasExt)
                xstrlcat(filename, ".xml",sizeof(filename));
            rc = export_to_prop_xml(filename);
        } else if (0 == xstricmp(format, "json")) {
            if (!hasExt)
                xstrlcat(filename, ".json",sizeof(filename));
            rc = export_to_json(filename);
        } else {
            _writef(sockd, "Unknown export format. Must be one of (GPX/KML/XML/CSV/JSON)\n\n");
            free_internal_set();
            return -1;
        }

        if (rc < 0) {
            _writef(sockd, "Failed to export in %s format to \"%s\"\n", format, filename);
            logmsg(LOG_ERR, "Cannot export to %s format: \"%s\" ( %d : %s )", format, filename, errno, strerror(errno));
            rc = -1;
        } else {
            _writef(sockd, "Exported %zd records in %s format to \"%s\"\n", resSetLength, format, filename);
            logmsg(LOG_INFO, "Exported %zd records in %s format to \"%s\"", resSetLength, format, filename);
            rc = 0;
        }
        free_internal_set();
        return rc;
    } else {
        logmsg(LOG_ERR, "Cannot connect to DB");
        _writef(sockd, "Cannot connect to DB\n\n");
        return -1;
    }
}


/*
 * EOF
 */
