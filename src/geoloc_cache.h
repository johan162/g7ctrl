/* =========================================================================
 * File:        geoloc_cache.c
 * Description: Implements a cache function for geolocation lookups to avoid
 *              making too many API calls to Google
 * Author:      Johan Persson (johan162@gmail.com)
 *
 * Copyright (C) 2015 Johan Persson
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

#ifndef GEOLOC_CACHE_H
#define GEOLOC_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Cache entry for address lookup
 */
struct address_cache_t {
    char *lat;
    char *lon;
    double dLat;
    double dLon;
    char *addr;
    time_t ts;
};

/**
 * Cache entry for minimap images lookup
 */
struct minimap_cache_t {
    char *lat;
    char *lon;
    double dLat;
    double dLon;
    unsigned zoom;
    unsigned width;
    unsigned height;
    char *filename;
    size_t imgdatasize;
    char *imgdata;
    time_t ts;
};

/**
 * Statistics for each geo-location cache 
 */
struct geo_cache_stat_t {
    unsigned cache_tot_calls;
    unsigned cache_hits;
    unsigned cache_max_idx;
};

/* The two types of cache we have, address and minimap*/
enum geo_cache_t {
    GEOCACHE_ADDR=0, GEOCACHE_MINIMAP=1
};


void
init_geoloc_cache(void);

int
write_geocache_stat(void);

int
read_geocache_stat(void);

int 
get_cache_num(enum geo_cache_t geo_cache, size_t *num, size_t *max_num);

int
in_address_cache(char *lat, char *lon, char *addr, size_t maxlen);

int
in_minimap_cache(const char *lat, const char *lon, unsigned zoom, unsigned width, unsigned height, char **imgdata, size_t *imgsize);

int
update_minimap_cache(const char *lat, const char *lon, unsigned zoom, unsigned width, unsigned height, size_t imgdatasize, char *imgdata);

int
update_address_cache(char *lat, char *lon, char *addr);

int
get_cache_stat(enum geo_cache_t geo_cache, unsigned *tot_call, double *hitrate, double *cache_fill, size_t *musage);


#ifdef __cplusplus
}
#endif

#endif /* GEOLOC_CACHE_H */

