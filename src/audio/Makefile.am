## Process this file with automake to produce Makefile.in

ottslib_LTLIBRARIES = libottsalsa.la libottslibao.la libottsnas.la libottsoss.la libottspulse.la libsdaudio.la
AM_CPPFLAGS = $(GLIB_CFLAGS)
libsdaudio_la_CPPFLAGS = $(AM_CPPFLAGS)  -DPLUGIN_DIR="\"@ottslibdir@\""
libsdaudio_la_LDFLAGS = -version-info @LIB_SDAUDIO_CURRENT@:@LIB_SDAUDIO_REVISION@:@LIB_SDAUDIO_AGE@
libsdaudio_la_LIBADD = $(GLIB_LIBS) -lltdl -lpthread
libsdaudio_la_SOURCES = spd_audio.c spd_audio.h spd_audio_plugin.h static_plugins.c

if oss_support
libottsoss_la_SOURCES = oss.c spd_audio_plugin.h
libottsoss_la_LIBADD = $(GLIB_LIBS)

if audio_static
libottsoss_la_LDFLAGS = -static
libsdaudio_la_LIBADD += libottsoss.la
else
libottsoss_la_LDFLAGS = -module
endif
endif

if nas_support
libottsnas_la_SOURCES = nas.c spd_audio_plugin.h
libottsnas_la_LIBADD = $(NAS_LIBS) $(GLIB_LIBS)

if audio_static
libottsnas_la_LDFLAGS = -static
libsdaudio_la_LIBADD += libottsnas.la
else
libottsnas_la_LDFLAGS = -module
endif
endif

if pulse_support
libottspulse_la_SOURCES = pulse.c spd_audio_plugin.h
libottspulse_la_CFLAGS = $(PULSE_CFLAGS)
libottspulse_la_LIBADD = $(PULSE_LIBS) $(GLIB_LIBS)

if audio_static
libottspulse_la_LDFLAGS = -static
libsdaudio_la_LIBADD += libottspulse.la
else
libottspulse_la_LDFLAGS = -module
endif
endif

if alsa_support
libottsalsa_la_SOURCES = alsa.c spd_audio_plugin.h
libottsalsa_la_CFLAGS = $(ALSA_CFLAGS)
libottsalsa_la_LIBADD = $(ALSA_LIBS) $(GLIB_LIBS)

if audio_static
libottsalsa_la_LDFLAGS = -static
libsdaudio_la_LIBADD += libottsalsa.la
else
libottsalsa_la_LDFLAGS = -module
endif
endif

if libao_support
libottslibao_la_SOURCES = libao.c spd_audio_plugin.h
libottslibao_la_LIBADD = $(LIBAO_LIBS) $(GLIB_LIBS)

if audio_static
libottslibao_la_LDFLAGS = -static
libsdaudio_la_LIBADD += libottslibao.la
else
libottslibao_la_LDFLAGS = -module
endif
endif

include_HEADERS = spd_audio_plugin.h

