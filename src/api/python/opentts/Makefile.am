## Process this file with automake to produce Makefile.in

opentts_pythondir = $(pyexecdir)/opentts
opentts_python_PYTHON = __init__.py _test.py client.py

if ENABLE_SESSION
nodist_opentts_python_PYTHON = spawn.py

edit = sed  \
	-e 's:@bindir[@]:$(bindir):g'

spawn.py: Makefile
	rm -f $@
	srcdir=; \
	test -f ./$@.in || srcdir=$(srcdir)/; \
	$(edit) $${srcdir}$@.in > $@

spawn.py: $(srcdir)/spawn.py.in

endif

EXTRA_DIST = spawn.py.in
