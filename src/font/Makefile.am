
SUBDIRS = in-core

noinst_LTLIBRARIES = libfont.la

SRC_FILES = charset.c rawbitmap.c sysfont.c logfont.c \
            devfont.c fontname.c varbitmap.c \
            fontcache.c freetype.c type1.c freetype2.c qpf.c \
            gbunimap.c gbkunimap.c gb18030unimap.c big5unimap.c \
            ujisunimap.c sjisunimap.c euckrunimap.c \
            textops.c mapunitogb.c mapunitogbk.c mapunitobig5.c mapunitogb18030.c \
			scripteasy.c

HDR_FILES = charset.h rawbitmap.h varbitmap.h freetype.h type1.h freetype2.h qpf.h se_minigui.h

EXTRA_DIST = convgbmap.c jisunimap.c $(SRC_FILES) $(HDR_FILES) \
             makefile.ng makefile.msvc

#if HAS_FREETYPE2
#INCLUDES = `freetype-config --cflags` -I../../include -I../include
#else 
INCLUDES = -I../../include -I../include
#endif

libfont_la_SOURCES = $(SRC_FILES) $(HDR_FILES)

LIB_NAME=libfont

TOP_DIR=../..
