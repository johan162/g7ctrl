/* =========================================================================
 * File:        nicks.h
 * Description: Handle device nick names
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: nicks.h 644 2015-01-10 10:18:27Z ljp $
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

#ifndef NICKS_H
#define	NICKS_H

#ifdef	__cplusplus
extern "C" {
#endif

#define DB_SCHEMA_NICK "CREATE TABLE tbl_device_nick "\
  "( 'fld_nick' TEXT NOT NULL,"\
  "'fld_devid' INTEGER NOT NULL,"\
  "'fld_imei' INTEGER PRIMARY KEY NOT NULL,"\
  "'fld_sim' TEXT NOT NULL,"\
  "'fld_phone' TEXT NOT NULL,"\
  "'fld_fwver' TEXT NOT NULL,"\
  "'fld_regdate' TEXT NOT NULL,"\
  "'fld_upddate' TEXT NOT NULL);"

#define DB_TABLE_NICK "tbl_device_nick"

#define MAX_NICK_RES_SET 100

struct nick_res_t {
    char nick[16];
    char devid[16];
    char imei[24];
    char sim[24];
    char phone[24];
    char fwver[48];
    char regdate[24];
    char upddate[24];
} ;

int
db_get_nick_list(const int sockd, const char *imei, int listformat);

int
db_delete_nick(const char *nick) ;

int
db_get_nick_from_devid(const char *devid, char *nick);

int
db_get_devid_from_nick(const char *nick, char *devid);

int
db_update_nick(const char *nick, const char *devid, const char *imei, const char *sim, const char *phone, const char *fwver) ;


#ifdef	__cplusplus
}
#endif

#endif	/* NICKS_H */

