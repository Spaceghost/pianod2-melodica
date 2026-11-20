## Process this file with automake to produce Makefile.in

wwwdir		= $(datadir)/pianod/html

SUBDIRS		= src man contrib

dist_www_DATA	= html/robots.txt \
		  html/translate.js \
		  \
		  html/console.html \
		  html/console.css \
		  html/console.lib.jsm \
		  \
		  html/viewer.html \
		  html/viewer.css \
		  html/viewer.lib.jsm \
		  \
		  html/index.html \
		  html/client.css \
		  html/client.lib.jsm \
		  html/ratings.js \
		  \
		  html/pianod-button.gif \
		  html/pianod-icon.gif \
		  html/no-art.jpeg \
		  \
		  html/locale/index.html.en_us \
		  html/locale/viewer.html.en_us \
		  html/locale/console.html.en_us \
		  html/locale/translate.js.en_us \
		  \
		  html/locale/index.html.en \
		  html/locale/viewer.html.en \
		  html/locale/console.html.en \
		  html/locale/translate.js.en \
		  \
		  html/locale/index.html.fr \
		  html/locale/viewer.html.fr \
		  html/locale/console.html.fr \
		  html/locale/translate.js.fr \
		  \
		  html/locale/index.html.de \
		  html/locale/viewer.html.de \
		  html/locale/console.html.de \
		  html/locale/translate.js.de \
		  \
		  html/locale/index.html.es \
		  html/locale/viewer.html.es \
		  html/locale/console.html.es \
		  html/locale/translate.js.es
		  

EXTRA_DIST      = pianod_unittest \
		  Documentation/commands.md \
		  Documentation/launching.md \
		  Documentation/predicates.md \
		  Documentation/protocol.md \
		  Documentation/title.md \
		  Documentation/football.md

TESTS		= pianod_unittest

$(dist_www_DATA):
	make -C html compress

FORCE:
