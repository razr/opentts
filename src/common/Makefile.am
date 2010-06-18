## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include

pvtlib_LTLIBRARIES = libcommon.la
libcommon_la_CPPFLAGS = $(GLIB_CFLAGS) -DLOCALEDIR=\"$(localedir)\" \
	-DOPENTTS_INTERNAL
libcommon_la_LIBADD = $(GLIB_LIBS)
libcommon_la_LDFLAGS = -avoid-version
libcommon_la_SOURCES = fdsetconv.c fdsetconv.h \
timestamp.c timestamp.h getline.c getline.h i18n.h i18n.c
