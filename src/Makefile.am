
INCLUDES = -I../include -I../ -I../filter -I../ -I../filter/hplj -I../ -I../filter/epc65 

lib_LTLIBRARIES=libmgp.la

libmgp_la_SOURCES=image.h filters.h \
				 image.c \
				 image-dc.c \
				 image-colorspace.c \
				 image-zoom.c \
				 ppd.c \
				 raster.c \
				 imagetoraster.c \
				 printing.c \
				 portclib.h \
				 portclib.c \
				 common.c \
                 nparser.h \
                 nparser.c \
                 printdlg.c

CLEANFILES=*.a

EXTRA_DIST = \
			makefile.ng filters.h portclib.c
