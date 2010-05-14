
bin_PROGRAMS = spdsend
spdsend_SOURCES = spdsend.h spdsend.c server.c client.c common.c 
spdsend_CPPFLAGS = "-I$(top_srcdir)/src/common/"
spdsend_LDADD = $(top_srcdir)/src/common/libcommon.la -lpthread @EXTRA_SOCKET_LIBS@

