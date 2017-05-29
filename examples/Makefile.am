

INCLUDES = -I../include

MGLIBS = ../src/libmgp.la

noinst_PROGRAMS= test_prn test_dlg

test_prn_SOURCES=test_prn.c pic.h
test_dlg_SOURCES=test_dlg.c pic.h

test_prn_LDADD =$(MGLIBS) @APP_LIBS@
test_dlg_LDADD =$(MGLIBS) @APP_LIBS@
