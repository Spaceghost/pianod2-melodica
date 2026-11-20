## Process this file with automake to produce Makefile.in

noinst_LIBRARIES	= libtonegen.a libtonegencommand.a

libtonegen_a_CPPFLAGS	= -I$(top_srcdir)/src/common \
			  -I$(top_srcdir)/src \
			  -I$(top_srcdir)/src/songs \
			  -I$(top_srcdir)/src/mediaunits \
			  -I$(top_srcdir)/src/user

libtonegen_a_SOURCES	= tonegen.h \
			  tonegen.cpp tonegenplayer.cpp \
			  tonegensource.cpp

libtonegencommand_a_CPPFLAGS = $(libtonegen_a_CPPFLAGS) -I$(top_srcdir)/src
libtonegencommand_a_SOURCES  = tonegencommand.h tonegencommand.cpp


