## Process this file with automake to produce Makefile.in

pvtlib_LTLIBRARIES = libcommon.la
libcommon_la_CPPFLAGS = "-I$(top_srcdir)/include" $(GLIB_CFLAGS) \
	-DLOCALEDIR=\"$(localedir)\" -DOPENTTS_INTERNAL
libcommon_la_LIBADD = $(GLIB_LIBS)
libcommon_la_LDFLAGS = -avoid-version
libcommon_la_SOURCES = fdsetconv.c getline.c i18n.c timestamp.c
