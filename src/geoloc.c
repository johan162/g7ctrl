/* =========================================================================
 * File:        geoloc.c
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

// We want the full POSIX and C99 standard
#define _GNU_SOURCE

// Standard defines
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <sys/syslog.h>
#include <sys/fcntl.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/xmlreader.h>

#include "config.h"
#include "logger.h"
#include "geoloc.h"
#include "gpsdist.h"
#include "g7config.h"
#include "libxstr/xstr.h"
#include "utils.h"
#include "dict.h"
#include "mailutil.h"
#include "futils.h"
#include "geoloc_cache.h"

static const xmlChar *resp_xml_root = (xmlChar *) "GeocodeResponse";
static const xmlChar *resp_xml_status = (xmlChar *) "status";
static const xmlChar *resp_xml_result = (xmlChar *) "result";
static const xmlChar *resp_xml_type = (xmlChar *) "type";
static const xmlChar *xml_text = (xmlChar *) "text";
static const xmlChar *resp_xml_street_address = (xmlChar *) "street_address";
static const xmlChar *resp_xml_postal_code = (xmlChar *) "postal_code";
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


#define TRUE 1
#define FALSE 0

// Keep track of the relative time we made the last callt o the
// Google API to be able to throttle the calls not to exceed the
// Google limit.
static unsigned long last_geocode_api_call = 0;
static unsigned long last_staticmap_api_call = 0;

// This gives around 8 lookups/sec at maximum
static unsigned long geocode_rlimit_ms = GOOGLE_ANONYMOUS_RLIMIT_MS;
static unsigned long staticmap_rlimit_ms = GOOGLE_ANONYMOUS_RLIMIT_MS;

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
    const unsigned long diff = t1 - last_staticmap_api_call;
    if (diff < staticmap_rlimit_ms) {
        logmsg(LOG_DEBUG, "Rate limiting (%lu ms)", staticmap_rlimit_ms - diff);
        usleep((staticmap_rlimit_ms - diff)*1000);
    }
    (void) mtime(&last_staticmap_api_call);
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
    const unsigned long diff = t1 - last_geocode_api_call;
    if (diff < geocode_rlimit_ms) {
        logmsg(LOG_DEBUG, "Rate limiting (%lu ms)", geocode_rlimit_ms - diff);
        usleep((geocode_rlimit_ms - diff)*1000);
    }
    (void) mtime(&last_geocode_api_call);
}

static _Bool send_mail_on_quota = 1;

/**
 * Send mail using the mail_quotalimit template. This is sent (if enabled in the config)
 * when the API limit in Google has been reached.
 */
void
send_mail_quotalimit(void) {
    if (send_mail_on_quota) {
        dict_t rkeys = new_dict();
        char valBuff[1024];

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

        char subjectbuff[512];
        snprintf(subjectbuff, sizeof (subjectbuff), SUBJECT_LOCRATEEXCEEDED, mail_subject_prefix);

        if (-1 == send_mail_template(subjectbuff, daemon_email_from, send_mailaddress, "mail_quotalimit",
                rkeys, NULL, 0, NULL)) {
            logmsg(LOG_ERR, "Failed to send mail using template \"mail_quotalimit\"");
        }
        logmsg(LOG_INFO, "Sent mail on Google quota limit to \"%s\"", send_mailaddress);
        free_dict(rkeys);
    }
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
        logmsg(LOG_DEBUG,"G Reverse lookup: %s",url);

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
    const char *msize = "normal"; // Normal marker size
    const char *mcolor = "0x990000"; // Dark red marker

    if (strlen(google_api_key) > 15) {
        snprintf(url, sizeof (url),
                "https://maps.googleapis.com/maps/api/staticmap?markers=size:%s|color:%s|%s,%s&zoom=%u&size=%dx%d&key=%s",
                msize, mcolor, lat, lon, zoom, width, height, google_api_key);

    } else {
        snprintf(url, sizeof (url),
                "https://maps.googleapis.com/maps/api/staticmap?markers=size:%s|color:%s|%s,%s&zoom=%u&size=%dx%d",
                msize, mcolor, lat, lon, zoom, width, height);
    }

    logmsg(LOG_DEBUG, "Map URL: \"%s\"", url);

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
        logmsg(LOG_ERR, "No street address available. Trying to find postal code");
        // If the location doesn't have a street address then try to find a postal code
        cur = start;
        _Bool is_postal_code = 0;
        while (NULL != cur && !is_postal_code) {
            if (xmlStrcmp(cur->name, xml_text)) { // Ignore all implicit text nodes
                if (xmlStrcmp(cur->name, resp_xml_type) == 0) {
                    logmsg(LOG_DEBUG, "Result node: %s", (char *) cur->xmlChildrenNode->content);
                    if (xmlStrcmp(cur->xmlChildrenNode->content, resp_xml_postal_code) == 0) {
                        is_postal_code = 1;
                        break;
                    }
                }
            }
            cur = cur->next;
        }

        // Give up if we can't even find the postal code
        if (!is_postal_code)
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

    logmsg(LOG_ERR, "Could not find formatted street address or postal code in geo lookup result");
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
                int i = 0;
                while (i < NUM_STATUS_CODES &&
                        xmlStrcmp(node->xmlChildrenNode->content, (xmlChar *) status_codes[i])) {
                    ++i;
                }
                if (0 == i)
                    break;

                xmlFreeDoc(doc);
                xmlCleanupParser();

                if (i < NUM_STATUS_CODES) {
                    logmsg(LOG_DEBUG, "Geo lookup error: %s", status_codes[i]);
                    return -(10 + i);
                } else {
                    logmsg(LOG_DEBUG, "Geo lookup error: UNKNOWN ERROR");
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
    FILE *fp = fopen("/tmp/failed_georeply.txt", "w");
    if (fp) {
        fprintf(fp, "%s", reply);
        fclose(fp);
        logmsg(LOG_DEBUG, "Wrote failed geo reply to \"/tmp/failed_georeply.txt\" (Address=%s)", address);
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
    logmsg(LOG_DEBUG, "Simulated Google API Call");
    xstrlcpy(address, "Simulated address", maxlen);
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
    
    logmsg(LOG_ERR, "Calling Google API for address lookup" );
    res = curl_easy_perform(curl_handle);

    int rc = 0;

    /* check for errors */
    if (res != CURLE_OK) {
        logmsg(LOG_ERR, "Could not get service reply from Google: %s", curl_easy_strerror(res));
        rc = -1;
    } else {
        if ((rc = parse_reply(chunk.memory, address, maxlen - 1))) {
            if (rc == -1) {
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
    curl_easy_setopt(curl_handle, CURLOPT_URL, get_staticmap_url(lat, lon, zoom, width, height));

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, _cb_curl);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &chunk);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* We must limit the rate of API calls */
    staticmap_rate_limit();
    
    logmsg(LOG_ERR, "Calling Google API for static map lookup" );
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

        const size_t check_len = sizeof (png_header);
        size_t check_cnt = 0;
        while (check_cnt < check_len && ((*(chunk.memory + check_cnt))&0xff) == png_header[check_cnt]) {
            check_cnt++;
        }

        if (check_cnt == check_len && chunk.size > 700) {
            update_minimap_cache(lat, lon, zoom, width, height, chunk.size, chunk.memory);
        } else {
            // The reply is not valid PNG image
            chunk.memory[chunk.size - 1] = '\0'; // Make sure it is zero terminated            
            // If the reply is less than 500 bytes and the first characters seems to be printable assume this is a error message
            if (chunk.size < 500 &&
                    isalpha(*(chunk.memory + 0)) && isalpha(*(chunk.memory + 1)) && isalpha(*(chunk.memory + 2)) && isalpha(*(chunk.memory + 3))) {
                logmsg(LOG_ERR, "Cannot fetch static Google map. Error: \"%s\"", chunk.memory);
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


static _Bool staticmap_rate_24h_exceeded = 0; // Have the 24h rate been exceeded ?
static time_t staticmap_rate_24h_wait_until = 0; // Wait until this timestamp to allow calls again?
static unsigned staticmap_rate_24h_try = 0; // We try three times until concluding that the 24h limit has been reached


static _Bool geocode_rate_24h_exceeded = 0; // Have the 24h rate been exceeded ?
static time_t geocode_rate_24h_wait_until = 0; // Wait until this timestamp to allow calls again?
static unsigned geocode_rate_24h_try = 0; // We try three times until concluding that the 24h limit has been reached

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

    if (geocode_rate_24h_exceeded) {
        time_t ts = time(NULL);
        if (ts < geocode_rate_24h_wait_until) {
            unsigned diff = geocode_rate_24h_wait_until - ts;
            unsigned h = diff / 3600;
            unsigned m = (diff - h * 3600) / 60;
            logmsg(LOG_ERR, "Reverse lookup failed. Rate limit exceeded, further API calls blocked another %02u:%02u hours", h, m);
            xstrlcpy(address, "(?)", maxlen);
            return GOOGLE_STATUS_OVERQUOTA;
        } else {
            geocode_rate_24h_exceeded = 0;
            geocode_rate_24h_try = 0;
        }
    }

    int rc;
    if (0 == (rc = _lookup_address_from_latlon(lat, lon, address, maxlen))) {
        logmsg(LOG_INFO, "Geolocation lookup: (%s,%s) -> \"%s\"", lat, lon, address);
    } else if (GOOGLE_STATUS_OVERQUOTA == rc) {
        // We wait two seconds and try again in case this is only a temporary failure. Temporary failure happens if
        // we flood the Google lookup with too many lookups in s short period of time
        logmsg(LOG_NOTICE, "Query limit exceeded. Waiting 2s for retry");
        sleep(2);
        if (0 == (rc = _lookup_address_from_latlon(lat, lon, address, maxlen))) {
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


    if (rc) {
        if (rc < -10) {
            // Specific error status from Google lookup API
            int idx = rc + 10;
            idx = -idx;
            const char *serr;
            if (idx < NUM_STATUS_CODES)
                serr = status_codes[idx];
            else
                serr = "UNKNOWN ERROR";
            logmsg(LOG_ERR, "Reverse geolocation failed for (%s,%s) ( \"%s\" : %d ) ", lat, lon, serr, rc);

            if (GOOGLE_STATUS_OVERQUOTA == rc) {
                if (FAILED_API_CALLS_BEFORE_BLOCKING_24H <= geocode_rate_24h_try) {
                    geocode_rate_24h_exceeded = 1;
                    geocode_rate_24h_wait_until = time(NULL) + (3600 * 24);
                    logmsg(LOG_ERR, "Geo lookup 24h limit reached. Blocking further calls for 24 hours");
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
