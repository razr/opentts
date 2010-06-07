## Process this file with automake to produce Makefile.in

dist_snddata_DATA = test.wav

dist_bin_SCRIPTS = otts-conf

opentts_pythondir = $(pyexecdir)/opentts_config
opentts_python_PYTHON = __init__.py config.py
nodist_opentts_python_PYTHON = paths.py i18n.py

paths_edit = sed \
	-e "s:[@]spdconforigdir[@]:$(spdconforigdir):" \
	-e "s:[@]spdconfdir[@]:$(spdconfdir):" \
	-e "s:[@]snddatadir[@]:$(snddatadir):" \
	-e "s:[@]spddesktopconforigdir[@]:$(spddesktopconforigdir):"

i18n_edit = sed \
	-e "s:[@]localedir[@]:$(localedir):" \
	-e "s:[@]GETTEXT_PACKAGE[@]:$(GETTEXT_PACKAGE):"

paths.py: Makefile
	rm -f $@
	srcdir=; \
	test -f ./$@.in || srcdir=$(srcdir)/; \
	$(paths_edit) $${srcdir}$@.in > $@

paths.py: $(srcdir)/paths.py.in

i18n.py: Makefile
	rm -f $@
	srcdir=; \
	test -f ./$@.in || srcdir=$(srcdir)/; \
	$(i18n_edit) $${srcdir}$@.in > $@

i18n.py: $(srcdir)/i18n.py.in

CLEANFILES = paths.py i18n.py

EXTRA_DIST = paths.py.in i18n.py.in
