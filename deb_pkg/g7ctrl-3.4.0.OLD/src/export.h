/* =========================================================================
 * File:        export.h
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

#ifndef EXPORT_H
#define	EXPORT_H

#ifdef	__cplusplus
extern "C" {
#endif


/**
 * The structure for the in-memory internal export format populated from the
 * DB. This is used to gather an internal representation of the DB in order
 * to make it easier to export it to the other available formats. Each field
 * corresponds to one column in the database for locations.
 */
struct g7loc_t {
    char deviceid[16];
    char lat[16];
    char lon[16];
    char approxaddr[128];
    char date[48];    // Date in UTZ format
    unsigned long  date_timestamp; // Date in raw format (easier for calculations)
    char altitude[8];
    char speed[8];
    char voltage[8];
    char satellite[8];
    char event[8];
    char heading[8];
};


extern size_t resSetLength;
extern struct g7loc_t *g7loc_list;

// Forward declarations
char *
convert_to_utc(const char *datetime);

int
exportdb_to_external_format(struct client_info *cli_info, ssize_t nf, char **fields);

int
export_to_internal_set(sqlite3 *sqlDB,char *fromDate, char *toDate, char *deviceId, char *eventId);

int
mail_gpx_attachment(struct client_info *cli_info);

int
mail_csv_attachment(struct client_info *cli_info);

void
free_internal_set(void);

#ifdef	__cplusplus
}
#endif

#endif	/* EXPORT_H */

