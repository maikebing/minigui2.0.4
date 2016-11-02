INCLUDES = -I../ -I../../include -I../../../include

noinst_LTLIBRARIES = libincore.la

libincore_la_SOURCES = vgarom_8x8.c sansserif_11x13.c courier_8x13.c symb12.c \
    system_14x16.c terminal_8x12.c fixedsys_8x15.c \
    6x12-iso8859-1.c 8x16-iso8859-1.c 12x24-iso8859-1.c \
    song-12-gb2312.c song-16-gb2312.c song-24-gb2312.c \
    hei-16-gb2312.c \
    vbf_helvR16.c  vbf_helvR21.c vbf_helvR27.c \
    gothic_6x12rk.c gothic_12_jiskan.c \
    gothic_8x16rk.c gothic_16_jiskan.c \
    gothic_7x14rk.c gothic_14_jiskan.c \
    ming-12-big5.c ming-16-big5.c ming-24-big5.c \
    qpf-helvetica-10.c qpf-micro-4.c qpf-times-10.c \
	qpf-fmsong-12.c qpf-fmsong-16.c qpf-fmkai-18.c qpf-fmhei-20.c

EXTRA_DIST=makefile.ng makefile.msvc

SRC_FILES=$(libincore_la_SOURCES)
LIB_NAME=libincore

TOP_DIR=../../..
