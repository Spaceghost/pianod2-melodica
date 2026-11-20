## Process this file with automake to produce Makefile.in

EXTRA_DIST      = gzstream.h

noinst_LIBRARIES	= libutility.a
libutility_a_CPPFLAGS	= -I$(top_builddir) \
			  -I$(top_srcdir)/src \
			  -I$(top_srcdir)/src/songs \
			  -I$(top_srcdir)/src/user \
			  -D_GNU_SOURCE -DSYSCONFDIR='"$(sysconfdir)"'

if WITH_PANDORA
http_client             = httpclient.h httpclient.cpp
else
http_client		= 
endif

libutility_a_SOURCES	= logging.h logging.cpp \
			  enum.h enumeratedarray.h \
			  threadsafecircularqueue.h \
			  callback.h callbackimpl.h \
			  lookup.h \
			  fundamentals.h \
			  utility.h utility.cpp \
			  interpreter.h interpreter.cpp \
			  fileio.h fileio.cpp \
			  $(http_client)

callback_test_SOURCES	= callback_test.cpp callback.h callbackimpl.h
http_test_SOURCES	= http_test.cpp httpclient.cpp httpclient.h
http_test_LDADD         = $(libcurl_LIBS)

utility_test_CXXFLAGS   = -DUNIT_TEST
utility_test_SOURCES    = failurecounter.h utility.cpp
utility_test_LDADD      = $(ncurses_LIBS)

if WITH_PANDORA
noinst_PROGRAMS		= callback_test http_test utility_test
TESTS			= callback_test utility_test
else
noinst_PROGRAMS		= callback_test utility_test
TESTS			= callback_test
endif
