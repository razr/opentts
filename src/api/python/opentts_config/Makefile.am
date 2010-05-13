## Process this file with automake to produce Makefile.in
dist_snddata_DATA = test.wav
dist_spddesktopconforig_DATA = opentts.desktop
if HAVE_PYTHON
bin_SCRIPTS = otts-conf
opentts_pythondir = $(pyexecdir)/opentts_config
opentts_python_PYTHON = __init__.py config.py paths.py

paths.py: paths.py.in
	sed -e "s#[@]spdconforigdir[@]#$(spdconforigdir)#" \
	  -e "s#[@]spdconfdir[@]#$(spdconfdir)#" \
	  -e "s#[@]snddatadir[@]#$(snddatadir)#" \
	  -e "s#[@]spddesktopconforigdir[@]#$(spddesktopconforigdir)#" \
	  paths.py.in > paths.py
endif
EXTRA_DIST = paths.py.in
