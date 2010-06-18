## Process this file with automake to produce Makefile.in

inc_local = "-I$(top_srcdir)/include" "-I$(top_srcdir)/src/common"
local_libs = -L$(top_builddir)/src/common -lcommon
common_libs = $(local_libs) $(GLIB_LIBS)
AM_CPPFLAGS = $(GLIB_CFLAGS) $(inc_local)

audio_LTLIBRARIES = 

if alsa_support
audio_LTLIBRARIES += alsa.la
alsa_la_SOURCES = alsa.c
alsa_la_CFLAGS = $(ALSA_CFLAGS)
alsa_la_LIBADD = $(ALSA_LIBS) $(common_libs)
alsa_la_LDFLAGS = -module -avoid-version
endif

if libao_support
audio_LTLIBRARIES += libao.la
libao_la_SOURCES = libao.c
libao_la_LIBADD = $(LIBAO_LIBS) $(common_libs)
libao_la_LDFLAGS = -module -avoid-version
endif

if nas_support
audio_LTLIBRARIES += nas.la
nas_la_SOURCES = nas.c
nas_la_LIBADD = $(NAS_LIBS) $(common_libs)
nas_la_LDFLAGS = -module -avoid-version
endif

if oss_support
audio_LTLIBRARIES += oss.la
oss_la_SOURCES = oss.c
oss_la_LIBADD = $(common_libs)
oss_la_LDFLAGS = -module -avoid-version
endif

if pulse_support
audio_LTLIBRARIES += pulse.la
pulse_la_SOURCES = pulse.c
pulse_la_CFLAGS = $(PULSE_CFLAGS)
pulse_la_LIBADD = $(PULSE_LIBS) $(common_libs)
pulse_la_LDFLAGS = -module -avoid-version
endif

