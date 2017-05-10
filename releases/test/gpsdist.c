#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#define d2r (M_PI / 180.0)

//calculate haversine distance for the freat circle distance between
//the two points
double gpsdist_km2(double lat1, double lon1, double lat2, double lon2)
{
    double dlon2 = (lon2 - lon1) * d2r / 2.0;
    double dlat2 = (lat2 - lat1) * d2r / 2.0 ;
    double a = sin(dlat2)*sin(dlat2) + cos(lat1*d2r) * cos(lat2*d2r) * sin(dlon2)*sin(dlon2);
    printf("a=%lf\n",a);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    printf("c=%lf\n",c);
    double d = 6371 * c;

    return d;
}

inline double
deg2rad(double d) __attribute__((pure));

double
deg2rad(double d) {
    return d*(M_PI / 180.0);
}
/**
 * Calculate an approximation of the distance between two GPS points.
 * This uses the Haversine method (great circle distance). This is a
 * good balance between accuracy and complexity. However please note
 * that the accuracy will vary since the earth radius is not equal
 * everywhere.
 * @param lat1 First point lat
 * @param lon1 First point lon
 * @param lat2 Second point lat
 * @param lon2 Second point lon
 * @return The distance in metric km between the points
 */
double
gpsdist_km(double lat1, double lon1, double lat2, double lon2)
{
    double dlon2 = deg2rad((lon2 - lon1) / 2.0);
    double dlat2 = deg2rad((lat2 - lat1) / 2.0) ;
    double a = sin( dlat2 )*sin( dlat2 ) + cos( deg2rad(lat1) ) * cos( deg2rad(lat2) ) * sin( dlon2 )*sin( dlon2 );
    return 6371.0 * 2 * atan2(sqrt(a), sqrt(1-a));
}




int
main(int argc, char **argv) {

  double dist=gpsdist_km(59.326100,18.074100,59.326300,18.074300);
  
  const double a=(59.326100-59.326300);
  const double b=(18.074200-18.074300);
  
  printf("Distance: =%lf (sqrt=%.12lf)\n",dist, a*a + b*b);
  exit(0);

}
