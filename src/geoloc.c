/* =========================================================================
 * File:        geolocation_google.c
 * Description: Use the Google map API to do reverse geolocation lookup
 *              to translate lat/long to street address
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: geoloc.c 952 2015-04-09 12:41:20Z ljp $
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
#include <errno.h>
#include <math.h>

#include <curl/curl.h>
#include <curl/easy.h>
#include <sys/syslog.h>

// XML2 lib headers
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/xmlreader.h>
#include <sys/fcntl.h>
#include <ctype.h>

#include "config.h"
#include "logger.h"
#include "geoloc.h"
#include "gpsdist.h"
#include "g7config.h"
#include "xstr.h"
#include "utils.h"
#include "rkey.h"
#include "mailutil.h"
#include "futils.h"

static const xmlChar *resp_xml_root = (xmlChar *) "GeocodeResponse";
static const xmlChar *resp_xml_status = (xmlChar *) "status";
static const xmlChar *resp_xml_result = (xmlChar *) "result";
static const xmlChar *resp_xml_type = (xmlChar *) "type";
static const xmlChar *xml_text = (xmlChar *) "text";
static const xmlChar *resp_xml_street_address = (xmlChar *) "street_address";
static const xmlChar *resp_xml_postal_code = (xmlChar *)"postal_code";
static const xmlChar *resp_xml_formatted_address = (xmlChar *) "formatted_address";

/*
static const xmlChar *xml_OK = (xmlChar *) "OK";
static const xmlChar *xml_OverQueryLimit = (xmlChar *)"OVER_QUERY_LIMIT";
static const xmlChar *xml_ZeroResults = (xmlChar *)"ZERO_RESULTS";
static const xmlChar *xml_InvalidRequest = (xmlChar *)"INVALID_REQUEST";
static const xmlChar *xml_RequestDenied = (xmlChar *)"REQUEST_DENIED";
static const xmlChar *xml_UnknownError = (xmlChar *)"UNKNOWN_ERROR";
*/

#define NUM_STATUS_CODES 6
/**
 * Error codes returned by Googlw Web services
 */
static const char *status_codes[NUM_STATUS_CODES] = {
    "OK",
    "OVER_QUERY_LIMIT",
    "ZERO_RESULTS",
    "INVALID_REQUEST",
    "REQUEST_DENIED",
    "UNKNOWN_ERROR"
};


/**
 * Structure to store the read reply back from Google
 * We will ask Google to give us back the answer in XML format
 * for ease of parsing
 */
struct memoryStruct {
    char *memory;
    size_t size;
};

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
 * Cache entry for minimap images
 */
struct minimap_cache_t {
    char *lat;
    char *lon;
    double dLat;
    double dLon;
    int zoom;
    int width;
    int height;
    char *filename;
    size_t imgdatasize;
    char *imgdata;
    time_t ts;
};

#define TRUE 1
#define FALSE 0

static struct cache_stat_t cache_stats[2];

// static array guarantees zero initializing
static struct address_cache_t address_cache[GEOCACHE_ADDRESS_SIZE];
static size_t address_cache_idx = 0; // Next free position

static struct minimap_cache_t minimap_cache[GEOCACHE_MINIMAP_SIZE];
static size_t minimap_cache_idx = 0; // Next free position

// Keep track of the relative time we made the last callt o the
// Google API to be able to throttle the calls not to exceed the
// Google limit.
static unsigned long last_geocode_api_call=0;
static unsigned long last_staticmap_api_call=0;

// This gives around 8 lookups/sec at maximum
static unsigned long geocode_rlimit_ms=GOOGLE_ANONYMOUS_RLIMIT_MS;
static unsigned long staticmap_rlimit_ms=GOOGLE_ANONYMOUS_RLIMIT_MS;

int
get_minimap_cache_filename(char *cfname, size_t maxlen, const char *lat, const char *lon, int zoom, int width, int height);


/**
 * Update statics for cache usage
 * @param cache_idx Which cache to update
 * @param hit IS this a cache hit update
 * @param idx The current cache index
 */
static void
update_cache_stat(unsigned cache_idx, _Bool hit, size_t idx) {
    cache_stats[cache_idx].cache_tot_calls++;
    if( idx > cache_stats[cache_idx].cache_max_idx )
        cache_stats[cache_idx].cache_max_idx = idx;
    if( hit )
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
    return &cache_stats[geo_cache];
}

/**
 * Caclulate memory usage (in bytes) for address cache
 * @return The cache sise in bytes
 */
size_t
get_addrcache_memusage(void) {

    size_t musage = 0;
    size_t idx = 0;

    musage = GEOCACHE_ADDRESS_SIZE * sizeof (struct address_cache_t);
    while (idx < GEOCACHE_ADDRESS_SIZE && address_cache[idx].addr) {
            musage += strlen(address_cache[idx].addr);
            musage += strlen(address_cache[idx].lat);
            musage += strlen(address_cache[idx].lon);
            idx++;
    }
    
    return musage;    
}

/**
 * 
 * @return 
 */
size_t
get_minimapcache_memusage(void) {

    size_t musage = 0;
    size_t idx = 0;

    musage = GEOCACHE_MINIMAP_SIZE * sizeof (struct minimap_cache_t);
    while (idx < GEOCACHE_MINIMAP_SIZE && minimap_cache[idx].filename) {
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
    if( geo_cache == CACHE_ADDR ) {
        *tot_call = cache_stats[geo_cache].cache_tot_calls;
        if( *tot_call > 0 ) {
            *hitrate = (double)cache_stats[geo_cache].cache_hits / *tot_call;
        } else {
            *hitrate = 0;
        }
        *cache_fill = (double)cache_stats[geo_cache].cache_max_idx / GEOCACHE_ADDRESS_SIZE;
        *musage = get_addrcache_memusage();
        logmsg(LOG_DEBUG,"address_idx=%zu",address_cache_idx);
        return 0;
    } else if ( geo_cache == CACHE_MINIMAP ) {
        *tot_call = cache_stats[geo_cache].cache_tot_calls;
        if( *tot_call > 0 ) {
            *hitrate = (double)cache_stats[geo_cache].cache_hits / *tot_call;
        } else {
            *hitrate = 0;
        }
        *cache_fill = (double)cache_stats[geo_cache].cache_max_idx / GEOCACHE_MINIMAP_SIZE;
        *musage = get_minimapcache_memusage();
        logmsg(LOG_DEBUG,"minimap_idx=%zu",minimap_cache_idx);
        return 0;        
    }
    return -1;
}

/**
 * @param limit Set the minimum time in ms between each call that 
 * is rate limited by a call to rate_limit()
 * @return 0 on success, -1 on failure
 */
int
staticmap_rate_limit_init(unsigned long limit) {
  staticmap_rlimit_ms = limit;
  return mtime(&last_staticmap_api_call);
}

/**
 * Calls this function just before a call is made to an API that must
 * be throttled to a maximum of n calls/s. The rate limit is specified
 * by initializing the rate limit with a call to rate_limit_init()
 * If the previous call was made in too short time this function will sleep
 * until the minimum throttle time between calls has bee reached and then
 * return.
 */
void 
staticmap_rate_limit(void) {

  unsigned long t1;
  mtime(&t1);
  const unsigned long diff = t1-last_staticmap_api_call;
  if( diff < staticmap_rlimit_ms ) {
        logmsg(LOG_DEBUG,"Rate limiting (%lu ms)",staticmap_rlimit_ms-diff);
        usleep((staticmap_rlimit_ms-diff)*1000); 
  }
  (void)mtime(&last_staticmap_api_call);
}



/**
 * @param limit Set the minimum time in ms between each call that 
 * is rate limited by a call to rate_limit()
 * @return 0 on success, -1 on failure
 */
int
geocode_rate_limit_init(unsigned long limit) {
  geocode_rlimit_ms = limit;
  return mtime(&last_geocode_api_call);
}


/**
 * Calls this function just before a call is made to an API that must
 * be throttled to a maximum of n calls/s. The rate limit is specified
 * by initializing the rate limit with a call to rate_limit_init()
 * If the previous call was made in too short time this function will sleep
 * until the minimum throttle time between calls has bee reached and then
 * return.
 */
void 
geocode_rate_limit(void) {

  unsigned long t1;
  mtime(&t1);
  const unsigned long diff = t1-last_geocode_api_call;
  if( diff < geocode_rlimit_ms ) {
        logmsg(LOG_DEBUG,"Rate limiting (%lu ms)",geocode_rlimit_ms-diff);
        usleep((geocode_rlimit_ms-diff)*1000); 
  }
  (void)mtime(&last_geocode_api_call);
}

static _Bool send_mail_on_quota=1;
#define MAX_KEYPAIRS 16

/**
 * Send mail using the mail_quotalimit template. This is sent (if enabled in the config)
 * when the API limit in Google has been reached.
 */
void
send_mail_quotalimit(void) {
    if (send_mail_on_quota) {
        struct keypairs *keys = new_keypairlist(MAX_KEYPAIRS);
        char valBuff[1024];
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

        char subjectbuff[512];
        snprintf(subjectbuff, sizeof (subjectbuff), "%sGoogle location rate limit exceeded",mail_subject_prefix);

        if (-1 == send_mail_template(subjectbuff, daemon_email_from, send_mailaddress, "mail_quotalimit", keys, keyIdx, MAX_KEYPAIRS, 
                NULL, 0, NULL)) {
            logmsg(LOG_ERR, "Failed to send mail using template \"mail_quotalimit\"");
        }
        logmsg(LOG_INFO, "Sent mail on Google quota limit to \"%s\"", send_mailaddress);
        free_keypairlist(keys, keyIdx);
    }
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
    snprintf(fullPath,sizeof(fullPath),"%s/%s",db_dir,DEFAULT_ADDRESS_GEOCACHE_FILE);
    
    FILE *fp = fopen(fullPath,"w");
    if( NULL == fp ) {
        logmsg(LOG_ERR,"Cannot create address geocache file \"%s\"  ( %d : %s )",fullPath,errno,strerror(errno));
        return -1;
    }
    for( size_t i=0; i < address_cache_idx; ++i ) {
        //xstrtrim_crnl(_cache[i].addr);
        fprintf(fp,"%ld;%s;%s;%s\n",address_cache[i].ts,address_cache[i].lat,address_cache[i].lon, address_cache[i].addr);
    }
    (void)fclose(fp);
    logmsg(LOG_INFO,"Wrote %zu entries to saved address geocache file \"%s\"",address_cache_idx,fullPath);
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
    snprintf(fullPath,sizeof(fullPath),"%s/%s",db_dir,DEFAULT_MINIMAP_GEOCACHE_FILE);
    
    FILE *fp = fopen(fullPath,"w");
    if( NULL == fp ) {
        logmsg(LOG_ERR,"Cannot create minimap geocache file \"%s\"  ( %d : %s )",fullPath,errno,strerror(errno));
        return -1;
    }
    
    // Remove the old directory where all the images are stored
    
    char mapfilename[255];
    for( size_t i=0; i < minimap_cache_idx; ++i ) {
        fprintf(fp,"%ld;%s;%s;%d;%d;%d;%s\n",minimap_cache[i].ts,
                minimap_cache[i].lat,minimap_cache[i].lon, 
                minimap_cache[i].zoom,
                minimap_cache[i].width,minimap_cache[i].height,
                minimap_cache[i].filename);
        
        snprintf(mapfilename,sizeof(mapfilename),"%s/%s/%s",db_dir,DEFAULT_MINIMAP_GEOCACHE_DIR,minimap_cache[i].filename);

        mode_t mode = S_IRUSR |S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        int fd = open(mapfilename,O_CREAT|O_RDWR,mode);
        if( fd < 0 ) {
            logmsg(LOG_ERR,"Failed to open minimap for writing. Aborting writing cache file \"%s\" ( %d : %s)",
                   mapfilename,errno,strerror(errno));
            fclose(fp);
            return -1;
        }
        int rc=write(fd,minimap_cache[i].imgdata,minimap_cache[i].imgdatasize);
        close(fd);
        if( -1 == rc ) {
            logmsg(LOG_ERR,"Failed to write minimap image. ( %d : %s )",errno,strerror(errno));
            fclose(fp);
            return -1;
        }
        
    }
    
    (void)fclose(fp);
    logmsg(LOG_INFO,"Wrote %zu entries to saved minimap geocache file \"%s\"",minimap_cache_idx,fullPath);
    return 0;
}

/**
 * Read saved address geo cache from file
 * @return 0 on success, -1 on failure
 */
int
read_address_geocache(void) {
    char fullPath[256];
    snprintf(fullPath,sizeof(fullPath),"%s/%s",db_dir,DEFAULT_ADDRESS_GEOCACHE_FILE);

    FILE *fp = fopen(fullPath,"r");
    if( NULL == fp ) {
        if( errno == ENOENT ) {
            logmsg(LOG_INFO,"No saved geo cache file exist.");
            return 0;
        } else {
            logmsg(LOG_ERR,"Failed trying to read saved geo cache file \"%s\" ( %d : %s )",fullPath,errno,strerror(errno));
            return -1;
        }
    } else {
        // Read file line by line
        char lbuff[256];
        struct splitfields fields;
        address_cache_idx=0;
        double lat,lon;
        while( fgets(lbuff, sizeof(lbuff)-1, fp) ) {
            
            // Get rid of trailing newlines
            xstrtrim_crnl(lbuff);           
            xstrsplitfields(lbuff,';',&fields);
            
            if( 4 == fields.nf ) {
                lat = xatof(fields.fld[1]);
                lon = xatof(fields.fld[2]);
                if( lat < 1.0 || lat > 89.0 || lon < 1.0 || lon > 89.0 || strnlen(fields.fld[3],255) < 10 ) {
                    logmsg(LOG_ERR,"Address geocache file invalid. Reading aborted");
                    address_cache_idx=0;
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
        logmsg(LOG_INFO,"Read %zu entries from geo cache file \"%s\"",address_cache_idx,fullPath);
        cache_stats[CACHE_ADDR].cache_max_idx = address_cache_idx;
        fclose(fp);
        return 0;
    }    
}

/**
 * Read saved address geo cache from file
 * @return 0 on success, -1 on failure
 */
int
read_mininap_geocache(void) {
    char fullPath[256];
    snprintf(fullPath,sizeof(fullPath),"%s/%s",db_dir,DEFAULT_MINIMAP_GEOCACHE_FILE);

    FILE *fp = fopen(fullPath,"r");
    if( NULL == fp ) {
        if( errno == ENOENT ) {
            logmsg(LOG_INFO,"No saved minimap geo cache file exist.");
            return 0;
        } else {
            logmsg(LOG_ERR,"Failed trying to read saved minimap geo cache file \"%s\" ( %d : %s )",fullPath,errno,strerror(errno));
            return -1;
        }
    } else {
        // Read file line by line
        char lbuff[256];
        struct splitfields fields;
        minimap_cache_idx=0;
        double lat,lon;
        while( fgets(lbuff, sizeof(lbuff)-1, fp) ) {
            
            // Get rid of trailing newlines
            xstrtrim_crnl(lbuff);           
            xstrsplitfields(lbuff,';',&fields);
            
            if( 7 == fields.nf ) {
                lat = xatof(fields.fld[1]);
                lon = xatof(fields.fld[2]);
                if( lat < 0.0 || lat > 90.0 || lon < 0.0 || lon > 90.0  ) {
                    logmsg(LOG_ERR,"Minimap geocache file invalid. Reading aborted");
                    minimap_cache_idx=0;
                    fclose(fp);
                    return -1;
                } else {                
                    char mapfilename[255];
                    snprintf(mapfilename,sizeof(mapfilename),"%s/%s/%s",db_dir,DEFAULT_MINIMAP_GEOCACHE_DIR,fields.fld[6]);
                    
                    int rc=read_file_buffer(mapfilename,
                            & minimap_cache[minimap_cache_idx].imgdatasize,
                            & minimap_cache[minimap_cache_idx].imgdata);
                    
                    if( 0==rc) {
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
                        logmsg(LOG_ERR,"Failed to read minimap image file from disk \"%s\"",
                               minimap_cache[minimap_cache_idx].filename);
                    }
                }          
                
            } else {
                
                logmsg(LOG_ERR,"Invalid line in saved minimap cache file ignoring line");
            }
            

        }
        logmsg(LOG_INFO,"Read %zu entries from geo cache file \"%s\"",minimap_cache_idx,fullPath);
        cache_stats[CACHE_MINIMAP].cache_max_idx = minimap_cache_idx;
        fclose(fp);
        return 0;
    }    
}


/**
 * Check if position is in the cache. In that case return <> 0 and store
 * address in the location pointed to by addr with maximum size maxlen
 * @param lat
 * @param lon
 * @param addr
 * @return 1 if found 0 if not found
 */
int
in_address_cache(char *lat, char *lon, char *addr, size_t maxlen) {
    // Search for match of lon & lat
    size_t idx = 0;

    const double dLat = atof(lat);
    const double dLon = atof(lon);
    while (idx < GEOCACHE_ADDRESS_SIZE && address_cache[idx].lat) {
        if( address_lookup_proximity > 0 ) {
            const double dist = gpsdist_m(dLat, dLon, address_cache[idx].dLat, address_cache[idx].dLon);
            if( dist <= (double)address_lookup_proximity) {
                xmb_strncpy(addr, address_cache[idx].addr, maxlen);
                logmsg(LOG_INFO, "Geocache address approx HIT for (%s,%s) -> \"%s\" distance=%.0f from (%.6f,%.6f)", 
                        lat, lon, addr, dist,
                        address_cache[idx].dLat, 
                        address_cache[idx].dLon);
                update_cache_stat(CACHE_ADDR,TRUE,address_cache_idx);
                return TRUE;                
            }
        } else {
            // Use strcmp() to avoid conversion floating point problems 
            if (0 == strcmp(lat, address_cache[idx].lat) && 0 == strcmp(lon, address_cache[idx].lon)) {
                xmb_strncpy(addr, address_cache[idx].addr, maxlen);
                logmsg(LOG_INFO, "Geocache address HIT (%s,%s) -> \"%s\"", lat, lon, addr);
                update_cache_stat(CACHE_ADDR,TRUE,address_cache_idx);
                return TRUE;
            }
        }
        idx++;
    }
    
    update_cache_stat(CACHE_ADDR,FALSE,address_cache_idx);
    return FALSE;
}

/**
 * Check if the requested minimap exists in the cache. This is a two stage proess. Firs
 * we check if it is in the memory vector to. If it is the  we construct the filename
 * from the parameters and check if it is in the cache directory. If it is then we read the
 * image by allocating storage.
 * @param lat Latitude
 * @param lon Longitude
 * @param zoom Zoom factor
 * @param width width of image
 * @param height height of image
 * @param[out] imgdata An allocated memory areas where the imagedata is stored. It is
 * the calling routines responsibility to free the memory
 * @param[out] imgsize The size of the image data
 * @return 1 if the image was found, 0 otherwise 
 */
int
in_minimap_cache(const char *lat, const char *lon, int zoom, int width, int height, char **imgdata, size_t *imgsize) {
    // Search for match of lon & lat
    size_t idx = 0;

    const double dLat = atof(lat);
    const double dLon = atof(lon);
    _Bool found = FALSE;
    
    while (idx < GEOCACHE_MINIMAP_SIZE && minimap_cache[idx].lat) {        
        
        if( zoom != minimap_cache[idx].zoom || minimap_cache[idx].width != width || minimap_cache[idx].height != height ) {
            idx++;
            continue;
        }

	// For the overview map we use the same map for even larger approximate distances
	// from the "true" center since at this scale it requires multiple time longer distance
	// to make any difference in practice in the images. The scale difference in the images
	// is roughly 50 times (1:10 000 to 1:200) for the default zoom=9 and zoom=15.
	// to be on the "safe" side we don't use a factor of 50 but a factor of 25
	// For the default proximity of 20m it means that we must move 500m for the overview map
	// to be changed.
	int proximity_dist = zoom==minimap_overview_zoom ? address_lookup_proximity * 25 : address_lookup_proximity;
        if( proximity_dist > 0 ) {
            const double dist = gpsdist_m(dLat, dLon, minimap_cache[idx].dLat, minimap_cache[idx].dLon);

            if(  dist <= (double)proximity_dist ) {                
                found = TRUE;
                logmsg(LOG_INFO, "Minimap geocache approx HIT for (%s,%s), distance %.0f m to (%.6f,%.6f). File \"%s\"", 
                        lat, lon, dist, minimap_cache[idx].dLat, minimap_cache[idx].dLon,
                        minimap_cache[idx].filename);                
            }

        } else {            

            if (0 == strcmp(lat, minimap_cache[idx].lat) && 0 == strcmp(lon, minimap_cache[idx].lon)) {
                found = TRUE;
                logmsg(LOG_INFO, "Minimap geocache HIT (%s,%s). File \"%s\"", lat, lon, minimap_cache[idx].filename );
            }

        }
                
        if( found ) {
            *imgdata = minimap_cache[idx].imgdata;
            *imgsize = minimap_cache[idx].imgdatasize;
            update_cache_stat(CACHE_MINIMAP,TRUE,minimap_cache_idx);
            return TRUE;
        }
        
        idx++;
    }   
    update_cache_stat(CACHE_MINIMAP,FALSE,minimap_cache_idx);    
    return FALSE;
}

/**
 * Updated the in-memory minimap cache
 * @param lat
 * @param lon
 * @param zoom
 * @param width
 * @param height
 * @param imgdatasize
 * @param imgdata
 * @return 0 on success, -1 on failure
 */
int
update_minimap_cache(const char *lat, const char *lon, int zoom, int width, int height, size_t imgdatasize, char *imgdata) {
    
    logmsg(LOG_DEBUG, "Updating minimap geocache [idx=%zu] (%s,%s) [%d,%dx%d]", minimap_cache_idx, lat, lon, zoom, width,height);
        
    const double dLat = atof(lat);
    const double dLon = atof(lon);   
         
    char filename[255];
    int rc = get_minimap_cache_filename(filename, sizeof(filename), 
                                        lat, lon, zoom, width, height);

    if( rc ) {
        logmsg(LOG_ERR,"Failed to create minimap filename");
        return -1;
    }
    
    if( minimap_cache[minimap_cache_idx].lat ) {
        
        // We are reusing an older cache entry
        minimap_cache[minimap_cache_idx].lat = realloc(minimap_cache[minimap_cache_idx].lat, strlen(lat) + 1);
        minimap_cache[minimap_cache_idx].lon = realloc(minimap_cache[minimap_cache_idx].lon, strlen(lon) + 1);
        strcpy(minimap_cache[minimap_cache_idx].lat, lat);
        strcpy(minimap_cache[minimap_cache_idx].lon, lon);
        
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

    if (minimap_cache_idx >= GEOCACHE_MINIMAP_SIZE) {
        logmsg(LOG_DEBUG, "Wrapping minimap geocache index at end [idx=%zu]", minimap_cache_idx);
        minimap_cache_idx = 0;
    }
    return 0;
}

/**
 * Updated the cache with new value
 * @param lat
 * @param lon
 * @param addr
 * @return 0 on success, -1 on failure
 */
int
update_address_cache(char *lat, char *lon, char *addr) {
    logmsg(LOG_DEBUG, "Updating address geocache [idx=%zu] (%s,%s) -> \"%s\"", address_cache_idx, lat, lon, addr);
    const double dLat = atof(lat);
    const double dLon = atof(lon);    
    if (address_cache[address_cache_idx].lat) {
        address_cache[address_cache_idx].lat = realloc(address_cache[address_cache_idx].lat, strlen(lat) + 1);
        address_cache[address_cache_idx].lon = realloc(address_cache[address_cache_idx].lon, strlen(lon) + 1);
        address_cache[address_cache_idx].addr = realloc(address_cache[address_cache_idx].addr, strlen(addr) + 1);
        strcpy(address_cache[address_cache_idx].addr, addr);
        strcpy(address_cache[address_cache_idx].lat, lat);
        strcpy(address_cache[address_cache_idx].lon, lon);
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

    if (address_cache_idx >= GEOCACHE_ADDRESS_SIZE) {
        logmsg(LOG_DEBUG, "Wrapping address geocache index at end [idx=%zu]", address_cache_idx);
        address_cache_idx = 0;
    }
    return 0;
}


/**
 * Get the Google geocode service URL string
 * @param lat
 * @param lon
 * @return A pointer to a static location for the URL to the Google service
 */
char *
get_geocode_url(char *lat, char *lon) {
    static char url[512];

    if (strlen(google_api_key) > 15) {
        // Assume this is a valid key
        // location type ROOFTOP means that we want a street level address
        snprintf(url, sizeof (url),
                "https://maps.googleapis.com/maps/api/geocode/xml?latlng=%s,%s&location_type=ROOFTOP&key=%s",
                lat, lon, google_api_key);
        
    } else {
        // Anonymous lookup. WIthout a key we are not allowed to set location_type
        snprintf(url, sizeof (url), "https://maps.googleapis.com/maps/api/geocode/xml?latlng=%s,%s", lat, lon);
    }

    return url;
}


/**
 * Get the Google static map service URL string
 * @param lat Latitude
 * @param lon Longitude
 * @param zoom Zoom factor 1-20
 * @param size Image size as width x height, for example "200x200"
 * @return A pointer to a static location for the URL to the Google static map service centered at the specified 
 * coordinates with specified zoom factor
 */
char *
get_staticmap_url(const char *lat, const char *lon, unsigned short zoom, int width, int height) {
    static char url[512];
    const char *msize = "normal";    // Normal marker size
    const char *mcolor = "0x990000"; // Dark red marker

    if (strlen(google_api_key) > 15) {
        snprintf(url, sizeof (url),
                "https://maps.googleapis.com/maps/api/staticmap?markers=size:%s|color:%s|%s,%s&zoom=%u&size=%dx%d&key=%s",
                msize,mcolor,lat, lon, zoom, width, height, google_api_key);
        
    } else {
        snprintf(url, sizeof (url),
                "https://maps.googleapis.com/maps/api/staticmap?markers=size:%s|color:%s|%s,%s&zoom=%u&size=%dx%d",
                msize,mcolor,lat, lon, zoom, width, height);
    }
    
    logmsg(LOG_DEBUG,"Map URL: \"%s\"",url);

    return url;
}


/**
 * Parse a single XML result element in the service reply. If this result
 * element is a street address result then store the formatted address in
 * the address out argument.
 * @param node
 * @param address
 * @param maxlen
 * @return 0 on success, -1 on failure to find street address
 */
static int
parse_result_node(xmlNodePtr node, char *address, size_t maxlen) {

    xmlNodePtr start = node->xmlChildrenNode;
    xmlNodePtr cur = start;
    _Bool is_street_address = 0;

    // We first check the type. We are looking for the street address result
    // Look for result that is of <type>Street address</type>
    while (NULL != cur && !is_street_address) {
        if (xmlStrcmp(cur->name, xml_text)) { // Ignore all implicit text nodes
            if (xmlStrcmp(cur->name, resp_xml_type) == 0) {
                if (xmlStrcmp(cur->xmlChildrenNode->content, resp_xml_street_address) == 0) {
                    is_street_address = 1;
                    break;
                }
            }
        }
        cur = cur->next;
    }

    if (!is_street_address) {
        logmsg(LOG_ERR,"No street address available. Trying to find postal code");
        // If the location doesn't have a street address then try to find a postal code
        cur = start;
        _Bool is_postal_code = 0;
        while (NULL != cur && !is_postal_code) {
            if (xmlStrcmp(cur->name, xml_text)) { // Ignore all implicit text nodes
                if (xmlStrcmp(cur->name, resp_xml_type) == 0) {
                    logmsg(LOG_DEBUG,"Result node: %s",(char *)cur->xmlChildrenNode->content);
                    if (xmlStrcmp(cur->xmlChildrenNode->content, resp_xml_postal_code) == 0) {
                        is_postal_code = 1;
                        break;
                    }
                }
            }
            cur = cur->next;
        }
        
        // Give up if we can't even find the postal code
        if( !is_postal_code )
            return -1;
    }

    // We now have now established that the result node is either a street address node
    // of a postal code. o what now remains is to get the formatted address.
    // We start from the beginning of the nodes since we have no guarantee that
    // the formatted address will always come after the type (even though it always
    // seems so)

    cur = start;

    while (NULL != cur) {
        if (xmlStrcmp(cur->name, xml_text)) { // Ignore all implicit text nodes
            if (xmlStrcmp(cur->name, resp_xml_formatted_address) == 0) {
                xmb_strncpy(address, (char *) cur->xmlChildrenNode->content, maxlen);
                return 0;
            }
        }
        cur = cur->next;
    }
    
    logmsg(LOG_ERR,"Could not find formatted street address or postal code in geo lookup result");
    return -1;
}


/**
 * Extract the street address from the reply from the service call
 * @param chunk
 * @param address
 * @param maxlen
 * @return 0 on success, -1 on general failure, 
 *     0    xml_OK,
 *   -11    xml_OverQueryLimit,
 *   -12    xml_ZeroResults,
 *   -13    xml_InvalidRequest,
 *   -14    xml_RequestDenied,
 *   -15    xml_UnknownError
 */
int
parse_reply(char *reply, char *address, size_t maxlen) {
    xmlNodePtr root;
    xmlNodePtr node;
    xmlDocPtr doc;

    *address = '\0';

    doc = xmlParseDoc((xmlChar *) reply);
    if (NULL == doc) {
        logmsg(LOG_ERR, "Service reply is not a valid XML document. ( %d : %s )", errno, strerror(errno));
        xmlCleanupParser();
        return -1;
    }

    /* Get root element */
    root = xmlDocGetRootElement(doc);

    /* Verify that this is the expected root for a reply*/
    if (xmlStrcmp(root->name, resp_xml_root)) {
        logmsg(LOG_ERR, "Service reply is not a valid document. Wrong XML root element. Found '%s' when expecting '%s'",
                root->name, resp_xml_root);
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return -1;
    }

    // Setup to parse all nodes
    node = root->xmlChildrenNode;

    // Find the status element and check status
    while (node != NULL) {
        if (xmlStrcmp(node->name, xml_text)) { // Ignore all implicit text nodes
            if (0 == xmlStrcmp(node->name, resp_xml_status)) { // Look for <status>OK</status>
                int i=0;
                while( i < NUM_STATUS_CODES && 
                       xmlStrcmp(node->xmlChildrenNode->content, (xmlChar *)status_codes[i])) {
                    ++i;
                }
                if( 0 == i )
                    break;
                
                xmlFreeDoc(doc);
                xmlCleanupParser();                
                
                if( i < NUM_STATUS_CODES ) {
                    logmsg(LOG_DEBUG,"Geo lookup error: %s", status_codes[i]);
                    return -(10+i);
                } else {
                    logmsg(LOG_DEBUG,"Geo lookup error: UNKNOWN ERROR");
                    return -1;
                }
                
                
            }
        }
        node = node->next;
    }

    // Find the formatted result element we are looking for
    node = root->xmlChildrenNode;

    // Loop through all the results until we find the street address result
    while (node != NULL) {
        if (xmlStrcmp(node->name, xml_text)) { // Ignore all implicit text nodes
            if (0 == xmlStrcmp(node->name, resp_xml_result)) { // Look for <result>....</result>
                if (0 == parse_result_node(node, address, maxlen)) {
                    xmlFreeDoc(doc);
                    xmlCleanupParser();
                    return 0;
                }
            }
        }
        node = node->next;
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();    
    FILE *fp=fopen("/tmp/failed_georeply.txt","w");
    if( fp ) {
        fprintf(fp,"%s",reply);
        fclose(fp);
        logmsg(LOG_DEBUG,"Wrote failed geo reply to \"/tmp/failed_georeply.txt\" (Address=%s)",address);       
    }

    return -1;
}


/**
 * Callback for curl library. This callback can be called by curl as many
 * times as needed to store the read data (service reply) in a memory buffer.
 * @param contents
 * @param size
 * @param nmemb
 * @param userp
 * @return The size of the data we have handled
 */
static size_t
_cb_curl(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct memoryStruct *mem = (struct memoryStruct *) userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        logmsg(LOG_ERR, "Not enough memory in CURL callback (realloc returned NULL) ( %d : %s )\n", errno, strerror(errno));
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}


/**
 * Do service call to the Google open API to do a reverse lookup of the
 * coordinates to get a street address
 * @param lat Latitude as string in decimal form, e.g 43.128318
 * @param lon Longitude as string in decimal form e.g. 19.553268
 * @param address Where to store the formatted address
 * @param maxlen Maximum size of address buffer
 * @return 0 on success, -1 on failure
 */
static int
_lookup_address_from_latlon(char *lat, char *lon, char *address, size_t maxlen) {
      
#ifdef SIMULATE_GOOGLE_API_CALL    
    geocode_rate_limit();
    logmsg(LOG_DEBUG,"Simulated Google API Call");
    xstrlcpy(address,"Simulated address",maxlen);
    return 0;
#endif   
    
    CURL *curl_handle;
    CURLcode res;

    if (address)
        *address = '\0';
    else
        return -1;

    // First check if this is already in the cache
    if (in_address_cache(lat, lon, address, maxlen)) {
        return 0;
    }


    struct memoryStruct chunk;

    chunk.memory = malloc(1); /* will be grown as needed */
    chunk.size = 0; /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, get_geocode_url(lat, lon));

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, _cb_curl);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &chunk);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* We must limit the rate of API calls */
    geocode_rate_limit();
    res = curl_easy_perform(curl_handle);

    int rc = 0;

    /* check for errors */
    if (res != CURLE_OK) {
        logmsg(LOG_ERR, "Could not get service reply from Google: %s", curl_easy_strerror(res));
        rc = -1;
    } else {
        if ( (rc=parse_reply(chunk.memory, address, maxlen - 1)) ) {
            if( rc == -1 ) {
                logmsg(LOG_ERR, "Failed to parse service reply for address (lat=%s, lon=%s)", lat, lon);
            } else {
              // Parse successful but an error message was returned
	      // Rate limit exceeded or some other Google lookup error. 
              // Most likely this means we have exceeded the 24h limit
	      // Error message already sent to client
            }            
        }
    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);

    if (chunk.memory)
        free(chunk.memory);

    /* we're done with libcurl, so clean it up */
    curl_global_cleanup();

    /* On success update the cache */
    if (0 == rc) {
        update_address_cache(lat, lon, address);
    }

    return rc;
}

/**
 * Internal helper function to do service call to the Google static map API to 
 * get a small static map as a PNG image centered around the given coordinates
 * @param lat Latitude as string in decimal form, e.g 43.128318
 * @param lon Longitude as string in decimal form e.g. 19.553268
 * @param imagedata A buffer that is allocated to store the image data. It is the calling
 * functions responsibility to free this buffer after it has been used
 * @param zoom The zoom factor for the fetched map
 * @param size The image size specified as a string "WxH", for example "200x200"
 * @return 0 on success, -1 on failure
 */
static int
_get_minimap_from_latlon(const char *lat, const char *lon, unsigned short zoom, int width, int height, char **imagedata, size_t *datasize) {
    
    CURL *curl_handle;
    CURLcode res;

    // First check if this is already in the cache
    if (in_minimap_cache(lat, lon, zoom, width, height, imagedata, datasize)) {
        return 0;
    }
    
    
    struct memoryStruct chunk;

    chunk.memory = malloc(1); /* will be grown as needed */
    chunk.size = 0; /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, get_staticmap_url(lat,lon,zoom,width,height));

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, _cb_curl);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &chunk);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* We must limit the rate of API calls */
    staticmap_rate_limit();
    res = curl_easy_perform(curl_handle);

    int rc = 0;

    /* check for errors */
    if (res != CURLE_OK) {
        logmsg(LOG_ERR, "Could not complete static map service from Google: %s", curl_easy_strerror(res));
        free(chunk.memory);
        rc = -1;
    } else {
        *imagedata = chunk.memory;
        *datasize = chunk.size;
        
        // Check for a correct PNG header. All valid PNG files has the magic sequence
        // 89 50 4e 47 0d 0a 1a 0a as the first 8 bytes
        static unsigned char png_header[8] = {
            0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
        };

	const size_t check_len = sizeof(png_header);
	size_t check_cnt = 0;
	while( check_cnt < check_len && ((*(chunk.memory+check_cnt))&0xff) == png_header[check_cnt] ) {
            check_cnt++;
	}
        
	if( check_cnt == check_len && chunk.size > 700 ) {
            update_minimap_cache(lat, lon, zoom, width, height, chunk.size, chunk.memory);
	} else {        
            // The reply is not valid PNG image
            chunk.memory[chunk.size-1]='\0'; // Make sure it is zero terminated            
            // If the reply is less than 500 bytes and the first characters seems to be printable assume this is a error message
            if( chunk.size < 500 && 
                isalpha(*(chunk.memory+0)) && isalpha(*(chunk.memory+1)) && isalpha(*(chunk.memory+2)) && isalpha(*(chunk.memory+3)) ) {
                logmsg(LOG_ERR, "Cannot fetch static Google map. Error: \"%s\"",chunk.memory);
            } else {
                logmsg(LOG_ERR, "Cannot fetch static Google map. Not a valid PNG image and does not seem like text.");
            }
            free(chunk.memory);
            rc = -1;
        } 
    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);

    /* we're done with libcurl, so clean it up */
    curl_global_cleanup();

    return rc;
}


static _Bool        staticmap_rate_24h_exceeded = 0; // Have the 24h rate been exceeded ?
static time_t       staticmap_rate_24h_wait_until = 0 ; // Wait until this timestamp to allow calls again?
static unsigned     staticmap_rate_24h_try = 0; // We try three times until concluding that the 24h limit has been reached


static _Bool        geocode_rate_24h_exceeded = 0; // Have the 24h rate been exceeded ?
static time_t       geocode_rate_24h_wait_until = 0 ; // Wait until this timestamp to allow calls again?
static unsigned     geocode_rate_24h_try = 0; // We try three times until concluding that the 24h limit has been reached

/**
 * REset rate limit
 */
void
staticmap_rate_limit_reset(void) {
    staticmap_rate_24h_exceeded = 0;
    staticmap_rate_24h_wait_until = 0;
    staticmap_rate_24h_try = 0;
}

/**
 * Reset rate limits
 */
void
geocode_rate_limit_reset(void) {
    geocode_rate_24h_exceeded = 0;
    geocode_rate_24h_wait_until = 0;
    geocode_rate_24h_try = 0;
}

/**
 * Create a cache filename from the details of the static google map image
 * @param cfname Pointer to storage for the created filename
 * @param maxlen Maximum size of filename
 * @param lat Latitude of image center
 * @param lon Longitude of imagae center
 * @param zoom Zoom factor
 * @param size Size string
 * @return  0 on success, -1 on failure
 */
int
get_minimap_cache_filename(char *cfname, size_t maxlen, const char *lat, const char *lon, int zoom, int width, int height) {

    // The name is created by combining the prefix "map_" first 6 digit from the lat,lon, the zoom factor (as 2 digits) and t¨
    // the size as a string
    // For example "map_571293_174361_15_200x200.png"
    
    char blat[16], blon[16];
    char *plat=blat;
    char *plon=blon;
    
    // Extract the part before the decimal comma
    while(*lat && *lat != '.') *plat++ = *lat++;
    while(*lon && *lon != '.') *plon++ = *lon++;
    
    // Read past the '.'
    lat++; 
    lon++;
    
    const int precision = 6;    
    for(int i=0; i < precision; i++) {
        *plat++ = *lat++;
        *plon++ = *lon++;
    }
    *plat='\0';
    *plon='\0';
    
    char fname[256];
    snprintf(fname,sizeof(fname),"map_%s_%s_%d_%dx%d.png",blat,blon,zoom,width,height);
    if( strlen(fname) > maxlen )
        return -1;
    strcpy(cfname,fname);
    return 0;
}


/**
 * Do service call to the Google static map API to get a small static map as a 
 * PNG image centered around the given coordinates
 * @param lat Latitude as string in decimal form, e.g 43.128318
 * @param lon Longitude as string in decimal form e.g. 19.553268
 * @param imagedata A buffer that is allocated to store the image data. It is the calling
 * functions responsibility to free this buffer after it has been used
 * @param zoom The zoom factor for the fetched map
 * @param imgsize The image size specified as a string "WxH", for example "200x200"
 * @return 0 on success, Negative error status code otherwise
 */
int
get_minimap_from_latlon(const char *lat, const char *lon, unsigned short zoom, int width, int height, char **imagedata, size_t *datasize) {
    if (staticmap_rate_24h_exceeded) {
        time_t ts = time(NULL);
        if (ts < staticmap_rate_24h_wait_until) {
            unsigned diff = staticmap_rate_24h_wait_until - ts;
            unsigned h = diff / 3600;
            unsigned m = (diff - h * 3600) / 60;
            logmsg(LOG_ERR, "Static map failed. Rate limit exceeded, further API calls blocked another %02u:%02u hours", h, m);
            return GOOGLE_STATUS_OVERQUOTA;
        } else {
            staticmap_rate_24h_exceeded = 0;
            staticmap_rate_24h_try = 0;
        }
    }
    return _get_minimap_from_latlon(lat, lon, zoom, width, height, imagedata, datasize);
}

/**
 * Do service call to the Google open API to do a reverse lookup of the
 * coordinates to get a street address
 * @param lat Latitude as string in decimal form, e.g 43.128318
 * @param lon Longitude as string in decimal form e.g. 19.553268
 * @param address Where to store the formatted address
 * @param maxlen Maximum size of address buffer
 * @return 0 on success, Negative error status code otherwise
 */
int
get_address_from_latlon(char *lat, char *lon, char *address, size_t maxlen) {
    
    if( geocode_rate_24h_exceeded ) {
        time_t ts=time(NULL);
        if( ts < geocode_rate_24h_wait_until ) {
            unsigned diff = geocode_rate_24h_wait_until-ts;
            unsigned h=diff/3600;
            unsigned m=(diff-h*3600)/60;
            logmsg(LOG_ERR,"Reverse lookup failed. Rate limit exceeded, further API calls blocked another %02u:%02u hours",h,m);
            xstrlcpy(address, "(?)", maxlen);
            return GOOGLE_STATUS_OVERQUOTA;
        } else {
            geocode_rate_24h_exceeded = 0;
            geocode_rate_24h_try = 0 ;
        }
    }
    
    int rc;
    if (0 == (rc=_lookup_address_from_latlon(lat, lon, address, maxlen)) ) {
        logmsg(LOG_INFO, "Geolocation lookup: (%s,%s) -> \"%s\"", lat, lon, address);
    } else if( GOOGLE_STATUS_OVERQUOTA == rc ) {
        // We wait two seconds and try again in case this is only a temporary failure. Temporary failure happens if
        // we flood the Google lookup with too many lookups in s short period of time
        logmsg(LOG_NOTICE,"Query limit exceeded. Waiting 2s for retry");
        sleep(2);
        if (0 == (rc=_lookup_address_from_latlon(lat, lon, address, maxlen)) ) {
            logmsg(LOG_INFO, "Geolocation lookup [2:nd try]: (%s,%s)->\"%s\"", lat, lon, address);
            return 0;
        } 
        
        /*
         // This is commented out since in practice we never need to try more than twice to be sure
         // this is not a temporary problem
         else if( GOOGLE_STATUS_OVERQUOTA == rc ) {
            // We wait three seconds and try again in case this is only a temporary failure. 
            logmsg(LOG_NOTICE,"Query limit exceeded. Waiting 2s for retry");
            sleep(3);
            if (0 == (rc=_lookup_address_from_latlon(lat, lon, address, maxlen)) ) {
                logmsg(LOG_INFO, "Geolocation lookup [3:rd try]: (%s,%s)->\"%s\"", lat, lon, address);
                return 0;
            } 
        } 
         */
    }

   
    if( rc ) {
        if( rc < -10  ) {
            // Specific error status from Google lookup API
            int idx = rc+10;
            idx = -idx;
            const char *serr;
            if( idx < NUM_STATUS_CODES )
                serr = status_codes[idx];
            else
                serr = "UNKNOWN ERROR";
            logmsg(LOG_ERR, "Reverse geolocation failed for (%s,%s) ( \"%s\" : %d ) ", lat, lon, serr, rc);

            if( GOOGLE_STATUS_OVERQUOTA == rc ) {
                if( FAILED_API_CALLS_BEFORE_BLOCKING_24H <= geocode_rate_24h_try) {
                    geocode_rate_24h_exceeded = 1;
                    geocode_rate_24h_wait_until = time(NULL) + (3600*24);
                    logmsg(LOG_ERR,"Geo lookup 24h limit reached. Blocking further calls for 24 hours");
                    send_mail_quotalimit();
                } else {
                    ++geocode_rate_24h_try;
                }
                xstrlcpy(address, "(?)", maxlen);
            } else {        
                xstrlcpy(address, "?", maxlen);
            }
        } else {
            // Generic error
            logmsg(LOG_ERR, "Reverse geolocation failed for (%s,%s)", lat, lon);
            xstrlcpy(address, "?", maxlen);
        }
    }
    
    return rc;
}

/* EOF */
