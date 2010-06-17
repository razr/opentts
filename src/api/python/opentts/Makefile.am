## Process this file with automake to produce Makefile.in

opentts_pythondir = $(pyexecdir)/opentts
opentts_python_PYTHON = __init__.py _test.py client.py

nodist_opentts_python_PYTHON = _constants.py

edit = sed  \
	-e 's:@bindir[@]:$(bindir):g' \
	-e 's:@PACKAGE_VERSION[@]:$(PACKAGE_VERSION):g'

_constants.py: Makefile
	rm -f $@
	srcdir=; \
	test -f ./$@.in || srcdir=$(srcdir)/; \
	$(edit) $${srcdir}$@.in > $@

_constants.py: $(srcdir)/_constants.py.in

CLEANFILES = _constants.py

EXTRA_DIST = _constants.py.in
