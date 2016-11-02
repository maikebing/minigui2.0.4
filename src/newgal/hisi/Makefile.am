TOP_DIR=../../..
INCLUDES=-I../../../include -I../../include -I../

SRC_FILES = hi3510_fbvideo.c hi3560_fbvideo.c pix_array.c tde.c
HDR_FILES = hi3510_fb.h hi3560_fb.h hi3510_fbvideo.h hi3560_fbvideo.h \
			hi_tde.h tde_reg.h

noinst_LTLIBRARIES = libvideo_hisifbcon.la
libvideo_hisifbcon_la_SOURCES = $(SRC_FILES) $(HDR_FILES)

EXTRA_DIST=

LIB_NAME=libvideo_hisifbcon
