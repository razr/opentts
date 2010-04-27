## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libsdaudio.la
libsdaudio_la_CPPFLAGS = $(GLIB_CFLAGS)
libsdaudio_la_SOURCES = spd_audio.c spd_audio.h spd_audio_plugin.h static_plugins.c oss.c
libsdaudio_la_LIBADD = $(GLIB_LIBS)

if nas_support
libsdaudio_la_SOURCES += nas.c
endif

if pulse_support
libsdaudio_la_SOURCES += pulse.c
endif

if alsa_support
libsdaudio_la_SOURCES += alsa.c
endif

if libao_support
libsdaudio_la_SOURCES += libao.c
endif

include_HEADERS = spd_audio_plugin.h

