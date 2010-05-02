## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(GLIB_CFLAGS)
lib_LTLIBRARIES = libsdaudio.la
libsdaudio_la_CPPFLAGS = $(AM_CPPFLAGS)  -DPLUGIN_DIR="\"@ottslibdir@\""
libsdaudio_la_LDFLAGS = -version-info @LIB_SDAUDIO_CURRENT@:@LIB_SDAUDIO_REVISION@:@LIB_SDAUDIO_AGE@
libsdaudio_la_LIBADD = $(GLIB_LIBS) -lltdl -lpthread
libsdaudio_la_SOURCES = spd_audio.c spd_audio.h spd_audio_plugin.h static_plugins.c

ottslib_LTLIBRARIES = 
if oss_support
ottslib_LTLIBRARIES += otts_oss.la
otts_oss_la_SOURCES = oss.c spd_audio_plugin.h
otts_oss_la_LIBADD = $(GLIB_LIBS)

if audio_static
otts_oss_la_LDFLAGS = -static
libsdaudio_la_LIBADD += otts_oss.la
else
otts_oss_la_LDFLAGS = -module -avoid-version
endif
endif

if nas_support
ottslib_LTLIBRARIES += otts_nas.la
otts_nas_la_SOURCES = nas.c spd_audio_plugin.h
otts_nas_la_LIBADD = $(NAS_LIBS) $(GLIB_LIBS)

if audio_static
otts_nas_la_LDFLAGS = -static
libsdaudio_la_LIBADD += otts_nas.la
else
otts_nas_la_LDFLAGS = -module -avoid-version
endif
endif

if pulse_support
ottslib_LTLIBRARIES += otts_pulse.la
otts_pulse_la_SOURCES = pulse.c spd_audio_plugin.h
otts_pulse_la_CFLAGS = $(PULSE_CFLAGS)
otts_pulse_la_LIBADD = $(PULSE_LIBS) $(GLIB_LIBS)

if audio_static
otts_pulse_la_LDFLAGS = -static
libsdaudio_la_LIBADD += otts_pulse.la
else
otts_pulse_la_LDFLAGS = -module -avoid-version
endif
endif

if alsa_support
ottslib_LTLIBRARIES += otts_alsa.la
otts_alsa_la_SOURCES = alsa.c spd_audio_plugin.h
otts_alsa_la_CFLAGS = $(ALSA_CFLAGS)
otts_alsa_la_LIBADD = $(ALSA_LIBS) $(GLIB_LIBS)

if audio_static
otts_alsa_la_LDFLAGS = -static
libsdaudio_la_LIBADD += otts_alsa.la
else
otts_alsa_la_LDFLAGS = -module -avoid-version
endif
endif

if libao_support
ottslib_LTLIBRARIES += otts_libao.la
otts_libao_la_SOURCES = libao.c spd_audio_plugin.h
otts_libao_la_LIBADD = $(LIBAO_LIBS) $(GLIB_LIBS)

if audio_static
otts_libao_la_LDFLAGS = -static
libsdaudio_la_LIBADD += otts_libao.la
else
otts_libao_la_LDFLAGS = -module -avoid-version
endif
endif

include_HEADERS = spd_audio_plugin.h

