## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include

noinst_LTLIBRARIES = libcommon.la
libcommon_la_SOURCES = fdsetconv.c fdsetconv.h \
timestamp.c timestamp.h getline.c getline.h
libcommon_la_CPPFLAGS = $(GLIB_CFLAGS) -DOPENTTS_INTERNAL
