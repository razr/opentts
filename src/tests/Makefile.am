
c_api = $(top_srcdir)/src/api/c
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/src/common

check_PROGRAMS = long_message clibrary clibrary2 run_test connection_recovery

long_message_SOURCES = long_message.c
long_message_LDADD = $(c_api)/libopentts.la @EXTRA_SOCKET_LIBS@

clibrary_SOURCES = clibrary.c
clibrary_LDADD = $(c_api)/libopentts.la @EXTRA_SOCKET_LIBS@

clibrary2_SOURCES = clibrary2.c
clibrary2_LDADD = $(c_api)/libopentts.la @EXTRA_SOCKET_LIBS@

connection_recovery_SOURCES = connection-recovery.c
connection_recovery_LDADD = $(c_api)/libopentts.la @EXTRA_SOCKET_LIBS@

run_test_SOURCES = run_test.c
run_test_LDADD = $(c_api)/libopentts.la @EXTRA_SOCKET_LIBS@

EXTRA_DIST= *.test *.wav
