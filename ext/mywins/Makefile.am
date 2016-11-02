TOP_DIR=../..
INCLUDES = -I../../include -I../include

noinst_LTLIBRARIES = libmywins.la

libmywins_la_SOURCES = mywindows.c helpwin.c filedlg.c newfiledlg.c colordlg.c colorspace.c

AM_CFLAGS = -D__MGEXT_LIB__

EXTRA_DIST=makefile.ng makefile.msvc

SRC_FILES=$(libmywins_la_SOURCES)
LIB_NAME=libmywins
