## Process this file with automake to produce Makefile.in

SUBDIRS= modules clients

all-local: openttsd.conf

openttsd.conf: openttsd.conf.in
	sed -e s:INCLUDE_PATH:$(spdconfdir):g speechd.conf.in > speechd.conf


spdconf_DATA= openttsd.conf

spdconforig_DATA= openttsd.conf

CLEANFILES = openttsd.conf

EXTRA_DIST = openttsd.conf.in
