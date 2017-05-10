/* =========================================================================
 * File:        DBCMD.H
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


#ifndef DBCMD_H
#define	DBCMD_H

#ifdef	__cplusplus
extern "C" {
#endif

/* What version of DB schema is this */
#define DB_VERSION 2

/**
 * DB schema. If no database is found it will be initialized with this
 * SQL statement
 */
#define DB_SCHEMA_LOC "CREATE TABLE tbl_track  "\
  "('fld_key' INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "\
  "'fld_timestamp' INTEGER NOT NULL, "\
  "'fld_deviceid' INTEGER NOT NULL, "\
  "'fld_datetime' INTEGER NOT NULL, "\
  "'fld_lat' TEXT NOT NULL, "\
  "'fld_lon' TEXT NOT NULL, "\
  "'fld_approxaddr' TEXT NOT NULL, "\
  "'fld_speed' TEXT NOT NULL, "\
  "'fld_heading' INTEGER NOT NULL, "\
  "'fld_altitude' INTEGER NOT NULL, "\
  "'fld_satellite' INTEGER NOT NULL, "\
  "'fld_event' INTEGER NOT NULL, "\
  "'fld_voltage' TEXT NOT NULL, "\
  "'fld_detachstat' INTEGER NOT NULL);"

#define DB_TABLE_LOC "tbl_track"
#define _SQL_SELECT_COUNT "SELECT count(*) FROM tbl_track;"

#define DB_SCHEMA_INFO "CREATE TABLE tbl_info "\
  "('fld_key' INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "\
  "'fld_created' TEXT NOT NULL, "\
  "'fld_dbversion' INTEGER NOT NULL);"

#define DB_TABLE_INFO "tbl_info"
#define _SQL_INFO_INSERT "INSERT INTO %s (fld_created,fld_dbversion) VALUES ('%s',%d);"
#define _SQL_SELECT_DBVERSION "SELECT fld_dbversion FROM tbl_info;"

#define _SQL_PRAGMA "PRAGMA synchronous=OFF;PRAGMA journal_mode=MEMORY;PRAGMA temp_store=MEMORY;"

enum sort_order_t {
    SORT_DEVICETIME=0, SORT_ARRIVALTIME=1
};

void
db_set_sortorder(enum sort_order_t order);

const char *
db_get_sortorder_string(void);


int
db_setup(sqlite3 **sqlDB);

void
db_close(sqlite3 *sqlDB);

int
db_exec_sql(char *sql,int *changes);

int
db_store_loc(sqlite3 *sqlDB,
	     char *deviceid,char *datetime,char *lon,char *lat,char *speed,
	     char *heading,char *alt,char *sat,char *eventid,char *volt,char *detach);

int
db_store_locations(int sockd, unsigned num_loc, const char *recvBuff, void (*cb)(struct splitfields *,void *), void *cb_option);

void
db_add_wcond(char *w, size_t maxlen, char *col, char *op, char *val);

int
db_calc_distance(struct client_info *cli_info, ssize_t nf, char **fields);

int
db_lastloc(struct client_info *cli_info);

int
db_loclist(struct client_info *cli_info, _Bool head, size_t numrows);

int
db_head(struct client_info *cli_info, unsigned size);

int
db_tail(struct client_info *cli_info, unsigned size);

int
db_get_numevents(struct client_info *cli_info);

double
deg2rad(double d) __attribute__ ((pure));

double
gpsdist_km(double lat1, double lon1, double lat2, double lon2) __attribute__ ((pure));

double
gpsdist_mi(double lat1, double lon1, double lat2, double lon2) __attribute__ ((pure));

int
mail_lastloc(struct client_info *cli_info);

int
db_empty_loc(struct client_info *cli_info);

void
db_help(struct client_info *cli_info, char **fields);

#ifdef	__cplusplus
}
#endif

#endif	/* DBCMD_H */

