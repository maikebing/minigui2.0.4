AUTOMAKE_OPTIONS=foreign

SUBDIRS = src ext vcongui include rtos etc scripts configs m4 build 3rd-party

EXTRA_DIST = \
            GNUmakefile makefile.ng makefile.msvc  rules.make \
            Version README README.zh NEWS \
            LICENSE.POLICY LICENSE.POLICY.zh   

# The directory where the include files will be installed
libminiguiincludedir = $(includedir)/minigui

# Which header files to install
libminiguiinclude_HEADERS = mgconfig.h

docs:
	doxygen Doxyfile

