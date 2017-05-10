/* =========================================================================
 * File:        geoloc_cache.c
 * Description: Implements a cache function for geolocation lookups to avoid
 *              making too many API calls to Google. 
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

// We want the full POSIX and C99 standard
#define _GNU_SOURCE

// Standard defines
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "logger.h"
#include "gpsdist.h"
#include "g7config.h"
#include "libxstr/xstr.h"
#include "utils.h"
#include "futils.h"
#include "geoloc_cache.h"


static struct cache_stat_t cache_stats[2];

// static array guarantees zero initializing
static struct address_cache_t *address_cache;
static size_t address_cache_idx = 0; // Next free position
static size_t address_cache_num=0;

static struct minimap_cache_t *minimap_cache;
static size_t minimap_cache_idx = 0; // Next free position
static size_t minimap_cache_num=0;

static _Bool isInit = FALSE;

/**
 * Initialize memory structures for the geolocation cache
 */
void
init_geoloc_cache(void) {
    address_cache = (struct address_cache_t *) _chk_calloc_exit(geocache_address_size * sizeof *address_cache); //calloc(GEOCACHE_ADDRESS_SIZE, sizeof *address_cache );
    minimap_cache = (struct minimap_cache_t *) _chk_calloc_exit(geocache_minimap_size * sizeof *minimap_cache); //calloc(GEOCACHE_MINIMAP_SIZE, sizeof *minimap_cache );

    isInit = TRUE;
}

/**
 * Update statics for cache usage
 * @param cache_idx Which cache to update
 * @param hit IS this a cache hit update
 * @param idx The current cache index
 */
static void
update_cache_stat(unsigned cache_idx, _Bool hit, size_t idx) {
    assert(isInit);
    cache_stats[cache_idx].cache_tot_calls++;
    if (idx > cache_stats[cache_idx].cache_max_idx)
        cache_stats[cache_idx].cache_max_idx = idx;
    if (hit)
        cache_stats[cache_idx].cache_hits++;
}

/**
 * Return a pointer to a structure that holds basic statistics for the specified 
 * cache. 
 * @param cache_idx Which cache
 * @return Pointer to cache structure
 */
struct cache_stat_t *
get_cache_entry(enum geo_cache_t geo_cache) {
    assert(isInit);
    return &cache_stats[geo_cache];
}

/**
 * Calculate memory usage (in bytes) for address cache
 * @return The cache size in bytes
 */
size_t
get_addrcache_memusage(void) {

    assert(isInit);

    size_t musage = 0;
    size_t idx = 0;

    musage = geocache_address_size * sizeof (struct address_cache_t);
    while (idx < geocache_address_size && address_cache[idx].addr) {
        musage += strlen(address_cache[idx].addr);
        musage += strlen(address_cache[idx].lat);
        musage += strlen(address_cache[idx].lon);
        idx++;
    }

    return musage;
}

/**
 * Calculate memory usage (in bytes) for minimap cache
 * @return The cache size in bytes
 */
size_t
get_minimapcache_memusage(void) {

    assert(isInit);

    size_t musage = 0;
    size_t idx = 0;

    musage = geocache_minimap_size * sizeof (struct minimap_cache_t);
    while (idx < geocache_minimap_size && minimap_cache[idx].filename) {
        musage += strlen(minimap_cache[idx].filename);
        musage += strlen(minimap_cache[idx].lat);
        musage += strlen(minimap_cache[idx].lon);
        musage += minimap_cache[idx].imgdatasize;
        idx++;
    }

    return musage;
}

/**
 * Calculate cache statistics for selected cache 
 * @param geo_cache Selection of either address or minimap cache
 * @param tot_call Return the total number of calls
 * @param hitrate The cache hit rate as a decimal between 0.0 to 1.0
 * @param cache_fill The fill rate of the cache a decimal between 0.0 to 1.0
 * @param musage Memory usage (in bytes)
 * @return 0 on success, -1 on failure
 */
int
get_cache_stat(enum geo_cache_t geo_cache, unsigned *tot_call, double *hitrate, double *cache_fill, size_t *musage) {
    if (geo_cache == CACHE_ADDR) {
        *tot_call = cache_stats[geo_cache].cache_tot_calls;
        if (*tot_call > 0) {
            *hitrate = (double) cache_stats[geo_cache].cache_hits / *tot_call;
        } else {
            *hitrate = 0;
        }
        *cache_fill = (double) cache_stats[geo_cache].cache_max_idx / geocache_address_size;
        *musage = get_addrcache_memusage();
        logmsg(LOG_DEBUG, "GEO ADDRESS: address_idx=%zu, cache_max_idx=%u", address_cache_idx, cache_stats[geo_cache].cache_max_idx);
        return 0;
    } else if (geo_cache == CACHE_MINIMAP) {
        *tot_call = cache_stats[geo_cache].cache_tot_calls;
        if (*tot_call > 0) {
            *hitrate = (double) cache_stats[geo_cache].cache_hits / *tot_call;
        } else {
            *hitrate = 0;
        }
        *cache_fill = (double) cache_stats[geo_cache].cache_max_idx / geocache_minimap_size;
        *musage = get_minimapcache_memusage();
        logmsg(LOG_DEBUG, "GEO MINIMAP: minimap_idx=%zu, cache_max_idx=%u", minimap_cache_idx, cache_stats[geo_cache].cache_max_idx);
        return 0;
    }
    return -1;
}

/**
 * Return the number of elements in the specified cache and its maximum size
 * @return 0 on success, -1 on failure
 */
int 
get_cache_num(enum geo_cache_t geo_cache, size_t *num, size_t *max_num) {
  if( geo_cache == CACHE_ADDR ) {
    *num = address_cache_num;
    *max_num = geocache_address_size;
  } else if ( geo_cache == CACHE_MINIMAP ) {
    *num = minimap_cache_num;
    *max_num = geocache_minimap_size;
  } else {
    return -1;
  }
  logmsg(LOG_DEBUG,"Elements in %s cache: current=%zu, max=%zu",geo_cache==CACHE_ADDR?"address":"minimap", *num, *max_num);
  return 0;
}

/**
 * Write the cache statistics necessary to recreate the statistics on startup
 * @return 0 on success, -1 on failure
 */
int
write_geocache_stat(void) {
    // The cache saved stats will always be written to the defined data directory
    // Which is available as the defined DEFAULT_DB_DIR
    char fullPath[256];
    snprintf(fullPath, sizeof (fullPath), "%s/%s", db_dir, DEFAULT_GEOCACHE_SAVEDSTAT_FILE);
    FILE *fp = fopen(fullPath, "w");
    if (NULL == fp) {
        logmsg(LOG_ERR, "Cannot create address geocache saved stat file \"%s\"  ( %d : %s )", fullPath, errno, strerror(errno));
        return -1;
    }
    fprintf(fp, "%u;%u\n", cache_stats[CACHE_ADDR].cache_tot_calls, cache_stats[CACHE_ADDR].cache_hits);
    fprintf(fp, "%u;%u\n", cache_stats[CACHE_MINIMAP].cache_tot_calls, cache_stats[CACHE_MINIMAP].cache_hits);
    fclose(fp);
    logmsg(LOG_INFO, "Saved geocache stat to \"%s\"", fullPath);
    return 0;
}

/**
 * Read back cache statistics from default file
 * @return 0 on success, -1 on failure
 */
int
read_geocache_stat(void) {
    // The cache saved stats will always be written to the defined data directory
    // Which is available as the defined DEFAULT_DB_DIR
    char fullPath[256];
    snprintf(fullPath, sizeof (fullPath), "%s/%s", db_dir, DEFAULT_GEOCACHE_SAVEDSTAT_FILE);
    FILE *fp = fopen(fullPath, "r");
    if (NULL == fp) {
        if (errno == ENOENT) {
            logmsg(LOG_INFO, "No saved geo cache stat file exist.");
            return 0;
        } else {
            logmsg(LOG_ERR, "Failed to open saved geo cache stat file \"%s\" ( %d : %s )", fullPath, errno, strerror(errno));
            return -1;
        }
    }

    // Read file line by line
    char lbuff[256];
    struct splitfields fields;

    unsigned tot_calls, hits;
    for (size_t i = 0; i < 2; i++) {
        if (fgets(lbuff, sizeof (lbuff) - 1, fp)) {
            // Get rid of trailing newlines
            xstrtrim_crnl(lbuff);
            xstrsplitfields(lbuff, ';', &fields);
            if (2 == fields.nf) {
                tot_calls = xatoi(fields.fld[0]);
                hits = xatoi(fields.fld[1]);
                size_t cache = i == 0 ? CACHE_ADDR : CACHE_MINIMAP;
                cache_stats[cache].cache_tot_calls = tot_calls;
                cache_stats[cache].cache_hits = hits;
            } else {
                logmsg(LOG_INFO, "Corrupt file for saved geo cache stat on line %zu", i);
                fclose(fp);
                return -1;
            }
        }
    }
    fclose(fp);
    return 0;
}

/**
 * Write address geo cache to file
 * @return 0 on success, -1 on failure
 */
int
write_address_geocache(void) {
    // The cache will always be written to the defined data directory
    // Which is available as the defined DEFAULT_DB_DIR
    char fullPath[256];
    char fullBackupPath[256];
    snprintf(fullPath, sizeof (fullPath), "%s/%s", db_dir, DEFAULT_ADDRESS_GEOCACHE_FILE);
    snprintf(fullBackupPath, sizeof (fullBackupPath), "%s/%s", db_dir, DEFAULT_ADDRESS_GEOCACHE_BACKUPFILE);

    
    // Always create a backup file first. This might look strange
    // but this achieves what we want. It takes a possible previous backup file
    // and renames it so we can write the latest backup to the non-indexed 
    // backup file in the next step.
    (void)mv_and_rename(fullBackupPath, fullBackupPath, NULL, 0);


    logmsg(LOG_DEBUG, "Trying to backup existing address cache ...");
    if( -1 == mv_and_rename(fullPath, fullBackupPath, NULL, 0) ) {
      logmsg(LOG_ERR, "Couldn't save backup of geoloc cache for addresses");
      return -1;
    }

    logmsg(LOG_DEBUG, "Created backup.");

    
    FILE *fp = fopen(fullPath, "w");
    if (NULL == fp) {
        logmsg(LOG_ERR, "Cannot create address geocache file \"%s\"  ( %d : %s )", fullPath, errno, strerror(errno));
        return -1;
    }
    for (size_t i = 0; i <= cache_stats[CACHE_ADDR].cache_max_idx && address_cache[i].addr ; ++i) {
        //xstrtrim_crnl(_cache[i].addr);
        fprintf(fp, "%ld;%s;%s;%s\n", address_cache[i].ts, address_cache[i].lat, address_cache[i].lon, address_cache[i].addr);
    }
    (void) fclose(fp);
    logmsg(LOG_INFO, "Wrote %zu entries to saved address geocache file \"%s\"", address_cache_idx, fullPath);
    return 0;
}

/**
 * Write minimap geo cache to file
 * @return 0 on success, -1 on failure
 */
int
write_minimap_geocache(void) {
    // The cache will always be written to the defined data directory
    // Which is available as the defined DEFAULT_DB_DIR
    char fullPath[256];
    snprintf(fullPath, sizeof (fullPath), "%s/%s", db_dir, DEFAULT_MINIMAP_GEOCACHE_FILE);

    FILE *fp = fopen(fullPath, "w");
    if (NULL == fp) {
        logmsg(LOG_ERR, "Cannot create minimap geocache file \"%s\"  ( %d : %s )", fullPath, errno, strerror(errno));
        return -1;
    }

    // Remove the old directory where all the images are stored

    char mapfilename[255];
    for (size_t i = 0; i <= cache_stats[CACHE_MINIMAP].cache_max_idx && minimap_cache[i].lat ; ++i) {
        fprintf(fp, "%ld;%s;%s;%d;%d;%d;%s\n", minimap_cache[i].ts,
                minimap_cache[i].lat, minimap_cache[i].lon,
                minimap_cache[i].zoom,
                minimap_cache[i].width, minimap_cache[i].height,
                minimap_cache[i].filename);

        snprintf(mapfilename, sizeof (mapfilename), "%s/%s/%s", db_dir, DEFAULT_MINIMAP_GEOCACHE_DIR, minimap_cache[i].filename);

        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        int fd = open(mapfilename, O_CREAT | O_RDWR, mode);
        if (fd < 0) {
            logmsg(LOG_ERR, "Failed to open minimap for writing. Aborting writing cache file \"%s\" ( %d : %s)",
                    mapfilename, errno, strerror(errno));
            fclose(fp);
            return -1;
        }
        int rc = write(fd, minimap_cache[i].imgdata, minimap_cache[i].imgdatasize);
        close(fd);
        if (-1 == rc) {
            logmsg(LOG_ERR, "Failed to write minimap image. ( %d : %s )", errno, strerror(errno));
            fclose(fp);
            return -1;
        }

    }

    (void) fclose(fp);
    logmsg(LOG_INFO, "Wrote %zu entries to saved minimap geocache file \"%s\"", minimap_cache_idx, fullPath);
    return 0;
}

/**
 * Read saved address geo cache from file
 * @return 0 on success, -1 on failure
 */
int
read_address_geocache(void) {
    assert(isInit);

    char fullPath[256];
    snprintf(fullPath, sizeof (fullPath), "%s/%s", db_dir, DEFAULT_ADDRESS_GEOCACHE_FILE);

    FILE *fp = fopen(fullPath, "r");
    if (NULL == fp) {
        if (errno == ENOENT) {
            logmsg(LOG_INFO, "No saved geo cache file exist.");
            return 0;
        } else {
            logmsg(LOG_ERR, "Failed trying to read saved geo cache file \"%s\" ( %d : %s )", fullPath, errno, strerror(errno));
            return -1;
        }
    } else {
        // Read file line by line
        char lbuff[256];
        struct splitfields fields;
        address_cache_idx = 0;
        double lat, lon;
        while (address_cache_idx < geocache_address_size - 1 && fgets(lbuff, sizeof (lbuff) - 1, fp)) {

            // Get rid of trailing newlines
            xstrtrim_crnl(lbuff);
            xstrsplitfields(lbuff, ';', &fields);

            if (4 == fields.nf) {
                lat = xatof(fields.fld[1]);
                lon = xatof(fields.fld[2]);
                if (fabs(lat) < 1.0 || fabs(lat) > 89.0 || fabs(lon) < 1.0 || fabs(lon) > 89.0 || strnlen(fields.fld[3], 255) < 5) {
                    logmsg(LOG_ERR, "Address geocache file invalid. Reading aborted");
                    address_cache_idx = 0;
                    fclose(fp);
                    return -1;
                } else {
                    address_cache[address_cache_idx].ts = xatol(fields.fld[0]);
                    address_cache[address_cache_idx].lat = strdup(fields.fld[1]);
                    address_cache[address_cache_idx].lon = strdup(fields.fld[2]);
                    address_cache[address_cache_idx].dLat = lat;
                    address_cache[address_cache_idx].dLon = lon;
                    address_cache[address_cache_idx].addr = strdup(fields.fld[3]);
                }

            }

            address_cache_idx++;
        }
        logmsg(LOG_INFO, "Read %zu entries from geo cache file \"%s\"", address_cache_idx, fullPath);
        cache_stats[CACHE_ADDR].cache_max_idx = address_cache_idx;
	address_cache_num = address_cache_idx;
        fclose(fp);
        return 0;
    }
}

/**
 * Read saved address geo cache from file
 * @return 0 on success, -1 on failure
 */
int
read_minimap_geocache(void) {

    assert(isInit);

    char fullPath[256];
    snprintf(fullPath, sizeof (fullPath), "%s/%s", db_dir, DEFAULT_MINIMAP_GEOCACHE_FILE);

    FILE *fp = fopen(fullPath, "r");
    if (NULL == fp) {
        if (errno == ENOENT) {
            logmsg(LOG_INFO, "No saved minimap geo cache file exist.");
            return 0;
        } else {
            logmsg(LOG_ERR, "Failed trying to read saved minimap geo cache file \"%s\" ( %d : %s )", fullPath, errno, strerror(errno));
            return -1;
        }
    } else {
        // Read file line by line
        char lbuff[256];
        struct splitfields fields;
        minimap_cache_idx = 0;
        double lat, lon;
        while (minimap_cache_idx < geocache_minimap_size - 1 && fgets(lbuff, sizeof (lbuff) - 1, fp)) {

            // Get rid of trailing newlines
            xstrtrim_crnl(lbuff);
            xstrsplitfields(lbuff, ';', &fields);

            if (7 == fields.nf) {
                lat = xatof(fields.fld[1]);
                lon = xatof(fields.fld[2]);
                if (fabs(lat) < 1.0 || fabs(lat) > 89.0 || fabs(lon) < 1.0 || fabs(lon) > 89.0) {
                    logmsg(LOG_ERR, "Minimap geocache file invalid. Reading aborted");
                    minimap_cache_idx = 0;
                    fclose(fp);
                    return -1;
                } else {
                    char mapfilename[255];
                    snprintf(mapfilename, sizeof (mapfilename), "%s/%s/%s", db_dir, DEFAULT_MINIMAP_GEOCACHE_DIR, fields.fld[6]);

                    int rc = read_file_buffer(mapfilename,
                            & minimap_cache[minimap_cache_idx].imgdatasize,
                            & minimap_cache[minimap_cache_idx].imgdata);

                    if (0 == rc) {
                        minimap_cache[minimap_cache_idx].ts = xatol(fields.fld[0]);
                        minimap_cache[minimap_cache_idx].lat = strdup(fields.fld[1]);
                        minimap_cache[minimap_cache_idx].lon = strdup(fields.fld[2]);
                        minimap_cache[minimap_cache_idx].dLat = lat;
                        minimap_cache[minimap_cache_idx].dLon = lon;
                        minimap_cache[minimap_cache_idx].zoom = xatol(fields.fld[3]);
                        minimap_cache[minimap_cache_idx].width = xatol(fields.fld[4]);
                        minimap_cache[minimap_cache_idx].height = xatol(fields.fld[5]);
                        minimap_cache[minimap_cache_idx].filename = strdup(fields.fld[6]);

                        minimap_cache_idx++;
                    } else {
                        logmsg(LOG_ERR, "Failed to read minimap image file from disk \"%s\"",
                                minimap_cache[minimap_cache_idx].filename);
                    }
                }

            } else {

                logmsg(LOG_ERR, "Invalid line in saved minimap cache file ignoring line");
            }


        }
        logmsg(LOG_INFO, "Read %zu entries from geo cache file \"%s\"", minimap_cache_idx, fullPath);
        cache_stats[CACHE_MINIMAP].cache_max_idx = minimap_cache_idx;
	minimap_cache_num = minimap_cache_idx;
        fclose(fp);
        return 0;
    }
}

/**
 * Check if position is in the cache. In that case return <> 0 and store
 * address in the location pointed to by addr with maximum size maxlen
 * @param lat Latitude to check
 * @param lon Longitude to check
 * @param addr Pointer to where to store the found address
 * @param maxlen The maximum size of address buffer
 * @return 1 if found 0 if not found
 */
int
in_address_cache(char *lat, char *lon, char *addr, size_t maxlen) {
    assert(isInit);

    // Search for match of lon & lat
    size_t idx = 0;

    const double dLat = atof(lat);
    const double dLon = atof(lon);
    while (idx < geocache_address_size && address_cache[idx].lat) {
        if (address_lookup_proximity > 0) {
            const double dist = gpsdist_m(dLat, dLon, address_cache[idx].dLat, address_cache[idx].dLon);
            if (dist <= (double) address_lookup_proximity) {
                xmb_strncpy(addr, address_cache[idx].addr, maxlen);
                logmsg(LOG_INFO, "Geocache address approx HIT for (%s,%s) -> \"%s\" distance=%.0f from (%.6f,%.6f)",
                        lat, lon, addr, dist,
                        address_cache[idx].dLat,
                        address_cache[idx].dLon);
                update_cache_stat(CACHE_ADDR, TRUE, idx);
                return TRUE;
            }
        } else {
            // Use strcmp() to avoid conversion floating point problems 
            if (0 == strcmp(lat, address_cache[idx].lat) && 0 == strcmp(lon, address_cache[idx].lon)) {
                xmb_strncpy(addr, address_cache[idx].addr, maxlen);
                logmsg(LOG_INFO, "Geocache address HIT (%s,%s) -> \"%s\"", lat, lon, addr);
                update_cache_stat(CACHE_ADDR, TRUE, idx);
                return TRUE;
            }
        }
        idx++;
    }

    // The index is irrelevant here since it is always 1+ over the size
    update_cache_stat(CACHE_ADDR, FALSE, 0);
    return FALSE;
}

/**
 * Check if the requested minimap exists in the cache. This is a two stage process. First
 * we check if it is in the memory vector. If it is then we construct the filename
 * from the parameters and check if it is in the cache directory. If it is then we read the
 * image by allocating storage.
 * @param lat Latitude to check
 * @param lon Longitude to check
 * @param zoom Zoom factor
 * @param width width of image
 * @param height height of image
 * @param[out] imgdata An allocated memory areas where the imagedata is stored. It is
 * the calling routines responsibility to free the memory
 * @param[out] imgsize The size of the image data
 * @return 1 if the image was found, 0 otherwise 
 */
int
in_minimap_cache(const char *lat, const char *lon, unsigned zoom, unsigned width, unsigned height, char **imgdata, size_t *imgsize) {
    assert(isInit);

    // Search for match of lon & lat
    size_t idx = 0;

    const double dLat = atof(lat);
    const double dLon = atof(lon);
    _Bool found = FALSE;

    // For the overview map we use the same map for even larger approximate distances
    // from the "true" center since at this scale it requires multiple time longer distance
    // to make any difference in practice in the images. The scale difference in the images
    // is roughly 50 times (1:10 000 to 1:200) for the default zoom=9 and zoom=15.
    // to be on the "safe" side we recalculate the zoom factor with a multiplicative constant
    // on the relation between the overview and detailed zoom factor.
    // For the default proximity of 20m it means that we must move 500m for the overview map
    // to be changed.
    const double proximity_dist_factor = minimap_detailed_zoom / minimap_overview_zoom * 15;
    const int proximity_dist = zoom == minimap_overview_zoom ? address_lookup_proximity * proximity_dist_factor : address_lookup_proximity;
    logmsg(LOG_DEBUG, "Using proximity distance (%d m)", proximity_dist);

    while (idx < geocache_minimap_size && minimap_cache[idx].lat) {

        if (zoom != minimap_cache[idx].zoom || minimap_cache[idx].width != width || minimap_cache[idx].height != height) {
            idx++;
            continue;
        }

        if (proximity_dist > 0) {
            const double dist = gpsdist_m(dLat, dLon, minimap_cache[idx].dLat, minimap_cache[idx].dLon);

            if (dist <= (double) proximity_dist) {
                found = TRUE;
                logmsg(LOG_INFO, "Minimap geocache approx HIT for (%s,%s), distance %.0f m to (%.6f,%.6f). File \"%s\"",
                        lat, lon, dist, minimap_cache[idx].dLat, minimap_cache[idx].dLon,
                        minimap_cache[idx].filename);
            }

        } else {

            if (0 == strcmp(lat, minimap_cache[idx].lat) && 0 == strcmp(lon, minimap_cache[idx].lon)) {
                found = TRUE;
                logmsg(LOG_INFO, "Minimap geocache HIT (%s,%s). File \"%s\"", lat, lon, minimap_cache[idx].filename);
            }

        }

        if (found) {
            *imgdata = minimap_cache[idx].imgdata;
            *imgsize = minimap_cache[idx].imgdatasize;
            update_cache_stat(CACHE_MINIMAP, TRUE, idx);
            return TRUE;
        }

        idx++;
    }

    // The index is irrelevant here. We just want to mark this as a miss
    update_cache_stat(CACHE_MINIMAP, FALSE, 0);
    return FALSE;
}

/**
 * Create a cache filename from the details of the static google map image
 * @param cfname Pointer to storage for the created filename
 * @param maxlen Maximum size of filename
 * @param lat Latitude of image center
 * @param lon Longitude of image center
 * @param zoom Zoom factor
 * @param size Size string
 * @return  0 on success, -1 on failure
 */
int
get_minimap_cache_filename(char *cfname, size_t maxlen, const char *lat, const char *lon, unsigned zoom, unsigned width, unsigned height) {

    // The name is created by combining the prefix "map_" first 6 digit from the lat,lon, the zoom factor (as 2 digits) and t¨
    // the size as a string
    // For example "map_571293_174361_15_200x200.png"


    char blat[16], blon[16];
    char *plat = blat;
    char *plon = blon;

    // Extract the part before the decimal comma
    while (*lat && *lat != '.') *plat++ = *lat++;
    while (*lon && *lon != '.') *plon++ = *lon++;

    // Read past the '.'
    lat++;
    lon++;

    const int precision = 6;
    for (int i = 0; i < precision; i++) {
        *plat++ = *lat++;
        *plon++ = *lon++;
    }
    *plat = '\0';
    *plon = '\0';

    char fname[256];
    snprintf(fname, sizeof (fname), "map_%s_%s_%d_%dx%d.png", blat, blon, zoom, width, height);
    if (strlen(fname) >= maxlen)
        return -1;
    xstrlcpy(cfname, fname, maxlen);

    // Sanity check
    if (strlen(cfname) < 31) {
        logmsg(LOG_ERR, "Internal error. Invalid cache filename created (%s) for minimap cache", cfname);
        return -1;
    }

    return 0;
}

/**
 * Updated the in-memory minimap cache for the given position
 * @param lat Latitude for minimap
 * @param lon Longitude for minimap
 * @param zoom Zoom factor
 * @param width Width of image
 * @param height Height of image
 * @param imgdatasize Size of image (in bytes)
 * @param imgdata Pointer to the image data
 * @return 0 on success, -1 on failure
 */
int
update_minimap_cache(const char *lat, const char *lon, unsigned zoom, unsigned width, unsigned height, size_t imgdatasize, char *imgdata) {
    assert(isInit);

    logmsg(LOG_DEBUG, "Updating minimap geocache [idx=%zu] (%s,%s) [%d,%dx%d]", minimap_cache_idx, lat, lon, zoom, width, height);

    const double dLat = atof(lat);
    const double dLon = atof(lon);

    if (fabs(dLat) > 89 || fabs(dLon) > 89 || fabs(dLon) < 1 || fabs(dLat) < 1) {
        logmsg(LOG_ERR, "Invalid lat, lon specified for updating minimap cache");
        return -1;
    }

    if (zoom < 1 || zoom > 20) {
        logmsg(LOG_ERR, "Invalid zoom factor specified for updating minimap cache");
        return -1;
    }

    if (width > 600 || height > 600) {
        logmsg(LOG_ERR, "Invalid width or height specified when updating minimap cache");
        return -1;
    }


    char filename[255];
    int rc = get_minimap_cache_filename(filename, sizeof (filename),
            lat, lon, zoom, width, height);

    if (rc) {
        logmsg(LOG_ERR, "Failed to create minimap filename");
        return -1;
    }

    if (minimap_cache[minimap_cache_idx].lat) {

        // We are reusing an older cache entry
        minimap_cache[minimap_cache_idx].lat = realloc(minimap_cache[minimap_cache_idx].lat, strlen(lat) + 1);
        minimap_cache[minimap_cache_idx].lon = realloc(minimap_cache[minimap_cache_idx].lon, strlen(lon) + 1);
        xstrlcpy(minimap_cache[minimap_cache_idx].lat, lat, strlen(lat));
        xstrlcpy(minimap_cache[minimap_cache_idx].lon, lon, strlen(lon));

        free(minimap_cache[minimap_cache_idx].filename);
        free(minimap_cache[minimap_cache_idx].imgdata);

    } else {

        minimap_cache[minimap_cache_idx].lat = strdup(lat);
        minimap_cache[minimap_cache_idx].lon = strdup(lon);

    }

    minimap_cache[minimap_cache_idx].imgdata = imgdata;
    minimap_cache[minimap_cache_idx].imgdatasize = imgdatasize;
    minimap_cache[minimap_cache_idx].dLat = dLat;
    minimap_cache[minimap_cache_idx].dLon = dLon;
    minimap_cache[minimap_cache_idx].zoom = zoom;
    minimap_cache[minimap_cache_idx].width = width;
    minimap_cache[minimap_cache_idx].height = height;
    minimap_cache[minimap_cache_idx].filename = strdup(filename);

    minimap_cache[minimap_cache_idx].ts = time(NULL);
    minimap_cache_idx++;

    if (minimap_cache_idx >= geocache_minimap_size) {
        logmsg(LOG_DEBUG, "Wrapping minimap geocache index at end [idx=%zu]", minimap_cache_idx);
        minimap_cache_idx = 0;
    } else {
      minimap_cache_num++;
    }
    return 0;
}

/**
 * Updated the cache with new value
 * @param lat Latitude for address
 * @param lon Longitude for address
 * @param addr Address for the position
 * @return 0 on success, -1 on failure
 */
int
update_address_cache(char *lat, char *lon, char *addr) {

    assert(isInit);

    logmsg(LOG_DEBUG, "Updating address geocache [idx=%zu] (%s,%s) -> \"%s\"", address_cache_idx, lat, lon, addr);
    const double dLat = atof(lat);
    const double dLon = atof(lon);

    if (fabs(dLat) > 89 || fabs(dLon) > 89 || fabs(dLon) < 1 || fabs(dLat) < 1) {
        logmsg(LOG_ERR, "Invalid lat, lon specified when updating address cache");
        return -1;
    }

    if (address_cache[address_cache_idx].lat) {
        address_cache[address_cache_idx].lat = realloc(address_cache[address_cache_idx].lat, strlen(lat) + 1);
        address_cache[address_cache_idx].lon = realloc(address_cache[address_cache_idx].lon, strlen(lon) + 1);
        address_cache[address_cache_idx].addr = realloc(address_cache[address_cache_idx].addr, strlen(addr) + 1);
        xstrlcpy(address_cache[address_cache_idx].addr, addr, strlen(addr));
        xstrlcpy(address_cache[address_cache_idx].lat, lat, strlen(lat));
        xstrlcpy(address_cache[address_cache_idx].lon, lon, strlen(lon));
        address_cache[address_cache_idx].dLat = dLat;
        address_cache[address_cache_idx].dLon = dLon;
    } else {
        address_cache[address_cache_idx].lat = strdup(lat);
        address_cache[address_cache_idx].lon = strdup(lon);
        address_cache[address_cache_idx].addr = strdup(addr);
        address_cache[address_cache_idx].dLat = dLat;
        address_cache[address_cache_idx].dLon = dLon;
    }
    address_cache[address_cache_idx].ts = time(NULL);
    address_cache_idx++;

    if (address_cache_idx >= geocache_address_size) {
        logmsg(LOG_DEBUG, "Wrapping address geocache index at end [idx=%zu]", address_cache_idx);
        address_cache_idx = 0;
    } else {
      address_cache_num++;
    }
    return 0;
}


/*
 * EOF
 */
