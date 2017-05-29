
INCLUDES = -I../../include
MGLIBS = ../../src/libmgp.la $(APP_LIBS)
bin_PROGRAMS= rastertoepson

rastertoepson_SOURCES=rastertoepson.c epson.h epson_i.h \
                      print-util.c print-escp2.c print-escp2.h \
                      print-escp2-data.c print-dither.c print-dither.h \
                      print-dither-matrices.c quickmatrix257.h ran.367.179.h \
                      ran.509.131.h print-weave.c print-color.c                                            

rastertoepson_LDADD =$(MGLIBS) -lm

