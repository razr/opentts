INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/src/common -I$(top_srcdir)/src/audio
common_sources = module_main.c module_utils.c module_utils.h  audio.c audio.h
local_libs = $(top_srcdir)/src/common/libcommon.la
common_libs = $(local_libs) $(AUDIO_DLOPEN_MODULES)

AM_CFLAGS = $(ERROR_CFLAGS)
AM_CPPFLAGS = -DDATADIR=\"$(snddatadir)\" $(DOTCONF_CFLAGS) $(GLIB_CFLAGS) $(GTHREAD_CFLAGS) $(ibmtts_include) $(SNDFILE_CFLAGS) \
-DPLUGIN_DIR="\"$(audiodir)\"" -D_GNU_SOURCE -DOPENTTS_INTERNAL

modulebin_PROGRAMS = cicero dummy festival generic

cicero_SOURCES = cicero.c $(common_sources)
cicero_LDADD = $(common_libs) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

dummy_SOURCES = dummy.c $(common_sources) module_utils_addvoice.c
dummy_LDADD = $(common_libs) $(DOTCONF_LIBS) $(GLIB_LIBS) $(SNDFILE_LIBS) $(GTHREAD_LIBS)
dist_snddata_DATA = dummy-message.wav

festival_SOURCES = festival.c festival_client.c festival_client.h $(common_sources)
festival_LDADD = $(common_libs) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS) $(EXTRA_SOCKET_LIBS)

generic_SOURCES = generic.c $(common_sources) module_utils_addvoice.c
generic_LDADD = $(common_libs) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)

if espeak_support
modulebin_PROGRAMS += espeak
espeak_SOURCES = espeak.c $(common_sources)
espeak_LDADD = $(common_libs) -lespeak $(DOTCONF_LIBS) $(GLIB_LIBS) $(SNDFILE_LIBS) $(GTHREAD_LIBS) $(EXTRA_ESPEAK_LIBS)
endif

if flite_support
modulebin_PROGRAMS += flite
flite_SOURCES = flite.c $(common_sources)
flite_LDADD = $(common_libs) $(flite_kal) $(flite_basic) $(DOTCONF_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if ibmtts_support
modulebin_PROGRAMS += ibmtts
ibmtts_SOURCES = ibmtts.c $(common_sources) module_utils_addvoice.c
ibmtts_LDADD = $(common_libs) $(DOTCONF_LIBS) -libmeci $(SNDFILE_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS)
endif

if ivona_support
modulebin_PROGRAMS += ivona
ivona_SOURCES = ivona.c ivona_client.c ivona_client.h $(common_sources)
ivona_LDADD = $(common_libs) -ldumbtts $(DOTCONF_LIBS) $(GLIB_LIBS) $(SNDFILE_LIBS) $(GTHREAD_LIBS)
endif
