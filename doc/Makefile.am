
info_TEXINFOS = ssip.texi speech-dispatcher.texi speech-dispatcher-cs.texi spd-say.texi
ssip_TEXINFOS = gpl.texi fdl.texi
speech_dispatcher_TEXINFOS = gpl.texi fdl.texi
speech_dispatcher_cs_TEXINFOS = gpl.texi fdl.texi

EXTRA_DIST = figures

SUFFIXES = .html
html_docs = speech-dispatcher.html ssip.html spd-say.texi
HTMLDIR = htmldoc

MAINTAINERCLEANFILES = speechd/*

.texi.html:
	$(MAKEINFO) --html -o $(HTMLDIR) $<
	cp -duv --parents figures/*.png $(HTMLDIR)

.PHONY: html
html: version.texi $(html_docs)


