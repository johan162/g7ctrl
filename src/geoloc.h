/* =========================================================================
 * File:        geolocation_google.h
 * Description: Use the Google map API to do reverse geolocation lookup
 *              to translate lat/long to street address
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

#ifndef GEOLOCATION_GOOGLE_H
#define	GEOLOCATION_GOOGLE_H

#ifdef	__cplusplus
extern "C" {
#endif

// For anonymous API calls we need At least 200 ms between each API call. 
// This will result in around 4-5 calls queries per second on average.
// Which is just shy of Google API limits    
#define GOOGLE_ANONYMOUS_RLIMIT_MS 220
    
// For key:ed API calls we need At least 100 ms between each API call. 
// This will result in around 9-10 calls queries per second on average.
// Note: For a non-commercial developer key the limit is still 5 QPS
#define GOOGLE_APIKEY_RLIMIT_MS 110

// How many retries before we block further calls to the gecode lookup
// for 24h
#define FAILED_API_CALLS_BEFORE_BLOCKING_24H 2
    
/** 
 * Error status code returned is -(10+offset) in the status_code
 * array.
 */
#define GOOGLE_STATUS_OK 0
#define GOOGLE_STATUS_OVERQUOTA -11
#define GOOGLE_STATUS_ZERO -12
#define GOOGLE_STATUS_INVALID -13
#define GOOGLE_STATUS_DENIED -14
#define GOOGLE_STATUS_UNKNOWN -15

/**
 * Size of cache vectors
 */
#define GEOCACHE_ADDRESS_SIZE 10000
#define GEOCACHE_MINIMAP_SIZE 1000

/**
 * Statistics for each geo-location cache 
 */
struct cache_stat_t {
    unsigned cache_tot_calls;
    unsigned cache_hits;
    unsigned cache_max_idx;
};

enum geo_cache_t {
    CACHE_ADDR=0, CACHE_MINIMAP=1
};

int
get_cache_stat(enum geo_cache_t geo_cache, unsigned *tot_call, double *hitrate, double *cache_fill, size_t *musage);

int
read_address_geocache(void);

int
write_address_geocache(void);

int
read_mininap_geocache(void);

int
write_minimap_geocache(void);

int
geocode_rate_limit_init(unsigned long limit);

int
staticmap_rate_limit_init(unsigned long limit);

void
geocode_rate_limit_reset(void);

void
staticmap_rate_limit_reset(void);

int
get_minimap_from_latlon(const char *lat, const char *lon, unsigned short zoom, int width, int height, char **imagedata, size_t *datasize);

int
get_address_from_latlon(char *lat, char *lon,char *address, size_t maxlen);


#ifdef	__cplusplus
}
#endif

#endif	/* GEOLOCATION_GOOGLE_H */

