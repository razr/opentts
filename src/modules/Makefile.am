inc_local = "-I$(top_srcdir)/src/common" "-I$(top_srcdir)/src/audio"
common_sources = module_main.c module_utils.c module_utils.h 
local_libs = $(top_srcdir)/src/common/libcommon.la $(top_srcdir)/src/audio/libsdaudio.la
common_libs = $(local_libs) @AUDIO_DLOPEN_MODULES@

SNDFILE_CFLAGS = @SNDFILE_CFLAGS@
SNDFILE_LIBS = @SNDFILE_LIBS@

AM_CFLAGS = @ERROR_CFLAGS@ 
AM_CPPFLAGS = -DDATADIR=\"$(snddatadir)\" $(inc_local) $(DOTCONF_CFLAGS) $(GLIB_CFLAGS) $(GTHREAD_CFLAGS) $(ibmtts_include) @SNDFILE_CFLAGS@ -D_GNU_SOURCE

modulebin_PROGRAMS = sd_dummy sd_generic sd_festival sd_cicero

if flite_support
modulebin_PROGRAMS += sd_flite
endif

if ibmtts_support
modulebin_PROGRAMS += sd_ibmtts
endif

if espeak_support
modulebin_PROGRAMS += sd_espeak
endif

if ivona_support
modulebin_PROGRAMS += sd_ivona
endif

sd_flite_SOURCES = flite.c $(common_sources)
sd_flite_LDADD = $(common_libs) $(flite_kal) $(flite_basic) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_generic_SOURCES = generic.c $(common_sources) module_utils_addvoice.c
sd_generic_LDADD = $(common_libs) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_festival_SOURCES = festival.c festival_client.c festival_client.h $(common_sources)
sd_festival_LDADD = $(common_libs) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS) @EXTRA_SOCKET_LIBS@

sd_ibmtts_SOURCES = ibmtts.c $(common_sources) module_utils_addvoice.c
sd_ibmtts_LDADD = $(common_libs) -lm -lpthread $(DOTCONF_LIBS) -libmeci @SNDFILE_LIBS@ $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_cicero_SOURCES = cicero.c $(common_sources)
sd_cicero_LDADD = $(common_libs) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

sd_espeak_SOURCES = espeak.c $(common_sources)
sd_espeak_LDADD = $(common_libs) -lespeak $(DOTCONF_LIBS) $(GLIB_LIBS) @SNDFILE_LIBS@ $(GTHREAD_LIBS) @EXTRA_ESPEAK_LIBS@

sd_ivona_SOURCES = ivona.c ivona_client.c ivona_client.h $(common_sources)
sd_ivona_LDADD = $(common_libs) -ldumbtts -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) @SNDFILE_LIBS@ $(GTHREAD_LIBS)

sd_dummy_SOURCES = dummy.c $(common_sources) module_utils_addvoice.c
sd_dummy_LDADD = $(common_libs) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) @SNDFILE_LIBS@ $(GTHREAD_LIBS)

snddata_DATA = dummy-message.wav
EXTRA_DIST = dummy-message.wav
