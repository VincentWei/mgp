#ifndef FILTERS_H_
#define FILTERS_H_

#include "mgpconfig.h"

#ifdef NOUNIX
extern int bmp_rasterentry(int fd, int driver);
extern int hp_rasterentry(int fd, int driver);
extern int epson65_rasterentry(int fd, int driver);
struct _filter_table {
    char * name;
    int (* raster_entry)(int, int); 
}FILTER_TABLE[]={
    {"Bitmap", bmp_rasterentry},
    {"HP-Laserjet", hp_rasterentry},
    {"Epson-C65", epson65_rasterentry},
};
#endif

#endif /*FILTERS_H_*/
