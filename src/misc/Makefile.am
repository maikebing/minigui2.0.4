INCLUDES = -I../../include -I../include

noinst_LTLIBRARIES = libmisc.la

EXTRA_DIST= misc.c systext.c rwops.c endianrw.c nposix.c clipboard.c \
			math.c about.c misc-dt.c error.c sockio.c listenfd.c \
            makefile.ng makefile.msvc

LITE_SRCS=error.c sockio.c listenfd.c

libmisc_la_SOURCES = misc.c systext.c rwops.c endianrw.c nposix.c clipboard.c \
				math.c about.c $(LITE_SRCS)

SRC_FILES = $(libmisc_la_SOURCES)
LIB_NAME=libmisc

TOP_DIR=../..
