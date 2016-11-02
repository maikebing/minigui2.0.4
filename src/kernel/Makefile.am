# also for makefile.ng and makefile.msvc
INCLUDES = -I../../include -I../include

noinst_LTLIBRARIES = libkernel.la

libkernel_la_SOURCES = timer.c \
                       init.c init-lite.c \
                       desktop-ths.c desktop-sa.c desktop-procs.c \
                       cursor.c cursor-procs.c cursor-sa.c \
                       sharedres.c \
                       message.c event.c \
                       blockheap.c fixstr.c slotset.c
# hash.c

EXTRA_DIST= desktop-comm.c makefile.ng makefile.msvc

# for makefile.ng and makefile.msvc
LIB_NAME = libkernel
SRC_FILES = $(libkernel_la_SOURCES)

TOP_DIR=../..
