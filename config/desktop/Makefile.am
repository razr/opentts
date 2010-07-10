## Process this file with automake to produce Makefile.in

spddesktopconforig_in_files = openttsd.desktop.in

nodist_spddesktopconforig_DATA = $(spddesktopconforig_in_files:.desktop.in=.desktop)

# This is to create %.desktop from %.desktop.in,
# Merging translations from .po files
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(spddesktopconforig_in_files)
CLEANFILES = $(nodist_spddesktopconforig_DATA)
