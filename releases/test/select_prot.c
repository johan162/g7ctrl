// We want the full POSIX and C99 standard
#define _GNU_SOURCE

// Standard defines
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sqlite3.h>

// Called for each row in the result set
// Global DB handle
sqlite3 *sqlDB=NULL;

struct GPX_trkType {
	char lat[32];
	char lon[32];
	char date[32];
	char elev[16];
};
char minlat[32],maxlat[32],minlon[32],maxlon[32];

size_t trkLC=0;
#define MAX_GPXTRKLIST 10000

struct GPX_trkType *GPX_trkTypeList;


// Silent gcc about unused "arg"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

char *
convert_to_utc(char *datetime) {
    static char buff[32];
    // Convert "20131201093042" to "2013-12-01T09:30:42Z"
    char *p=datetime;
    strncpy(buff,p,4);
    p+=4;
    buff[4]='-';
    buff[5]=*p++;
    buff[6]=*p++;
    buff[7]='-';
    buff[8]=*p++;
    buff[9]=*p++;
    buff[10]='T';
    buff[11]=*p++;
    buff[12]=*p++;
    buff[13]=':';
    buff[14]=*p++;
    buff[15]=*p++;
    buff[16]=':';
    buff[17]=*p++;
    buff[18]=*p++;
    buff[19]='Z';
    buff[20]='\0';

    return buff;
}

// Result callback from sqlite3_exec(). This callback is called for each row
// in the result set.
static int
sqres_callback(void *NotUsed, int nColumns, char **azSolVals, char **azColName) {
    //for (int i = 0; i < nColumns; i++) {
     //   fprintf(stderr,"Column %03d: %s = %s", i, azColName[i], azSolVals[i] ? azSolVals[i] : "NULL");
    //}

    if( nColumns != 4 || trkLC >= MAX_GPXTRKLIST ) {
            return 1; // Abort
    }
    fprintf(stderr,"SELECT result #%02d (date=%s)\n",trkLC,azSolVals[2]);
    struct GPX_trkType *p = &GPX_trkTypeList[trkLC];
    strcpy(p->lat, azSolVals[0]);
    strcpy(p->lon, azSolVals[1]);
    strcpy(p->date, convert_to_utc(azSolVals[2]));
    strcpy(p->elev, azSolVals[3]);

    // Keep track of boundary values in set since this is needed
    // in the GPX format
    if( 0 == trkLC ) {
        strcpy(minlat,p->lat);
        strcpy(maxlat,p->lat);
        strcpy(minlon,p->lon);
        strcpy(maxlon,p->lon);
    } else {
        if( strcmp(p->lat,minlat) < 0 )
            strcpy(minlat,p->lat);
        if( strcmp(p->lon,minlon) < 0 )
            strcpy(minlon,p->lon);

        if( strcmp(p->lat,maxlat) > 0 )
            strcpy(maxlat,p->lat);
        if( strcmp(p->lon,maxlon) > 0 )
            strcpy(maxlon,p->lon);
    }
    trkLC++;
    return 0;
}
#pragma GCC diagnostic pop

void
add_wcond(char *w, char *col, char *op, char *val) {
    if (*val) {
        if (*w) {
            sprintf(w, " AND %s %s %s ", col, op, val);
        } else {
            sprintf(w, "WHERE %s %s %s ", col, op, val);
        }
    }
}

int
export_to_internal_set(void) {

    char q[1024];
    strcpy(q, "select fld_lat, fld_long, fld_datetime, fld_altitude from tbl_track ");
    char w[512];

    char from_date[32] = {'\0'};
    char to_date[32] = {'\0'};
    char deviceid[32] = {'\0'};
    char eventid[32] = {'\0'};

    *w = '\0';
    add_wcond(w, "fld_datetime", ">=", from_date);
    add_wcond(w, "fld_datetime", "<=", to_date);
    add_wcond(w, "fld_deviceid", "=", deviceid);
    add_wcond(w, "fld_eventid", "=", eventid);
    strcat(q, w);
    strcat(q, ";");
    fprintf(stderr, "SQL q=\"%s\"\n\n", q);

    // Pre-allocate space for result set
    // Assume result set < 10,000 entries
    GPX_trkTypeList = (struct GPX_trkType *) calloc(MAX_GPXTRKLIST, sizeof (struct GPX_trkType));
    if (NULL == GPX_trkTypeList) {
        fprintf(stderr, "Cannot allocate result buffer\n");
        return -1;
    }
    trkLC = 0;
    *minlat='\0';*maxlat='\0';*minlon='\0';*maxlon='\0';
    char *errMsg;
    int rc = sqlite3_exec(sqlDB, q, sqres_callback, (void *) 0, &errMsg);
    if (rc) {
        fprintf(stderr, "Can execute SQL [\"%s\"]\n", errMsg);
        sqlite3_free(errMsg);
        return -1;
    }
}

void
dump_internal_set() {
    fprintf(stderr,"DUMP FOR SELECT\n");
    for(size_t i=0; i < trkLC; i++) {
        fprintf(stderr,"#%03d: %s (%s,%s) [%s]\n", (int)i,
                GPX_trkTypeList[i].date,
                GPX_trkTypeList[i].lat,GPX_trkTypeList[i].lon,
                GPX_trkTypeList[i].elev);
    }
    fprintf(stderr,"\nDone.\n\n");
}

#define GPX_XML_HEADER "<?xml version=\"1.0\"?>\n\
<gpx version=\"1.1\" \n\
creator=\"g7ctrl http://www.sourceforge/p/g7ctrl\" \n\
xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n\
xmlns=\"http://www.topografix.com/GPX/1/1\"\n\
xsi:schemaLocation=\"http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd\">\n"


int
export_to_gpx() {

    fprintf(stderr,GPX_XML_HEADER);
    fprintf(stderr,"<time>20013-11-29T11:32:09Z</time>\n");
    fprintf(stderr,"<bounds minlat=\"%s\" minlon=\"%s\" maxlat=\"%s\" maxlon=\"%s\" />\n",
                   minlat,minlon,maxlat,maxlon);
    fprintf(stderr,"<trk>\n");
    fprintf(stderr,"  <name>Export from GM7 GPS tracker</name>\n");
    fprintf(stderr,"  <trkseg>\n");
    for(size_t i=0; i < trkLC; i++) {
        fprintf(stderr,"    <trkpt lat=\"%s\" lon=\"%s\">\n      <ele>%s</ele>\n      <time>%s</time>\n    </trkpt>\n",
                GPX_trkTypeList[i].lat,GPX_trkTypeList[i].lon,
                GPX_trkTypeList[i].elev,
                GPX_trkTypeList[i].date);
    }
    fprintf(stderr,"  </trkseg>\n");
    fprintf(stderr,"</trk>\n");
}

int
db_setup(char *dbfilename) {

    int rc = sqlite3_open(dbfilename,&sqlDB);
    if( rc ){
        fprintf(stderr,"Can not open database file %s\n",dbfilename);
        sqlite3_close(sqlDB);
        return -1;
    } else {
       fprintf(stdout,"Opened DB file %s\n",dbfilename);
    }
    return 0;
}

int
main(void) {

    printf("Starting ...\n");

    if( 0==db_setup("/home/ljp/dev/g7ctrl/test/tracker_db.sqlite") ) {
        export_to_internal_set();
        sqlite3_close(sqlDB);
    }

    dump_internal_set();
    fprintf(stderr,"\n");
    export_to_gpx();

}