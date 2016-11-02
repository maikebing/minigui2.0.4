TOP_DIR=..

COMMON_SUBDIRS = libc misc main kernel sysres ial control font gui ime mybmp include 

lib_LTLIBRARIES = libminigui.la

COMMON_LADD =               \
    libc/liblibc.la         \
    misc/libmisc.la         \
    control/libcontrol.la   \
    font/libfont.la         \
    font/in-core/libincore.la \
    ial/libial.la           \
    gui/libgui.la           \
    ime/libime.la           \
    main/libmain.la         \
    mybmp/libmybmp.la       \
    kernel/libkernel.la     \
    sysres/libsysres.la     

if LITE_VERSION
if STAND_ALONE
LITE_LADD = standalone/libstandalone.la
LITE_SUBDIRS = standalone
else
LITE_LADD = server/libserver.la client/libclient.la
LITE_SUBDIRS = server client
endif
else
LITE_LADD = 
LITE_SUBDIRS = 
endif

NEWGAL_LADD = newgal/libnewgal.la newgdi/libnewgdi.la
NEWGAL_SUBDIRS = newgal newgdi

DIST_SUBDIRS = $(COMMON_SUBDIRS) newgal newgdi server client standalone
SUBDIRS = $(COMMON_SUBDIRS) $(NEWGAL_SUBDIRS) $(LITE_SUBDIRS) 

libminigui_la_LIBADD =  \
    $(COMMON_LADD)      \
    $(LITE_LADD)        \
    $(NEWGAL_LADD)

#@SYSTEM_LIBS@

libminigui_la_SOURCES = template.c template.h
libminigui_la_LDFLAGS = -release $(LT_RELEASE) -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

CLEANFILES=*.a

EXTRA_DIST=makefile.ng makefile.msvc
