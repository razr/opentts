## Process this file with automake to produce Makefile.in

inc_local = "-I$(top_srcdir)/include" "-I$(top_srcdir)/src/common"
local_libs = $(top_srcdir)/src/common/libcommon.la
common_libs = $(local_libs) $(GLIB_LIBS)
AM_CPPFLAGS = $(GLIB_CFLAGS) $(inc_local)
lib_LTLIBRARIES = libsdaudio.la
libsdaudio_la_CPPFLAGS = $(AM_CPPFLAGS)  -DPLUGIN_DIR="\"@audiodir@\""
libsdaudio_la_LDFLAGS = -version-info @LIB_SDAUDIO_CURRENT@:@LIB_SDAUDIO_REVISION@:@LIB_SDAUDIO_AGE@
libsdaudio_la_LIBADD = $(common_libs) -lltdl -lpthread
libsdaudio_la_SOURCES = spd_audio.c spd_audio.h

audio_LTLIBRARIES = 
if oss_support
audio_LTLIBRARIES += otts_oss.la
otts_oss_la_SOURCES = oss.c
otts_oss_la_LIBADD = $(common_libs)
otts_oss_la_LDFLAGS = -module -avoid-version
endif

if nas_support
audio_LTLIBRARIES += otts_nas.la
otts_nas_la_SOURCES = nas.c
otts_nas_la_LIBADD = $(NAS_LIBS) $(common_libs)

otts_nas_la_LDFLAGS = -module -avoid-version
endif

if pulse_support
audio_LTLIBRARIES += otts_pulse.la
otts_pulse_la_SOURCES = pulse.c
otts_pulse_la_CFLAGS = $(PULSE_CFLAGS)
otts_pulse_la_LIBADD = $(PULSE_LIBS) $(common_libs)

otts_pulse_la_LDFLAGS = -module -avoid-version
endif

if alsa_support
audio_LTLIBRARIES += otts_alsa.la
otts_alsa_la_SOURCES = alsa.c
otts_alsa_la_CFLAGS = $(ALSA_CFLAGS)
otts_alsa_la_LIBADD = $(ALSA_LIBS) $(common_libs)

otts_alsa_la_LDFLAGS = -module -avoid-version
endif

if libao_support
audio_LTLIBRARIES += otts_libao.la
otts_libao_la_SOURCES = libao.c
otts_libao_la_LIBADD = $(LIBAO_LIBS) $(common_libs)

otts_libao_la_LDFLAGS = -module -avoid-version
endif

