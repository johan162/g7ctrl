/* =========================================================================
 * File:        gpsdist.h
 * Description: Functions to calculate the geodesic distance between two
 *              latitude/longitude points.
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: gpsdist.h 688 2015-01-25 09:51:19Z ljp $
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

#ifndef GPSDIST_H
#define	GPSDIST_H

#ifdef	__cplusplus
extern "C" {
#endif

double gpsdist_km(const double lat1, const double lon1, const double lat2, const double lon2);

double gpsdist_mi(const double lat1, const double lon1, const double lat2, const double lon2);

double gpsdist_m(const double lat1,const double lon1,const double lat2,const double lon2);

#ifdef	__cplusplus
}
#endif

#endif	/* GPSDIST_H */

