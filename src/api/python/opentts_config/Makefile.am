## Process this file with automake to produce Makefile.in

dist_snddata_DATA = test.wav
dist_spddesktopconforig_DATA = opentts.desktop

bin_SCRIPTS = otts-conf
opentts_pythondir = $(pyexecdir)/opentts_config
opentts_python_PYTHON = __init__.py config.py
nodist_opentts_python_PYTHON = paths.py

edit = sed \
	-e "s:@spdconforigdir[@]:$(spdconforigdir):" \
	-e "s:@spdconfdir[@]:$(spdconfdir):" \
	-e "s:@snddatadir[@]:$(snddatadir):" \
	-e "s:@spddesktopconforigdir[@]:$(spddesktopconforigdir):"

paths.py: Makefile
	rm -f $@
	srcdir=; \
	test -f ./$@.in || srcdir=$(srcdir)/; \
	$(edit) $${srcdir}$@.in > $@

paths.py: $(srcdir)/paths.py.in

EXTRA_DIST = paths.py.in
