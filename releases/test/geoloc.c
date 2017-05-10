/* =========================================================================
 * File:        geolocation_google.c
 * Description: Use the Google map API to do reverse geolocation lookup
 *              to translate lat/long to street address
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id$
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

// Compile with
// gcc -std=gnu99 `xml2-config --libs` `curl-config --libs`  geoloc.c


// We want the full POSIX and C99 standard
#define _GNU_SOURCE

// Standard defines
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <malloc.h>

#include <sys/syslog.h>
#include <curl/curl.h>
#include <curl/easy.h>

// XML2 lib headers
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/xmlreader.h>


/**
 * Key needed to use the Google service
 */
#define GOOGLE_API_KEY "AIzaSyCi43OvQu7sFxKPWK9aYNkUM3QocgktWlY"

static const xmlChar *resp_xml_root =                (xmlChar *) "GeocodeResponse";
static const xmlChar *resp_xml_status =              (xmlChar *) "status";
static const xmlChar *resp_xml_result =              (xmlChar *) "result";
static const xmlChar *resp_xml_type =                (xmlChar *) "type";
static const xmlChar *xml_text =                     (xmlChar *) "text";
static const xmlChar *resp_xml_street_address =      (xmlChar *) "street_address";
static const xmlChar *resp_xml_formatted_address =   (xmlChar *) "formatted_address";

static const xmlChar *xml_OK =   (xmlChar *) "OK";

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
 * Get the Google service URL string
 * @param lat
 * @param lon
 * @return A pointer to a static location for the URL to the Google service
 */
char *
get_service_url(char *lat, char *lon) {
    static char url[512];
    /*
    snprintf(url, sizeof (url),
            "https://maps.googleapis.com/maps/api/geocode/xml?latlng=%s,%s&location_type=ROOFTOP&result_type=street_address&key=%s",
            lat, lon, GOOGLE_API_KEY);
     */
    snprintf(url, sizeof (url),
            "https://maps.googleapis.com/maps/api/geocode/xml?latlng=%s,%s",
            lat, lon);
    return url;
}

/**
 * Callback for curl library. This callback can be called by curl as many
 * times as needed to store the read data in a memory buffer.
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
        fprintf(stderr,"Not enough memory (realloc returned NULL) ( %d : %s )\n",errno,strerror(errno));
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}


// curl -o ex_reply.xml https://maps.googleapis.com/maps/api/geocode/xml?latlng=59.366526,1795997&key=AIzaSyCi43OvQu7sFxKPWK9aYNkUM3QocgktWlY
char *
get_xmlbuff(char *name) {

    FILE *fp = fopen(name,"r");
    if( NULL == fp ) {
        logmsg(LOG_ERR,"Failed to open file \"%s\" ( %d : %s)",name,errno,strerr(errno));
        return NULL;
    }
    
    const size_t BUFSIZE=20*1024;
    char *buffer=calloc(BUFSIZE, sizeof(char));
    size_t nread=fread(buffer,1,BUFSIZE-1,fp);
    buffer[nread]='\0';
    return buffer;
}

int
parse_result_node(xmlNodePtr node, char *address, size_t maxlen) {

    xmlNodePtr start = node->xmlChildrenNode;
    xmlNodePtr cur = start;
    _Bool is_street_address = 0;

    // We first check the type. We are looking for the street address result
    while (NULL != cur && !is_street_address) {
        if (xmlStrcmp(cur->name, xml_text)) { // Ignore all implicit text nodes
            if (xmlStrcmp(cur->name, resp_xml_type) == 0) {
                if( xmlStrcmp(cur->xmlChildrenNode->content, resp_xml_street_address) == 0 ) {
                    is_street_address = 1;
                    break;
                }
            }
        }
        cur = cur->next;
    }

    if ( ! is_street_address ) {
        return -1;
    }

    // We now have now established that the result node is a street address node
    // so what now remains is to get the formatted address.
    // We start from the beginning of the nodes since we have no guarantee that
    // the formatted address will always come after the type (even though it always
    // seems so)

    cur = start;

    while (NULL != cur) {
        if (xmlStrcmp(cur->name, xml_text)) { // Ignore all implicit text nodes
            if (xmlStrcmp(cur->name, resp_xml_formatted_address) == 0) {
                strncpy(address, cur->xmlChildrenNode->content, maxlen);
                return 0;
            }
        }
        cur = cur->next;
    }

    return -1;
}

/**
 * Extract the street address from the reply from the service call
 * @param chunk
 * @param address
 * @param maxlen
 * @return 0 on success, -1 on failure
 */
int
parse_reply(char *reply, char *address, size_t maxlen) {
    xmlNodePtr root;
    xmlNodePtr node;
    xmlDocPtr doc;

    *address = '\0';

    doc = xmlParseDoc((xmlChar *)reply);
    if (NULL == doc) {
        fprintf(stderr, "Service reply is not a valid XML document. ( %d : %s )", errno, strerror(errno));
        xmlCleanupParser();
        return -1;
    }

    /* Get root element */
    root = xmlDocGetRootElement(doc);

    /* Verify that this is the expected root for a reply*/
    if (xmlStrcmp(root->name, resp_xml_root)) {
        fprintf(stderr, "Service reply is not a valid document. Wrong XML root element. Found '%s' when expecting '%s'",
                node->name, resp_xml_root);
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return -1;
    }

    // Setup to parse all nodes
    node = root->xmlChildrenNode;

    // Find the status element and check status
    while( node != NULL ) {
        if (xmlStrcmp(node->name, xml_text) ) { // Ignore all implicit text nodes
            if (0 == xmlStrcmp(node->name, resp_xml_status)) {
                if (0 == xmlStrcmp(node->xmlChildrenNode->content, xml_OK) ) {
                    break;
                } else {
                    xmlFreeDoc(doc);
                    xmlCleanupParser();
                    return -2;
                }
            }
        }
        node=node->next;
    }

    // Find the formatted result element we are looking for
    node = root->xmlChildrenNode;

    // Loop through all the results until we find the street address result
    while( node != NULL ) {
        if (xmlStrcmp(node->name, xml_text) ) { // Ignore all implicit text nodes
            if ( 0 == xmlStrcmp(node->name, resp_xml_result) ) {
                if( 0 == parse_result_node(node,address,maxlen) ) {
                    xmlFreeDoc(doc);
                    xmlCleanupParser();
                    return 0;
                }
            }
        }
        node=node->next;
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();

    return -1;
}


/**
 * Do service call to the Google open API to do a reverse lookup of the
 * coordinates to get a street address
 * @param lat
 * @param lon
 * @param address
 * @param maxlen
 * @return 0 on success, -1 on failure
 */
int
lookup_address_from_latlon(char *lat, char *lon,char *address, size_t maxlen) {
    CURL *curl_handle;
    CURLcode res;

    if( address )
        *address = '\0';
    else
        return -1;

    struct memoryStruct chunk;

    chunk.memory = malloc(1); /* will be grown as needed */
    chunk.size = 0; /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, get_service_url(lat, lon));

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, _cb_curl);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &chunk);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* get it! */
    res = curl_easy_perform(curl_handle);

    int rc = 0;

    /* check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "Cannot fetch street address from Google: %s", curl_easy_strerror(res));
        rc = -1;
    } else {
        if (parse_reply(chunk.memory, address, maxlen - 1)) {
            fprintf(stderr, "Failed parsing reply from Google");
            rc = -1;
        }
    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);

    if (chunk.memory)
        free(chunk.memory);

    /* we're done with libcurl, so clean it up */
    curl_global_cleanup();

    return rc;
}


int
main(void) {
    char *name = "ex_reply.xml";
    char address[512];

    // 59.366526,17.95997
    char *lat="59.366526";
    char *lon="17.959971";

    //char *lat="62.387341";
    //char *lon="17.275872";


    if( 0==lookup_address_from_latlon(lat, lon,address, sizeof(address)) )  {
        printf("ADDRESS FOUND: %s\n",address);
    } else {
        printf("ADDRESS NOT FOUND!!\n");
    }

    exit(0);



    char *buff = get_xmlbuff(name);
    if( NULL == buff ) {
        fprintf(stderr,"Cannot open XML file");
    }

    if( 0 == parse_reply(buff, address, sizeof(address)) ) {

        printf("Address found is: %s\n",address);
    } else {
        printf("Failed to parse reply!\n");
    }

    exit(0);
}