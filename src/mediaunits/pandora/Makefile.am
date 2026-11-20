## Process this file with automake to produce Makefile.in

noinst_LIBRARIES	= libpandora.a libpandoracommand.a

libpandora_a_CPPFLAGS	= -I$(top_srcdir)/src \
			  -I$(top_srcdir)/src/common \
			  -I$(top_srcdir)/src/songs \
			  -I$(top_srcdir)/src/mediaunits \
			  -I$(top_srcdir)/src/user

libpandora_a_SOURCES	= pandora.h \
			  pandoratypes.h pandoratypes.cpp \
			  pandoramessages.h pandoramessages.cpp \
			  pandoracomm.h pandoracomm.cpp \
			  pandoraparameters.h pandoraparameters.cpp \
			  pandorasource.cpp \
			  pandoralibrary.cpp \
			  pandorastation.h pandorastation.cpp \
			  pandoraplayer.cpp \
			  pandoraseeds.cpp \
                          blowfish.h blowfish.cpp

blowfish_test_CPPFLAGS    = -I$(top_srcdir)/src_common
blowfish_test_SOURCES     = blowfish_test.cpp blowfish.h blowfish.cpp

noinst_PROGRAMS		  = blowfish_test
TESTS			  = blowfish_test


libpandoracommand_a_CPPFLAGS = $(libpandora_a_CPPFLAGS) -I$(top_srcdir)/src
libpandoracommand_a_SOURCES = pandoracommand.h pandoracommand.cpp

