## Process this file with automake to produce Makefile.in

spddesktopconforig_in_files = openttsd.desktop.in

spddesktopconforig_DATA = $(spddesktopconforig_in_files:.desktop.in=.desktop)

# This is to create %.desktop from %.desktop.in,
# Merging translations from .po files
@INTLTOOL_DESKTOP_RULE@

extra_DIST = $(spddesktopconforig_in_files)
DISTCLEANFILES = $(spddesktopconforig_DATA)
