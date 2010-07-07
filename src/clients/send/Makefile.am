
bin_PROGRAMS = otts-send
otts_send_SOURCES = otts-send.h otts-send.c server.c client.c common.c
otts_send_CPPFLAGS = "-I$(top_srcdir)/include"
otts_send_LDADD = $(top_builddir)/src/libs/common/libcommon.la -lcommon $(EXTRA_SOCKET_LIBS)

