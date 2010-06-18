
bin_PROGRAMS = otts-send
otts_send_SOURCES = otts-send.h otts-send.c server.c client.c common.c
otts_send_CPPFLAGS = "-I$(top_srcdir)/src/common/"
otts_send_LDADD = -L$(top_builddir)/src/common -lcommon $(EXTRA_SOCKET_LIBS)

