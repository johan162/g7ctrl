/* =========================================================================
 * File:        nicks.c
 * Description: Handle device nick names
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
#include <string.h>
#include <sqlite3.h>
#include <sys/syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#include "config.h"
#include "g7config.h"
#include "g7ctrl.h"
#include "logger.h"
#include "utils.h"
#include "libxstr/xstr.h"
#include "dbcmd.h"
#include "nicks.h"

// Used as a cheap way to count the number of nicks found
static size_t nick_cb_cnt = 0;

// List of all nicks read from the DB
struct nick_res_t nick_res_set[MAX_NICK_RES_SET];

// Silent gcc about unused "arg"in the callbacks
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/**
 * Callback to read all callbacks from DB to internal memory structure
 * @param NotUsed
 * @param nColumns
 * @param vals
 * @param names
 * @return 0 on success, -1 result large than reserved space
 */
static int
update_nick_callback(void *NotUsed, int nColumns, char **vals, char **names) {
    struct nick_res_t *r;

    if (nick_cb_cnt >= MAX_NICK_RES_SET)
        return -1;

    r = &nick_res_set[nick_cb_cnt++];
    memset(r, 0, sizeof (struct nick_res_t));

    xmb_strncpy(r->nick, vals[0], 15);
    xmb_strncpy(r->devid, vals[1], 15);
    xmb_strncpy(r->imei, vals[2], 23);
    xmb_strncpy(r->sim, vals[3], 23);
    xmb_strncpy(r->phone, vals[4], 23);
    xmb_strncpy(r->fwver, vals[5], 47);
    xmb_strncpy(r->regdate, vals[6], 23);
    xmb_strncpy(r->upddate, vals[7], 23);

    //logmsg(LOG_DEBUG,"NICK CALLBACK: (%s,%s,%s,%s)",vals[0],vals[1],vals[2],vals[3]);

    return 0;
}
#pragma GCC diagnostic pop

/**
 * Update the nick name and device information. If any of the supplied arguments is NULL or empty string then the existing value
 * in the DB will be untouched.
 * @param nick Nickname
 * @param devid Device id
 * @param imei IMEI number of device
 * @param sim SIM number
 * @param phone Phone number of SIM card
 * @param fwver Firmware version of device
 * @return 0 on success, -1 on failure
 */
int
db_update_nick(const char *nick, const char *devid, const char *imei, const char *sim, const char *phone, const char *fwver) {
    sqlite3 *sqlDB;
    int rc = 0;
    if (0 == db_setup(&sqlDB)) {
        char q[512];
        char *errMsg;

        // First check if this device is already registered
        if (imei == NULL || *imei == '\0')
            return -1;

        nick_cb_cnt = 0;
        snprintf(q, sizeof (q), "SELECT * FROM %s WHERE fld_imei=%s;", DB_TABLE_NICK, imei);
        rc = sqlite3_exec(sqlDB, q, update_nick_callback, (void *) NULL, &errMsg);
        if (SQLITE_OK != rc) {
            logmsg(LOG_ERR, "Cannot SELECT on nick table (%s)", errMsg);
            sqlite3_free(errMsg);
            db_close(sqlDB);
            return -1;
        }
        char currTime[32];
        if (-1 == get_datetime(currTime, 0)) {
            logmsg(LOG_CRIT, "Cannot determine local time");
            db_close(sqlDB);
            return -1;
        }
        if (nick_cb_cnt > 0) {
            if (1 == nick_cb_cnt) {
                // Device exists
                struct nick_res_t *r;
                r = &nick_res_set[nick_cb_cnt - 1];
                logmsg(LOG_DEBUG, "fld_devid[%zd]=%s", nick_cb_cnt, r->devid);
                snprintf(q, sizeof (q), "UPDATE %s SET "
                        "fld_nick='%s',"
                        "fld_devid=%s,"
                        "fld_sim='%s',"
                        "fld_phone='%s',"
                        "fld_fwver='%s',"
                        "fld_upddate='%s' WHERE fld_imei=%s;", DB_TABLE_NICK,
                        *nick ? nick : r->nick,
                        *devid ? devid : r->devid,
                        *sim ? sim : r->sim,
                        *phone ? phone : r->phone,
                        *fwver ? fwver : r->fwver,
                        currTime,
                        imei);
                logmsg(LOG_DEBUG, "NICK UPDATE : %s", q);
                rc = sqlite3_exec(sqlDB, q, NULL, NULL, &errMsg);
                if (SQLITE_OK != rc) {
                    logmsg(LOG_ERR, "Cannot do NICK Update (%s)", errMsg);
                    sqlite3_free(errMsg);
                    db_close(sqlDB);
                    return -1;
                }

            } else {
                logmsg(LOG_CRIT, "Duplicate entries in NICK TABLE");
            }
        } else {
            // New device
            // Check that all values are supplied (except phone number)
            if (nick && devid && imei && sim && fwver &&
                    *nick && *devid && *imei && *sim && *fwver) {

                snprintf(q, sizeof (q), "INSERT INTO %s (fld_nick,fld_devid,fld_imei,fld_sim,fld_phone,fld_fwver,fld_regdate,fld_upddate) "
                        "VALUES ('%s',%s,%s,'%s','%s','%s','%s','%s');", DB_TABLE_NICK,
                        nick, devid, imei, sim, phone, fwver, currTime, currTime);
                logmsg(LOG_DEBUG, "NICK NEW : %s", q);
                rc = sqlite3_exec(sqlDB, q, NULL, NULL, &errMsg);
                if (SQLITE_OK != rc) {
                    logmsg(LOG_ERR, "Cannot do NICK new entry (%s)", errMsg);
                    sqlite3_free(errMsg);
                    db_close(sqlDB);
                    return -1;
                }

            } else {
                logmsg(LOG_ERR, "All values must be defined when creating a new nick");
                db_close(sqlDB);
                return -1;
            }
        }
        db_close(sqlDB);

    } else {
        rc = -1;
    }
    return rc;
}

/**
 * Return the nickname (if defined) for the supplied device id. It is the
 * calling routines responsibility to ensure that the nick name buffer can
 * hold 12 chars
 * @param devid Device id to find nick name for
 * @param[out] nick Nickname.
 * @return 0 on success, -1 no nick name defined
 */
int
db_get_nick_from_devid(const char *devid, char *nick) {
    sqlite3 *sqlDB;
    int rc = 0;
    *nick = '\0';
    if (0 == db_setup(&sqlDB)) {
        char q[512];
        char *errMsg;

        snprintf(q, sizeof (q), "SELECT * FROM %s WHERE fld_devid=%s", DB_TABLE_NICK, devid);
        nick_cb_cnt = 0;
        rc = sqlite3_exec(sqlDB, q, update_nick_callback, (void *) NULL, &errMsg);
        if (SQLITE_OK != rc) {
            logmsg(LOG_ERR, "Cannot SELECT on nick table (%s)", errMsg);
            sqlite3_free(errMsg);
            db_close(sqlDB);
            return -1;
        }
        if (1 == nick_cb_cnt) {
            xmb_strncpy(nick, nick_res_set[0].nick, 12);
        } else if (nick_cb_cnt > 1) {
            logmsg(LOG_ERR, "Duplicate entry in NICK TABLE for devid=%s", devid);
            rc = -1;
        } else {
            logmsg(LOG_INFO, "devid=%s does not have a nick name", devid);
            rc = -1;
        }
        db_close(sqlDB);
    } else {
        logmsg(LOG_ERR, "Cannot open DB to get nick name for devid=%s", devid);
        rc = -1;
    }

    return rc;
}

/**
 * Get the device id for the device with the specified nick-name. It is the
 * calling routines responsibility that the devid buffer is at least 11 characters
 * long to store the devid + terminating 0
 * @param nick Nick name
 * @param[out] devid Buffer to hold the device ID
 * @return 0 on success, -1 on failure
 */
int
db_get_devid_from_nick(const char *nick, char *devid) {
    sqlite3 *sqlDB;
    int rc = 0;
    *devid = '\0';
    if (0 == db_setup(&sqlDB)) {
        char q[512];
        char *errMsg;

        snprintf(q, sizeof (q), "SELECT * FROM %s WHERE fld_nick='%s'", DB_TABLE_NICK, nick);
        nick_cb_cnt = 0;
        rc = sqlite3_exec(sqlDB, q, update_nick_callback, (void *) NULL, &errMsg);
        if (SQLITE_OK != rc) {
            logmsg(LOG_ERR, "Cannot SELECT on nick table (%s)", errMsg);
            sqlite3_free(errMsg);
            db_close(sqlDB);
            return -1;
        }
        if (1 == nick_cb_cnt) {
            xmb_strncpy(devid, nick_res_set[0].devid, 10);
        } else if (nick_cb_cnt > 1) {
            logmsg(LOG_ERR, "Duplicate entry in NICK TABLE for nickname=%s", nick);
            rc = -1;
        } else {
            logmsg(LOG_ERR, "Nickname=%s does not exists", nick);
            rc = -1;
        }
        db_close(sqlDB);
    } else {
        rc = -1;
    }

    return rc;
}

/**
 * Delete the specified nick from DB
 * @param nick Nick name to delete
 * @return 0 on success, -1 on failure
 */
int
db_delete_nick(const char *nick) {
    sqlite3 *sqlDB;
    int rc = 0;
    if (0 == db_setup(&sqlDB)) {
        char q[512];
        char *errMsg;

        snprintf(q, sizeof (q), "DELETE FROM %s WHERE fld_nick='%s';", DB_TABLE_NICK, nick);
        rc = sqlite3_exec(sqlDB, q, update_nick_callback, (void *) NULL, &errMsg);
        if (SQLITE_OK != rc) {
            logmsg(LOG_ERR, "Cannot DELETE on nick table (%s)", errMsg);
            sqlite3_free(errMsg);
            db_close(sqlDB);
            return -1;
        }
        if (1 != sqlite3_changes(sqlDB)) {
            logmsg(LOG_DEBUG, "Trying to delete non-existing nick-name");
            rc = -1;
        }
        db_close(sqlDB);
    } else {
        rc = -1;
    }
    return rc;
}

/**
 * Get a list of all stored nick name and information of devices
 * and print them to the client socket
 * @param sockd Client socket
 * @param imei If specified then only return the entry for the specified IMEI number
 * @param listformat Format to use for list 0=one line per records, 1=two lines per record, 2=one line per field in record
 * @return 0 on success, -1 on failure
 */
int
db_get_nick_list(const int sockd, const char *imei, int listformat) {
    sqlite3 *sqlDB;
    int rc = 0;
    if (0 == db_setup(&sqlDB)) {
        char q[512];
        char *errMsg;

        if (imei && *imei) {
            snprintf(q, sizeof (q), "SELECT * FROM %s WHERE fld_imei=%s;", DB_TABLE_NICK, imei);
        } else {
            snprintf(q, sizeof (q), "SELECT * FROM %s;", DB_TABLE_NICK);
        }
        nick_cb_cnt = 0;
        rc = sqlite3_exec(sqlDB, q, update_nick_callback, (void *) NULL, &errMsg);
        if (SQLITE_OK != rc) {
            logmsg(LOG_ERR, "Cannot SELECT on nick table (%s)", errMsg);
            sqlite3_free(errMsg);
            db_close(sqlDB);
            return -1;
        }

        if (nick_cb_cnt > 0) {
            const char *hl1 = "----------------------------------------------------------------------------------------------------------------------------------\n";
            const char *hl3 = "-------------------------------------------\n";
            //#    Nick        DevId       IMEI             SIM               PHONE            FW.VER                   REG.DATE          UPD.DATE
            //01.  car01       3000000001  2354561265412    823543892652      079-0023459876   WP 2.1.13, Rev A02 P     2014-01-17 10:12  2014-01-19 12:33
            switch (listformat) {

                case 0:
                    // List
                    _writef(sockd, "%s%-3s%-12s%-11s%-17s%-21s%-15s%-21s%-16s%-16s\n%s",
                            hl1, "#", "NICK", "DEV.ID", "IMEI", "SIM", "PHONE", "FW.VER", "REG.DATE", "UPD.DATE", hl1);
                    for (size_t i = 0; i < nick_cb_cnt; ++i) {
                        _writef(sockd, "%-3zd%-12s%-11s%-17s%-21s%-15s%-21s%-16s%-16s\n",
                                i + 1,
                                nick_res_set[i].nick,
                                nick_res_set[i].devid,
                                nick_res_set[i].imei,
                                nick_res_set[i].sim,
                                nick_res_set[i].phone,
                                nick_res_set[i].fwver,
                                nick_res_set[i].regdate,
                                nick_res_set[i].upddate);
                    }
                    break;

                case 1:
                    // Multi column post
                    for (size_t i = 0; i < nick_cb_cnt; ++i) {
                        _writef(sockd, "%-4s%-12s%-21s%-18s%-21s\n", "#", "NICK", "DEV.ID", "IMEI", "SIM");
                        _writef(sockd, "%02zd  %-12s%-21s%-18s%-21s\n\n",
                                i + 1,
                                nick_res_set[i].nick,
                                nick_res_set[i].devid,
                                nick_res_set[i].imei,
                                nick_res_set[i].sim);

                        _writef(sockd, "%-16s%-21s%-18s%-16s\n", "PHONE", "FW.VER", "REG.DATE", "UPD.DATE");
                        _writef(sockd, "%-16s%-21s%-18s%-16s\n",
                                nick_res_set[i].phone,
                                nick_res_set[i].fwver,
                                nick_res_set[i].regdate,
                                nick_res_set[i].upddate);

                    }
                    break;

                case 2:
                    // Single column
                    for (size_t i = 0; i < nick_cb_cnt; ++i) {
                        _writef(sockd, "%s%-10s%02zd\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n",
                                hl3,
                                "#:", i + 1,
                                "NICK:", nick_res_set[i].nick,
                                "DEV.ID:", nick_res_set[i].devid,
                                "IMEI:", nick_res_set[i].imei,
                                "SIM:", nick_res_set[i].sim,
                                "PHONE:", nick_res_set[i].phone,
                                "FW.VER:", nick_res_set[i].fwver,
                                "REG.DATE:", nick_res_set[i].regdate,
                                "UPD.DATE:", nick_res_set[i].upddate);
                    }
                    break;
                case 3:
                    // Shortened list
                    _writef(sockd, "%s%-4s%-12s%-12s%-18s\n%s", hl3, "#", "NICK", "DEV.ID", "IMEI", hl3);
                    for (size_t i = 0; i < nick_cb_cnt; ++i) {
                        _writef(sockd, "%02zd  %-12s%-12s%-18s\n",
                                i + 1,
                                nick_res_set[i].nick,
                                nick_res_set[i].devid,
                                nick_res_set[i].imei);
                    }
                    break;
                default:
                    _writef(sockd, "[ERR] Unknown list format \"%d\"", listformat);
                    rc = -1;
                    break;
            }
        } else {
            _writef(sockd, "[ERR] No nick names defined yet");
        }
        db_close(sqlDB);
    } else {
        rc = -1;
    }

    return rc;
}

/* EOF */
