## Process this file with automake to produce Makefile.in
if HAVE_PYTHON
opentts_pythondir = $(pyexecdir)/opentts
opentts_python_PYTHON = __init__.py _test.py client.py
if ENABLE_SESSION
opentts_python_PYTHON += spawn.py

spawn.py:
	echo "SPD_SPAWN_CMD=\"$(prefix)/bin/speechd-server-spawn\"" > spawn.py
endif
endif
