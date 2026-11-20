## Process this file with automake to produce Makefile.in

noinst_LIBRARIES	= libpianoduser.a

libpianoduser_a_CPPFLAGS= -I$(top_srcdir)/src/common \
			  -I$(top_srcdir)/src \
			  -I$(top_srcdir)/src/songs \
			  -I$(top_srcdir)/src/mediaunits -D_GNU_SOURCE
libpianoduser_a_SOURCES	= datastore.h user.h users.h ownership.h \
			  datastore.cpp user.cpp users.cpp \
			  ownership.cpp usercommand.cpp

