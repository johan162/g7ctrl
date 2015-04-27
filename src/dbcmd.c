/* =========================================================================
 * File:        DBCMD.C
 * Description: Commands to extract data from the database of received
 *              events.
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
#include "xstr.h"
#include "dbcmd.h"
#include "mailutil.h"
#include "gpsdist.h"
#include "nicks.h"
#include "export.h"
#include "geoloc.h"
#include "unicode_tbl.h"


char *db_filename = DEFAULT_TRACKER_DB;

/**
 * Number of callbacks. Must be initialized by the function that
 * initiates a callback setup.
 */
static int num_cb = 0;
static int num_cb_cols = 0;
static int max_num_cb = 0;

// Silent gcc about unused "arg"in the callbacks
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/**
 * Result callback from sqlite3_exec(). This callback is called for each row
 * in the result set after a call to get last stored location
 *
 * @param res       INterpret as array of string pointers. Used to store the
 *                  read location to communicates to initiating function
 * @param nColumns  Number of columns in row
 * @param values    Column values
 * @param colNames  Name of each column
 * @return 0 to continue to the next row, -1 to abort
 */
static int
sql_reslist_callback(void *res, int nColumns, char **values, char **colNames) {
    if (0 == nColumns || num_cb >= max_num_cb || nColumns != num_cb_cols)
        return 1;

    for (int i = 0; i < nColumns; i++) {
        ((char **) res)[num_cb*nColumns+i] = strdup(values[i]);
        //logmsg(LOG_DEBUG,"sqres_lastloc_callback(): Column %03d: %s = %s", i, colNames[i], values[i] ? values[i] : "NULL");
    }
    ++num_cb;
    return 0;
}

/**
 * Check DB Version callback sqlite3_exec()
 * @param currentDBVersion
 * @param nColumns
 * @param values
 * @param colNames
 * @return
 */
static int
_chk_db_version_cb(void *currentDBVersion, int nColumns, char **values, char **colNames) {
    if (0 == nColumns)
        return 1;
    if (0 == strcmp(colNames[0], "fld_dbversion")) {
        *((int *) currentDBVersion) = xatoi(values[0]);
    } else {
        return 1;
    }
    return 0;
}

/**
 * Check DB size callback for sqlite3_exec()
 * @param numLocRows
 * @param nColumns
 * @param values
 * @param colNames
 * @return
 */
static int
_chk_db_size_cb(void *numLocRows, int nColumns, char **values, char **colNames) {
    if (0 == nColumns)
        return 1;

    *((int *) numLocRows) = xatoi(values[0]);
    return 0;
}

#pragma GCC diagnostic pop

/**
 * Check that the opened database is the current version
 * @param sqlDB
 * @return 0 on success, -1 on version mismatch
 */
static int
_chk_db_version(sqlite3 *sqlDB) {
    static char *q = _SQL_SELECT_DBVERSION;
    char *errMsg;
    int currentDBVersion = -1;
    int rc = sqlite3_exec(sqlDB, q, _chk_db_version_cb, (void *) &currentDBVersion, &errMsg);
    if (rc != SQLITE_OK) {
        logmsg(LOG_CRIT, "Can not read DB version ( \"%s\" )", errMsg);
        sqlite3_free(errMsg);
        sqlite3_close(sqlDB);
        return -1;
    } else {
        if (currentDBVersion == DB_VERSION) {
            return 0;
        } else {
            logmsg(LOG_ERR, "Database version mismatch. Please delete old DB");
            return -1;
        }
    }
}

/**
 * Return the total number of rows in location table
 * @param[out] size Size of table
 * @return 0 on success, -1 on failure
 */
int
_db_get_size(int *size) {
    sqlite3 *sqlDB;
    if (0 == db_setup(&sqlDB)) {
        static char *q = _SQL_SELECT_COUNT;
        char *errMsg;
        int rc = sqlite3_exec(sqlDB, q, _chk_db_size_cb, (void *) size, &errMsg);
        if (rc != SQLITE_OK) {
            logmsg(LOG_CRIT, "Can not read DB size ( \"%s\" )", errMsg);
            sqlite3_free(errMsg);
            sqlite3_close(sqlDB);
            return -1;
        }        
        sqlite3_close(sqlDB);
        return 0;
    } 
    return -1;
}


/**
 * Return number of events in total stored in the DB. Internally
 * uses _db_get_numloc()
 * @param sockd Client socket to communicate on
 * @return 0 on success, -1 on failure
 */
int
db_get_numevents(struct client_info *cli_info) {
    const int sockd = cli_info->cli_socket;
    int num;
    if (_db_get_size(&num)) {
        _writef(sockd, "[ERR] Can not read number of events in DB.");
        return -1;
    }
    _writef(sockd, "%d", num);
    return 0;
}

/**
 * Opens the predefined Database where the location data is stored. The location
 * of the database file is defined in the configuration file.
 * After the call the DB mutex will be locked and requires a corresponding call
 * to db_close to unlock. This is to prevent multiple clients accessing the
 * DB simultaneous which sqlite3 does not allow.
 * The handle set in the argument must be used with all subsequent DB routine
 * calls.
 * Please note that it is vital that the db_close() is called in pair with this
 * function since a deadlock will otherwise occur.
 * @param sqlDB handle for DB (out)
 * @return 0 on success, -1 on failure
 */
int
db_setup(sqlite3 **sqlDB) {
    if (0 == strlen(db_dir)) {
        logmsg(LOG_CRIT, "No DB directory specified");
        return -1;
    }

    // Check if old DB file exists, otherwise create it
    char tracker_db_file[1024];
    snprintf(tracker_db_file, sizeof (tracker_db_file), "%s/%s", db_dir, db_filename);
    struct stat statbuf;
    unsigned filesize_kB = 0;
    int newdb;
    if ((newdb = stat(tracker_db_file, &statbuf))) {
        logmsg(LOG_INFO, "No previous database file exists. Will create a new DB at %s", tracker_db_file);
    } else {
        filesize_kB = statbuf.st_size / (1024);
    }
    sqlite3 *db;
    int rc = sqlite3_open(tracker_db_file, &db);
    *sqlDB = db;
    if (rc) {
        logmsg(LOG_CRIT, "Can not open database file %s", tracker_db_file);
        sqlite3_close(*sqlDB);
        return -1;
    } else {
        logmsg(LOG_DEBUG, "Opened DB=\"%s\" (%dkB)", tracker_db_file, filesize_kB);

        // If this is a new DB we need to create the schemas
        if (newdb) {
            char *errMsg = 0;
            logmsg(LOG_DEBUG, "SCHEMA : %s %s %s", DB_SCHEMA_INFO, DB_SCHEMA_LOC, DB_SCHEMA_NICK);
            rc = sqlite3_exec(*sqlDB, DB_SCHEMA_INFO DB_SCHEMA_LOC DB_SCHEMA_NICK, NULL, 0, &errMsg);
            if (rc != SQLITE_OK) {
                logmsg(LOG_CRIT, "Can not create DB Schema in new DB ( \"%s\" )", errMsg);
                sqlite3_free(errMsg);
                sqlite3_close(*sqlDB);
                return -1;
            } else {
                char currTime[32];
                if (-1 == get_datetime(currTime, 0)) {
                    logmsg(LOG_CRIT, "Cannot determine local time");
                    sqlite3_close(*sqlDB);
                    return -1;
                }
                char q[512];
                snprintf(q, sizeof (q), _SQL_INFO_INSERT, DB_TABLE_INFO, currTime, DB_VERSION);
                rc = sqlite3_exec(*sqlDB, q, NULL, 0, &errMsg);
                if (rc) {
                    logmsg(LOG_CRIT, "Can not initialize tbl_info in new DB ( \"%s\" )", errMsg);
                    sqlite3_free(errMsg);
                    sqlite3_close(*sqlDB);
                    return -1;
                }

            }

        }
        if (_chk_db_version(*sqlDB)) {
            sqlite3_close(*sqlDB);
            return -1;
        }
    }

    // Some tuning to get maximum speed in sqlite3 by using a bit more memory
    char *errMsg;
    if (SQLITE_OK !=
            sqlite3_exec(*sqlDB, _SQL_PRAGMA, NULL, NULL, &errMsg)) {
        logmsg(LOG_ERR, "Cannot set DB pragma (%s) ", errMsg);
        sqlite3_free(errMsg);
        return -1;
    }

    return 0;
}

/**
 * Close the open DB and unlock the DB mutex. NOTE: This must be called at the
 * end of each DB transaction!
 * @param sqlDB handle for DB
 */
void
db_close(sqlite3 *sqlDB) {
    sqlite3_close(sqlDB);
    logmsg(LOG_DEBUG, "Closed DB.");
    // pthread_mutex_unlock(&db_mutex);
}

/**
 * Execute a SQL statement where the result of the statement will not be used.
 * @param sql SQL statment
 * @param[out] changes Number of rows changed
 * @return 0 on success, -1 on failure
 */
int
db_exec_sql(char *sql, int *changes) {
    sqlite3 *sqlDB;
    if (0 == db_setup(&sqlDB)) {
        char *errorMessage;
        logmsg(LOG_DEBUG, "Executing SQL: \"%s\"", sql);
        int rc = sqlite3_exec(sqlDB, sql, NULL, NULL, &errorMessage);
        if (rc != SQLITE_OK) {
            logmsg(LOG_ERR, "SQL Error [%s] for sql=\"%s\"", errorMessage, sql);
            sqlite3_free(errorMessage);
            sqlite3_close(sqlDB);
            return -1;
        }
        *changes = sqlite3_changes(sqlDB);
        sqlite3_close(sqlDB);
    } else {
        return -1;
    }
    return 0;
}

/**
 * Handles storing the received location update from the device in our
 * database. The received data can be both a single update or a batch of
 * updates either from device memory or directly.
 * The callback function can be used to perform any activity needed just
 * after each row has been updated in the DB. The callback function will
 * receive all fields just stored as the first argument.
 * @param recvBuff Received data from the device. This can be one or
 *                 multiple location updates separated with "\r\n"
 * @param cb Callback function (int,struct splitfields *,void *)
 * @param cb_option The third argument to the callback
 * @return number of written positions on success (>0), -1 on failure
 */
int
db_store_locations(int sockd, unsigned num_loc, const char *recvBuff, void (*cb)(struct splitfields *, void *), void *cb_option) {
       
    sqlite3 *sqlDB;
    unsigned pcnt = 0;
    int cnt=0;

    if (0 == db_setup(&sqlDB)) {
        // Now the location update gets a bit convoluted. The string we received
        // from the tracker is either of the form
        // "[(loc-update-data\r\n)+]"
        // or
        // "(loc-update-data\r\n)+"
        //
        // we can receive an arbitrary number of updates in the same packet
        //
        // Example data received is:[3000000001,20131211002222,17.959445,59.366545,0,0,0,0,2,3.88V,0\r\n
        //                           3000000001,20131211002422,17.959445,59.366545,0,0,0,0,2,3.88V,0]
        //
        char *sqlStmt =
                "insert into tbl_track (fld_timestamp,fld_deviceid,fld_datetime,fld_lon,fld_lat,fld_approxaddr,fld_speed,"
                "fld_heading,fld_altitude,fld_satellite,fld_event,fld_voltage,fld_detachstat) "
                "values (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13)";

        sqlite3_stmt* stmt;
        if (SQLITE_OK != sqlite3_prepare_v2(sqlDB, sqlStmt, strlen(sqlStmt), &stmt, NULL)) {
            logmsg(LOG_ERR, "Cannot compile SQL : \"%s\"", sqlite3_errmsg(sqlDB));
            db_close(sqlDB);
            return -1;
        }

        _Bool expectBracket = FALSE;
        const char *bptr = recvBuff;
        if ('[' == *recvBuff) {
            expectBracket = TRUE;
            bptr++;
            logmsg(LOG_INFO, "Received location update from stale positions which previously failed to be sent back");
        }

        char locBuff[512] = {'\0'};
        char *lptr = locBuff;

        char* errorMsg;
        if (SQLITE_OK != sqlite3_exec(sqlDB, "BEGIN TRANSACTION", NULL, NULL, &errorMsg)) {
            logmsg(LOG_ERR, "Cannot start DB transaction (%s)", errorMsg);
            sqlite3_finalize(stmt);
            sqlite3_free(errorMsg);
            db_close(sqlDB);
            return -1;
        }

        logmsg(LOG_DEBUG, "Storing location(s) in DB : db_store_locations()");
        
        const unsigned percent10 = num_loc/10;
        unsigned percent_progress = 0;
        
        if( num_loc > 100 ) {
            _writef(sockd,"[0%%].");
        }
        
        do {

            size_t numFields = 1;
            _Bool finished = 0;
            do {
                finished = '\0' == *bptr ||
                        ('\r' == *bptr && '\n' == *(bptr + 1)) ||
                        ']' == *bptr;
                if (!finished) {
                    *lptr++ = *bptr++;
                    if (LOC_DELIM == *bptr) numFields++;
                }

            } while (!finished && numFields < 12);

            *lptr = '\0';
            lptr = locBuff; // Reset running pointer for potential next row
            if ('\r' == *bptr && '\n' == *(bptr + 1)) {
                bptr += 2;
            }

            if (numFields >= 12) {
                logmsg(LOG_ERR, "Unknown format in received location update: \"%s\"", locBuff);
                sqlite3_finalize(stmt);
                db_close(sqlDB);
                return -1;
            }

            size_t len = strlen(locBuff);
            if (len < 50) {
                logmsg(LOG_ERR, "Received %zd chars. Location data must be >= 50 chars.", len);
                logmsg(LOG_ERR, "  \"%s\"", locBuff);
                sqlite3_finalize(stmt);
                db_close(sqlDB);
                return -1;
            }

            // Split the received data in the different fields and store in the DB
            struct splitfields flds;
            if (-1 == xstrsplitfields(locBuff, LOC_DELIM, &flds)) {
                logmsg(LOG_ERR, "Error running xstrsplitfields() on \"%s\"", locBuff);
                sqlite3_finalize(stmt);
                db_close(sqlDB);
                return -1;
            }

            if (11 != flds.nf) {
                logmsg(LOG_ERR, "Expected 11 field but found %zd in \"%s\"", flds.nf, locBuff);
                sqlite3_finalize(stmt);
                db_close(sqlDB);
                return -1;
            }

            // A real GM7 location string has the device id as the first field and deviceid
            // always start with a '3' digit in the first position. So we test for that to
            // verify that the data is proper.
            if (strlen(flds.fld[GM7_LOC_DEVID]) != 10 || '3' != *(flds.fld[GM7_LOC_DEVID])) {
                logmsg(LOG_ERR, "Received data is not a valid GM7 location update");
                sqlite3_finalize(stmt);
                db_close(sqlDB);
                return -1;
            }

            // Remove the ending 'V' in the battery voltage
            const size_t vlen = strlen(flds.fld[GM7_LOC_VOLT]);
            if (vlen > 0) {
                flds.fld[GM7_LOC_VOLT][ vlen - 1 ] = '\0';
            }

            // We now have one row of location data that we can send to the
            // the database for storage
            // Example data: 3000000001,20131211002222,17.959445,59.366545,0,0,0,0,2,3.88V,0\r\n
            sqlite3_bind_int64(stmt, 1, time(NULL));
            sqlite3_bind_int64(stmt, 2, xatol(flds.fld[GM7_LOC_DEVID]));
            sqlite3_bind_int64(stmt, 3, xatol(flds.fld[GM7_LOC_DATE]));
            sqlite3_bind_text(stmt, 4, flds.fld[GM7_LOC_LON], -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, flds.fld[GM7_LOC_LAT], -1, SQLITE_TRANSIENT);

            // If user has enabled reverse address lookup we try to find the approx address and store it in DB as well
            char address[512];
            *address='\0';
            
            if (use_address_lookup) {
                (void)get_address_from_latlon(flds.fld[GM7_LOC_LAT], flds.fld[GM7_LOC_LON], address, sizeof (address));
            } else {
                logmsg(LOG_DEBUG, "Geolocation lookup disabled. Setting location to \"---\"");
                xstrlcpy(address, "---", sizeof (address));
            }
            
            sqlite3_bind_text(stmt, 6, address, -1, SQLITE_TRANSIENT);

            sqlite3_bind_int(stmt, 7, xatol(flds.fld[GM7_LOC_SPEED]));
            sqlite3_bind_int(stmt, 8, xatol(flds.fld[GM7_LOC_HEADING]));
            sqlite3_bind_int(stmt, 9, xatol(flds.fld[GM7_LOC_ALT]));
            sqlite3_bind_int(stmt, 10, xatol(flds.fld[GM7_LOC_SAT]));
            sqlite3_bind_int(stmt, 11, xatol(flds.fld[GM7_LOC_EVENTID]));
            sqlite3_bind_text(stmt, 12, flds.fld[GM7_LOC_VOLT], -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 13, xatol(flds.fld[GM7_LOC_DETACH]));

            int rc = sqlite3_step(stmt);

            if (rc == SQLITE_LOCKED) {
                // If the table is locked this is due to another client just updating the table
                // Hold back a random time (1-5s) and try up to three times before really giving up.

                srand((unsigned) time(NULL));
                size_t wait = rand() % 5 + 1;

                logmsg(LOG_DEBUG, "DB is locked waiting %zu s before trying again", wait);
                sleep(wait);

                rc = sqlite3_step(stmt);
                if (rc == SQLITE_LOCKED) {
                    wait = rand() % 7 + 1;
                    logmsg(LOG_DEBUG, "DB is still locked waiting %zu s before trying again", wait);
                    sleep(wait);
                    rc = sqlite3_step(stmt);
                }

                if (rc != SQLITE_DONE) {
                    logmsg(LOG_ERR, "sqlite3_step() : Failed. \"%s\"", sqlite3_errmsg(sqlDB));
                }

            }

            sqlite3_reset(stmt);

            if (NULL != cb) {
                cb(&flds, cb_option);
            }

            pcnt++;
            cnt++;
            // Only send back progress if there are more than 100 locations to update
            if(  num_loc > 100 && pcnt > percent10 ) {
                pcnt=0;
                percent_progress += 10;
                _writef(sockd,"[%u%%].",percent_progress);
            }

        } while (*bptr && ']' != *bptr);

        if( num_loc > 100 ) {
            _writef(sockd,"[100%%]\n");
        }
        
        if (SQLITE_OK != sqlite3_exec(sqlDB, "COMMIT TRANSACTION", NULL, NULL, &errorMsg)) {
            logmsg(LOG_ERR, "Cannot COMMIT TRANSACTION ( %s )", errorMsg);
            sqlite3_free(errorMsg);
            sqlite3_finalize(stmt);
            db_close(sqlDB);
            return -1;
        } else {
            sqlite3_finalize(stmt);
            db_close(sqlDB);
            logmsg(LOG_DEBUG, "Successfully updated DB with %03d records",cnt);
        }

        if (expectBracket && ']' != *bptr) {
            logmsg(LOG_ERR, "Was expecting ']' at end of location data from tracker");
        }

    } else {

        logmsg(LOG_CRIT, "Cannot open DB to write location update! ( %d : %s )", errno, strerror(errno));
        return -1;

    }

    return cnt;
}

/**
 * Empty the location table
 * @param sockd Client socket to write back information on
 * @return 0 on success, -1 on failure
 */
int
db_empty_loc(struct client_info *cli_info) {
    const int sockd = cli_info->cli_socket;
    sqlite3 *sqlDB;
    int rc = 0;
    if (0 == db_setup(&sqlDB)) {
        char q[512];
        char *errorMsg = NULL;
        snprintf(q, sizeof (q), "DROP TABLE %s; VACUUM; %s", DB_TABLE_LOC, DB_SCHEMA_LOC);
        int ret = sqlite3_exec(sqlDB, q, NULL, NULL, &errorMsg);
        if (ret != SQLITE_OK || NULL != errorMsg) {
            logmsg(LOG_ERR, "SQLITE3 error when dropping table : %s", errorMsg);
            sqlite3_free(errorMsg);
            rc = -1;
        } else {
            _writef(sockd, "ALL stored locations deleted.");
        }
    } else {
        rc = -1;
    }
    db_close(sqlDB);
    return rc;
}

/**
 * Utility function to build the while part of a SQL statement from one
 * or more conditions. This function will be called for each condition
 * we want to add. It is the calling functions responsibility that 'w'
 * is large enough to hold the constructed while
 * @param w   Partial or empty while statement
 * @param col Column to operate on
 * @param op  Operator (e.g. "=", "like" etc.)
 * @param val Comparison value
 */
void
add_wcond(char *w, char *col, char *op, char *val) {
    xstrtrim(val);
    if (*val) {
        if (*w) {
            char buf[256];
            snprintf(buf, sizeof (buf) - 1, " AND %s %s %s ", col, op, val);
            strcat(w, buf);
        } else {
            sprintf(w, "WHERE %s %s %s ", col, op, val);
        }
    }
}

/**
 * Execute the distance calculating command. This will select the chosen
 * location points and read them into an internal memory structure. From these
 * point in memory the approximate traveled distance will be calculated.
 * @param sockd Client socket to communicate on
 * @param nf Number of fields in the command line parsed
 * @param fields Command line arguments
 * @return 0 on success, -1 on failure
 */
int
db_calc_distance(struct client_info *cli_info, ssize_t nf, char **fields) {

    const int sockd = cli_info->cli_socket;
    sqlite3 *sqlDB;
    int rc = 0;

    if (0 == db_setup(&sqlDB)) {

        char from[16], to[16], deviceid[16], eventid[8];

        // Default values
        *deviceid = '\0'; // Match all devices
        *eventid = '\0'; // Match all events
        *from = '\0';
        *to = '\0'; // Match all dates

        if (nf > 2) {
            snprintf(deviceid, sizeof (deviceid), "%s", fields[2]);
        }
        if (nf > 4) {
            snprintf(eventid, sizeof (eventid), "%s", fields[4]);
        }
        if (nf > 11) {
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
            }
        }

        // Get a memory copy of all the selected data so we can operate on it
        rc = export_to_internal_set(sqlDB, from, to, deviceid, eventid);
        db_close(sqlDB);

        if (0 == rc) {
            if (resSetLength > 1) {
                double dist = 0.0;
                double dist2 = 0.0;
                _writef(sockd, "Calculating approximate distance using %zd points.\n", resSetLength);
                struct g7loc_t *p1, *p2;
                for (size_t i = 0; i < resSetLength - 1; ++i) {
                    p1 = &g7loc_list[i];
                    p2 = &g7loc_list[i + 1];
                    const double lat1 = xatof(p1->lat);
                    const double lon1 = xatof(p1->lon);
                    const double lat2 = xatof(p2->lat);
                    const double lon2 = xatof(p2->lon);
                    dist += gpsdist_km(lat1, lon1, lat2, lon2);
                    dist2 += gpsdist_m(lat1, lon1, lat2, lon2);
                }
                if (dist > 1) {
                    _writef(sockd, "%.1f km (alt. %.1f m)", round(dist * 10) / 10.0, round(dist2 * 10) / 10.0);
                } else {
                    _writef(sockd, "%.0f m", round(dist * 1000));
                }
            } else {
                _writef(sockd, "[ERR] Selection must have at least two points.");
            }

        } else {
            if (-2 == rc) {
                _writef(sockd, "\"From Date\" can not be larger than \"To Date\"");
            } else {
                _writef(sockd, "[ERR] Problem reading DB");
                logmsg(LOG_ERR, "Export to internal set failed. Cannot calculate distance.");
            }
        }
        free_internal_set();
    } else {
        logmsg(LOG_ERR, "Cannot connect to DB");
        _writef(sockd, "[ERR] Cannot connect to DB");
        rc = -1;
    }

    return rc;
}

static int sort_order = SORT_ARRIVALTIME;

/**
 * Sort order for the DB tail and DB head.
 * If the sort order is == 0 the sort is based on date time based on device time
 * If the soer order is == 1 then the sort order is based on the time of arrival to the
 * server.
 * @param order Sort to be based on arrival time or device timestamp
 */
void
db_set_sortorder(enum sort_order_t order) {
    sort_order = order;
}

/**
 * Get a human string representation of the sort order., The return value
 * is a string pointer to a static internal string
 * @return Pointer to static string
 */
const char *
db_get_sortorder_string(void) {
    static char const  *sort_order_string[] = {"By device time","By arrival time"};
    return sort_order_string[sort_order];
}


/**
 * Internal helper. Store six char * values in buff[] it is the calling routines responsibility to free
 * the strings after usage.
 * @param buff Buffer to store result in
 * @return  0 on success, -1 on failure, -2 if DB empty
 */
int
_db_getloclist(char *buff[],unsigned maxrows,_Bool head) {
    sqlite3 *sqlDB;
    if (0 == db_setup(&sqlDB)) {
        char *errMsg;

        char q[256];
        snprintf(q,sizeof(q),"SELECT fld_datetime, fld_deviceid, fld_lat, fld_lon, fld_speed, fld_approxaddr FROM tbl_track ORDER BY %s %s LIMIT %u;",
                sort_order?"fld_timestamp":"fld_datetime",
                head?"DESC":"ASC",maxrows);
        num_cb = 0;
        num_cb_cols = 6;
        max_num_cb = maxrows;
        int rc = sqlite3_exec(sqlDB, q, sql_reslist_callback, buff, &errMsg);
        if (rc) {
            logmsg(LOG_DEBUG, "Can execute SQL [\"%s\"]", errMsg);
            db_close(sqlDB);
            sqlite3_free(errMsg);
            return -1;
        }
        rc = 0;
        logmsg(LOG_DEBUG,"_db_getloclist : num_cb=%d",num_cb);
        if (0 == num_cb) {
            // No callbacks so DB must be empty
            rc = -2;
        }
        db_close(sqlDB);
        return rc;
    } else {
        return -1;
    }
}

int
_db_getlastloc(char *buff[]) {
    return _db_getloclist(buff,1,TRUE);
}


/**
 * Command to get the last stored location (regardless of even and device) and
 * write it back to the client socket
 * @param cli_info Client context
 * @return 0 on success, -1 on failure
 */
int
db_lastloc(struct client_info *cli_info) {
   return db_loclist(cli_info, TRUE, 1);
}

/**
 * Command to get a list of locations stored in the database. The list is collected
 * either from the head or from the bottom in regards to the timestamp.
 * @param cli_info Client context
 * @param head Should the list be from the head or from the bottom
 * @param numrows Number of rows to read
 * @return 0 on success, -1 on failure
 */
int
db_loclist(struct client_info *cli_info, _Bool head, size_t numrows) {
    const int sockd = cli_info->cli_socket;
    const size_t cols = 6;

    if (numrows > 1000) {
        logmsg(LOG_ERR, "Too many rows in db_loclist()");
        _writef(sockd, "[ERR] Too many rows specified.\n");
        return -1;
    }

    char *res[cols * numrows];
    memset(res, 0, cols*numrows * sizeof(char *));

    // For head the list is in descending order (newest first)
    // For tail the list is in ascending order (oldest first)
    int rc = _db_getloclist(res, numrows, head);
    
    logmsg(LOG_DEBUG, "db_loclist() : _db_getloclist(), rc=%d", rc);
    
    char buff[32];
    int size;
    _db_get_size(&size);
    if (0 == rc) {

        // Construct the array of data fields                
        char *tdata[(numrows + 1)*(cols + 1)];
        memset(tdata, 0, sizeof (tdata));
        tdata[0] = strdup("#");
        tdata[1] = strdup("Date");
        tdata[2] = strdup("Dev.ID");
        tdata[3] = strdup("Lat");
        tdata[4] = strdup("Lon");
        tdata[5] = strdup("Speed");
        tdata[6] = strdup("Address");

        if (head) {
            for (int i = 0; i < num_cb; i++) {
                snprintf(buff, sizeof (buff), "%04d", size - i);
                tdata[(i + 1)*(cols + 1) + 0] = strdup(buff);
                splitdatetime(res[i * cols + 0], buff);
                tdata[(i + 1)*(cols + 1) + 1] = strdup(buff);
                for (size_t j = 2; j < cols + 1; ++j) {
                    tdata[(i + 1)*(cols + 1) + j] = strdup(res[i * cols + (j - 1)]);
                }
            }
        } else {
            for (int i = 0; i < num_cb; i++) {
                snprintf(buff, sizeof (buff), "%04d", num_cb - i);
                tdata[(i + 1)*(cols + 1) + 0] = strdup(buff);
                splitdatetime(res[(num_cb - i - 1) * cols + 0], buff);
                tdata[(i + 1)*(cols + 1) + 1] = strdup(buff);
                for (size_t j = 2; j < cols + 1; ++j) {
                    tdata[(i + 1)*(cols + 1) + j] = strdup(res[(num_cb - i - 1) * cols + (j - 1)]);
                }
            }
        }

        table_t *t = utable_create_set(num_cb + 1, cols + 1, tdata);
        utable_set_row_halign(t, 0, CENTERALIGN);                    
        if (cli_info->use_unicode_table) {
            utable_set_interior(t,TRUE,FALSE);
            utable_stroke(t, sockd, TSTYLE_DOUBLE_V2);
        } else {
            utable_stroke(t, sockd, TSTYLE_ASCII_V3);
        }
        
        
        utable_free(t);
        for (size_t i = 0; i < (numrows + 1)*(cols + 1); i++) {
            free(tdata[i]);
        }

        for (size_t i = 0; i < numrows * cols; ++i) {
            if (res[i])
                free(res[i]);
        }
    } else {
        _writef(sockd, "[ERR] Could not retrieve the last stored location.");
    }
    return rc;
}

/**
 * Print the latest positions in  a row
 * @param cli_info Client context
 * @return 0 on success, -1 on failure
 */
int
db_head(struct client_info *cli_info, unsigned size) {
   return db_loclist(cli_info, TRUE, size);
}

/**
 * Print the oldest positions in  a row
 * @param cli_info Client context
 * @return 0 on success, -1 on failure
 */
int
db_tail(struct client_info *cli_info, unsigned size) {
    return db_loclist(cli_info, FALSE, size);
}

/**
 * Mail the last stored location to the predefined mail address in the config
 * file.
 * @param sockd Client socket to write reply on
 * @return 0 on success, -1 on failure
 */
int
mail_lastloc(struct client_info *cli_info) {

    const int sockd = cli_info->cli_socket;
    const size_t MAX_KEYPAIRS = 50;

    // Setup key replacements
    struct keypairs *keys = new_keypairlist(MAX_KEYPAIRS);
    char valBuff[256];
    size_t keyIdx = 0;

    // Get full current time to include in mail
    time_t now = time(NULL);
    ctime_r(&now, valBuff);
    valBuff[strnlen(valBuff, sizeof (valBuff)) - 1] = 0; // Remove trailing newline
    add_keypair(keys, MAX_KEYPAIRS, "SERVERTIME", valBuff, &keyIdx);

    // Include the server name in the mail
    gethostname(valBuff, sizeof (valBuff));
    valBuff[sizeof (valBuff) - 1] = '\0';
    add_keypair(keys, MAX_KEYPAIRS, "SERVERNAME", valBuff, &keyIdx);

    add_keypair(keys, MAX_KEYPAIRS, "DAEMONVERSION", PACKAGE_VERSION, &keyIdx);
    add_keypair(keys, MAX_KEYPAIRS, "FORMAT", "GPX", &keyIdx);

    // Add information on disk usage
    char ds_fs[64], ds_size[64], ds_avail[64], ds_used[64];
    int ds_use;
    if (0 == get_diskspace(db_dir, ds_fs, ds_size, ds_used, ds_avail, &ds_use)) {
        add_keypair(keys, MAX_KEYPAIRS, "DISK_SIZE", ds_size, &keyIdx);
        add_keypair(keys, MAX_KEYPAIRS, "DISK_USED", ds_used, &keyIdx);
        snprintf(valBuff, sizeof (valBuff), "%d", ds_use);
        add_keypair(keys, MAX_KEYPAIRS, "DISK_PERCENT_USED", valBuff, &keyIdx);
    }


    char *res[6];
    int rc = _db_getlastloc(res);
    if (0 == rc) {
        char dbuff[32];
        char subjectbuff[128];
        // Translate dev id to nick name if it exists
        char nick[16];
        char nick_devid[32];
        
        char short_devid[8];        
        *short_devid = '\0';            
        if( use_short_devid ) {
            const size_t devid_len = strlen(res[1]);
            for( size_t i=0 ; i < 4; i++) {
                short_devid[i] = res[1][devid_len-4+i];
            }            
            short_devid[4] = '\0';            
            add_keypair(keys, MAX_KEYPAIRS, "DEVICEID", short_devid, &keyIdx);
        } else {
            add_keypair(keys, MAX_KEYPAIRS, "DEVICEID", res[1], &keyIdx);
        }        
        
        if (db_get_nick_from_devid(res[1], nick)) {
            // No nickname. Put device ID in its place
            xstrlcpy(nick, res[1], sizeof (nick) - 1);
            nick[sizeof (nick) - 1] = '\0';
            if( use_short_devid )
                xstrlcpy(nick_devid, short_devid, sizeof (nick_devid) - 1);
            else
                xstrlcpy(nick_devid, res[1], sizeof (nick_devid) - 1);
        } else {
            snprintf(nick_devid, sizeof (nick_devid), "%s (%s)", nick, 
                     use_short_devid ? short_devid : res[1]);
        }

        snprintf(subjectbuff, sizeof (subjectbuff), "%s[%s] - Last location in DB",mail_subject_prefix, res[1]);

        add_keypair(keys, MAX_KEYPAIRS, "NICK", nick, &keyIdx);
        add_keypair(keys, MAX_KEYPAIRS, "NICK_DEVID", nick_devid, &keyIdx);
        add_keypair(keys, MAX_KEYPAIRS, "DATETIME", splitdatetime(res[0], dbuff), &keyIdx);
        add_keypair(keys, MAX_KEYPAIRS, "DEVICEID", res[1], &keyIdx);
        add_keypair(keys, MAX_KEYPAIRS, "LAT", res[2], &keyIdx);
        add_keypair(keys, MAX_KEYPAIRS, "LON", res[3], &keyIdx);
        add_keypair(keys, MAX_KEYPAIRS, "SPEED", res[4], &keyIdx);
        add_keypair(keys, MAX_KEYPAIRS, "HEADING", res[5], &keyIdx);
        
        if (include_minimap) {
            
            logmsg(LOG_DEBUG,"Adding minimap to mail");

            char kval[32];
            snprintf(kval,sizeof(kval),"%d",minimap_width);
            add_keypair(keys, MAX_KEYPAIRS, "IMG_WIDTH", kval, &keyIdx);
            
            snprintf(kval,sizeof(kval),"%d",minimap_overview_zoom);
            add_keypair(keys, MAX_KEYPAIRS, "ZOOM_OVERVIEW", kval, &keyIdx);
            
            snprintf(kval,sizeof(kval),"%d",minimap_detailed_zoom);
            add_keypair(keys, MAX_KEYPAIRS, "ZOOM_DETAILED", kval, &keyIdx);

            char *overview_imgdata, *detailed_imgdata;
            const char *lat = res[2];
            const char *lon = res[3];
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
                                keys, keyIdx, MAX_KEYPAIRS, NULL, 0, NULL);
            } else {
                struct inlineimage_t *inlineimg_arr = calloc(2, sizeof (struct inlineimage_t));
                setup_inlineimg(&inlineimg_arr[0], overview_filename, overview_datasize, overview_imgdata);
                setup_inlineimg(&inlineimg_arr[1], detailed_filename, detailed_datasize, detailed_imgdata);

                rc = send_mail_template(subjectbuff, daemon_email_from, send_mailaddress,
                                    "mail_lastloc_img",
                                    keys, keyIdx, MAX_KEYPAIRS,
                                    NULL, 2, inlineimg_arr);

                free_inlineimg_array(inlineimg_arr, 2);
                free(inlineimg_arr);

            }

        } else {
        
            rc = send_mail_template(subjectbuff, daemon_email_from, send_mailaddress, "mail_lastloc", keys, 
                                    keyIdx, MAX_KEYPAIRS, NULL, 0, NULL);
        }
        
        for (size_t i = 0; i < 6; ++i) {
            free(res[i]);
        }
                 
        if (-1 == rc) {
            logmsg(LOG_ERR, "Cannot send mail with last location ( %d : %s )", errno, strerror(errno));
            _writef(sockd, "[ERR] Could not send mail.");
            free(keys);
            return -1;
        }

        _writef(sockd, "Mail with location sent to \"%s\"", send_mailaddress);
        rc = 0;
        
    } else {
        
        logmsg(LOG_ERR, "Cannot get last location from DB");
        _writef(sockd, "[ERR] Cannot retrieve location from DB.");
        rc = -1;
        
    }
    free(keys);
    return rc;
}

/**
 * Structure to store the help text for the DB commands
 */
struct db_help {
    char *cmd;
    char *desc;
    char *syn;
    char *arg;
    char *examples;
};

/**
 * List of help texts for DB commands
 */
struct db_help db_help_list [] = {
    {"deletelocations",
        "Delete all stored locations. This action CAN NOT BE UNDONE and hence the command name is long intentionally!",
        "",
        "",
        "\"db deletelocations\""},    
    {"dist",
        "Calculate the distance traveled using the Haversine and Vincenity methods",
        "[dev=nnn] [ev=nnn] [from to]",

        "dev=nnn            Optional. Filter on device id\n"
        "ev=nnn             Optional. Filter on eventid\n"
        "[from to]          Optional. Date interval to calculate distance in format \"yyyy-mm-dd\"\n",

        "\"db dist\" - Calculate the total distance using all locations in the database\n"
        "\"db dist 2013-05-01 2013-05-31\" - Calculate the total distance traveled in may\n"
        "\"db dist dev=3000000002\" - Calculate the total length for device=3000000002"},
    {"export",
        "Export entire or filtered portion of the existing DB to other formats",
        "[dev=nnn] [ev=nnn] [from to] (gpx|kml|xml|csv|json) [filename]",

        "dev=nnn            Optional. Filter on device id\n"
        "ev=nnn             Optional. Filter on eventid\n"
        "[from to]          Optional. Date interval to export in format \"yyyy-mm-dd\"\n"
        "(gpx|kml|xml|csv|json)  Mandatory. Which format used for exporting the DB\n"
        "                    gpx = Exchange format or GPS devices. WIdely used.\n"
        "                    kml = Format used to display tracks on Google earth and Google maps\n"
        "                    xml = Export all fields in DB in a XML format. See separate schema docs.\n"
        "                    csv = Export in comma separated format. Useful for importing into spreadsheets\n"
        "                    json= Export in JSON format\n"
        "filename           Optional. Filename of exported file.\n",

        "\"db export gpx\" - Export entire DB to in GPX format\n"
        "\"db export 2013-01-01 2013-01-31 csv /tmp/jan.xml\" - Export all january entries to \"jan.xml\" in CSV format\n"
        "\"db export dev=3000000002 kml /tmp/export.xml\" - Export all entries for device=3000000002 to \"export.xml\" in KML format"},        
    {"head",
        "Return the n newest locations in the database.",
        "[n]",
        "",
        "\"db head\" - Display the newest 10 locations\n"
        "\"db head 20\" - Display the newest 20 locations"},
    {"lastloc",
        "Display the latest recorded position in DB",
        "",
        "",
        "\"db lastloc\""},
    {"mailpos",
        "Mail the last recorded position to the predefined mail address in the configuration file",
        "",
        "",
        "\"db mailpos\""},
    {"mailcsv",
        "Mail a compressed version of a CSV export file of the entire DB as an attachment to the predefined mail address in the configuration file",
        "",
        "",
        "\"db mailcsv\""},
    {"mailgpx",
        "Mail a compressed version of a GPX export file of the entire DB as an attachment to the predefined mail address in the configuration file",
        "",
        "",
        "\"db mailgpx\""},
    {"size",
        "Return number of locations stored in the database",
        "",
        "",
        "\"db size\""},
    {"sort",
        "Set the sort order for the head and tail commands.",
        "[arrival|device]",
        "arival = Base the sorting on the order the locations was received by the server"
        "device = Base the sorting on the order the timestamp on the location updates. This is the default.",
        "\"db sort device\" - Display the oldest 10 locations\n"},        
    {"tail",
        "Return the n oldest locations in the database.",
        "[n]",
        "",
        "\"db tail\" - Display the oldest 10 locations\n"
        "\"db tail 20\" - Display the oldest 20 locations"}
};

/**
 * Number of DB commands
 */
const size_t num_db_len = sizeof(db_help_list) / sizeof(struct db_help);

/**
 * Print help for the db commands to the client socket
 * @param sockd Client socket to write reply on
 * @param fields The db command to give help for
 */
void
db_help(struct client_info *cli_info, char **fields) {
    const int sockd = cli_info->cli_socket;
    char defFileName[256];
    snprintf(defFileName, sizeof (defFileName), "%s/export.xml", db_dir);

    logmsg(LOG_DEBUG, "DB command: %s", fields[1]);
    for(size_t i=0; i<num_db_len; ++i ) {
        if (0 == strcmp(db_help_list[i].cmd, fields[1])) {
            _writef(sockd, "db %s - %s\n\n", fields[1], db_help_list[i].desc);
            _writef(sockd, "SYNOPSIS:\n");
            _writef(sockd, "db %s %s\n\n", fields[1], db_help_list[i].syn);
            if (strlen(db_help_list[i].arg) > 0) {
                _writef(sockd, "ARGUMENTS:\n");
                _writef(sockd, "%s\n\n", db_help_list[i].arg);
            }
            _writef(sockd, "EXAMPLES:\n");
            _writef(sockd, "%s\n\n", db_help_list[i].examples);
            return;
        }
    }
    _writef(sockd, "\"db %s\" is not a valid database command.\n"
            "Valid commands are:\n",fields[1]);
    
    for(size_t i=0; i<num_db_len; ++i ) {
        _writef(sockd,"db %s\n",db_help_list[i].cmd);
    }
}

/* EOF */
