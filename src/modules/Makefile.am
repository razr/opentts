inc_local = "-I$(top_srcdir)/src/common" "-I$(top_srcdir)/src/audio" "-I$(top_srcdir)/include"
common_sources = module_main.c module_utils.c module_utils.h 
local_libs = $(top_srcdir)/src/common/libcommon.la $(top_srcdir)/src/audio/libsdaudio.la
common_libs = $(local_libs) @AUDIO_DLOPEN_MODULES@

SNDFILE_CFLAGS = @SNDFILE_CFLAGS@
SNDFILE_LIBS = @SNDFILE_LIBS@

AM_CFLAGS = @ERROR_CFLAGS@ 
AM_CPPFLAGS = -DDATADIR=\"$(snddatadir)\" $(inc_local) $(DOTCONF_CFLAGS) $(GLIB_CFLAGS) $(GTHREAD_CFLAGS) $(ibmtts_include) @SNDFILE_CFLAGS@ -D_GNU_SOURCE

modulebin_PROGRAMS = cicero dummy festival generic

cicero_SOURCES = cicero.c $(common_sources)
cicero_LDADD = $(common_libs) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

dummy_SOURCES = dummy.c $(common_sources) module_utils_addvoice.c
dummy_LDADD = $(common_libs) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) @SNDFILE_LIBS@ $(GTHREAD_LIBS)
dist_snddata_DATA = dummy-message.wav

festival_SOURCES = festival.c festival_client.c festival_client.h $(common_sources)
festival_LDADD = $(common_libs) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS) @EXTRA_SOCKET_LIBS@

generic_SOURCES = generic.c $(common_sources) module_utils_addvoice.c
generic_LDADD = $(common_libs) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

if espeak_support
modulebin_PROGRAMS += espeak
espeak_SOURCES = espeak.c $(common_sources)
espeak_LDADD = $(common_libs) -lespeak $(DOTCONF_LIBS) $(GLIB_LIBS) @SNDFILE_LIBS@ $(GTHREAD_LIBS) @EXTRA_ESPEAK_LIBS@
endif

if flite_support
modulebin_PROGRAMS += flite
flite_SOURCES = flite.c $(common_sources)
flite_LDADD = $(common_libs) $(flite_kal) $(flite_basic) -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if ibmtts_support
modulebin_PROGRAMS += ibmtts
ibmtts_SOURCES = ibmtts.c $(common_sources) module_utils_addvoice.c
ibmtts_LDADD = $(common_libs) -lm -lpthread $(DOTCONF_LIBS) -libmeci @SNDFILE_LIBS@ $(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if ivona_support
modulebin_PROGRAMS += ivona
ivona_SOURCES = ivona.c ivona_client.c ivona_client.h $(common_sources)
ivona_LDADD = $(common_libs) -ldumbtts -lpthread $(DOTCONF_LIBS) $(GLIB_LIBS) @SNDFILE_LIBS@ $(GTHREAD_LIBS)
endif
