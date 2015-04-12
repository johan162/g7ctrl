/* =========================================================================
 * File:        gpsdist.c
 * Description: Functions to calculate the geodesic distance between two
 *              latitude/longitude points.
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: gpsdist.c 914 2015-04-05 22:37:26Z ljp $
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
#include <math.h>
#include <syslog.h>

#include "config.h"
#include "gpsdist.h"
#include "logger.h"

/**
 * Convert degree to radian
 * @param d Degree
 * @return Radian
 */
static inline double
deg2rad(const double d) {
    return d * (M_PI / 180.0);
}

/**
 * Calculate an approximation of the distance between two GPS points 
 * assuming a WGS-84 ellipsoid (geodetic distance).
 * This uses the Haversine method (great circle distance). This is a
 * good balance between accuracy and complexity. However please note
 * that the accuracy will vary since the earth radius is not equal
 * everywhere. 
 * See gpsdist_mi() to get result in miles
 * @param lat1 First point lat
 * @param lon1 First point lon
 * @param lat2 Second point lat
 * @param lon2 Second point lon
 * @return The distance in km between the points
 * @see http://en.wikipedia.org/wiki/Law_of_haversines
 */
double
gpsdist_km(const double lat1,const double lon1,const double lat2,const double lon2) {
    static const double earth_diameter_km = 6372.797 * 2.0;
    const double dlon2 = deg2rad((lon2 - lon1) / 2.0);
    const double dlat2 = deg2rad((lat2 - lat1) / 2.0);
    const double a = sin(dlat2) * sin(dlat2) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * sin(dlon2) * sin(dlon2);
    return earth_diameter_km * atan2(sqrt(a), sqrt(1 - a));
}

/**
 * Calculate an approximation of the distance between two GPS points
 * assuming a WGS-84 ellipsoid (geodetic distance)
 * This uses the Haversine method (great circle distance). This is a
 * good balance between accuracy and complexity. However please note
 * that the accuracy will vary since the earth radius is not equal
 * everywhere.
 * See gpsdist_km() to get result in km
 * See gpsdist2_km() for alternative way to calculate this distance
 * @param lat1 First point lat
 * @param lon1 First point lon
 * @param lat2 Second point lat
 * @param lon2 Second point lon
 * @return The distance in miles between the points
 * @sa http://en.wikipedia.org/wiki/Law_of_haversines
 */
double
gpsdist_mi(const double lat1,const double lon1,const  double lat2,const double lon2) {
    return gpsdist_km(lat1, lon1, lat2, lon2) / 1.609344;
}

/**
 * Calculate an approximation of the distance between two GPS points
 * assuming a WGS-84 ellipsoid (geodetic distance)
 * This uses the Vincenty inverse formula for ellipsoids.
 * See gpsdist_km() for alternative way to calculate this distance using 
 * Haversine method.
 *
 * from: Vincenty inverse formula - T Vincenty, "Direct and Inverse Solutions of Geodesics on the
 *       Ellipsoid with application of nested equations", Survey Review, vol XXII no 176, 1975
 * @see   http://www.ngs.noaa.gov/PUBS_LIB/inverse.pdf
 *
 * @param lat1, lon1 first point in decimal degrees
 * @param lat2, lon2 second point in decimal degrees
 * @return distance in metric meters between points
 */
double
gpsdist_m(const double lat1, const double lon1, const double lat2, const double lon2) {
    
    //logmsg(LOG_DEBUG,"Calculating distance between (%lf,%lf) and (%lf,%lf)",lat1,lon1,lat2,lon2);
    
    static const double a = 6378137, b = 6356752.314245, f = 1 / 298.257223563; // WGS-84 ellipsoid params
    const double L = deg2rad(lon2 - lon1);
    const double U1 = atan((1 - f) * tan(deg2rad(lat1)));
    const double U2 = atan((1 - f) * tan(deg2rad(lat2)));
    const double sinU1 = sin(U1), cosU1 = cos(U1);
    const double sinU2 = sin(U2), cosU2 = cos(U2);

    double sinLambda, cosLambda, sinSigma;
    double cosSigma, sigma, sinAlpha, cosSqAlpha, cos2SigmaM;
    double C;

    double lambda = L, lambdaP, iterLimit = 100;
    do {
        sinLambda = sin(lambda);
        cosLambda = cos(lambda);
        sinSigma = sqrt((cosU2 * sinLambda) * (cosU2 * sinLambda) +
                (cosU1 * sinU2 - sinU1 * cosU2 * cosLambda) * (cosU1 * sinU2 - sinU1 * cosU2 * cosLambda));

        if (sinSigma == 0) return 0; // co-incident points

        cosSigma = sinU1 * sinU2 + cosU1 * cosU2*cosLambda;
        sigma = atan2(sinSigma, cosSigma);
        sinAlpha = cosU1 * cosU2 * sinLambda / sinSigma;
        cosSqAlpha = 1 - sinAlpha*sinAlpha;
        cos2SigmaM = cosSigma - 2 * sinU1 * sinU2 / cosSqAlpha;

        if (isnan(cos2SigmaM)) cos2SigmaM = 0; // equatorial line: cosSqAlpha=0 (§6)

        C = f / 16 * cosSqAlpha * (4 + f * (4 - 3 * cosSqAlpha));
        lambdaP = lambda;
        lambda = L + (1 - C) * f * sinAlpha *
                (sigma + C * sinSigma * (cos2SigmaM + C * cosSigma * (-1 + 2 * cos2SigmaM * cos2SigmaM)));
    } while (abs(lambda - lambdaP) > 1e-12 && --iterLimit > 0);

    if (iterLimit == 0) {
        return nan(""); // formula failed to converge
    }

    const double uSq = cosSqAlpha * (a * a - b * b) / (b * b);
    const double A = 1 + uSq / 16384 * (4096 + uSq * (-768 + uSq * (320 - 175 * uSq)));
    const double B = uSq / 1024 * (256 + uSq * (-128 + uSq * (74 - 47 * uSq)));
    const double deltaSigma = B * sinSigma * (cos2SigmaM + B / 4 * (cosSigma * (-1 + 2 * cos2SigmaM * cos2SigmaM) -
            B / 6 * cos2SigmaM * (-3 + 4 * sinSigma * sinSigma)*(-3 + 4 * cos2SigmaM * cos2SigmaM)));
    double s = b * A * (sigma - deltaSigma);

    s = round(s * 10.0) / 10.0; // round to 1m precision
    
    //logmsg(LOG_DEBUG,"Distance (%.6f,%.6f) <-> (%.6f,%.6f) : %.1f m",lat1,lon1,lat2,lon2,s);
    
    return s;

}

/* EOF */
