## Process this file with automake to produce Makefile.in

noinst_LIBRARIES	= libfilesystem.a libfilesystemcommand.a

libfilesystem_a_CPPFLAGS= $(taglib_CPPFLAGS) \
			  -I$(top_srcdir)/src \
			  -I$(top_srcdir)/src/common \
			  -I$(top_srcdir)/src/songs \
			  -I$(top_srcdir)/src/mediaunits \
			  -I$(top_srcdir)/src/user

libfilesystem_a_SOURCES	= filesystem.h \
			  filesystem.cpp filesystemplayer.cpp \
			  filesystemsource.cpp filesystemtypes.cpp

libfilesystemcommand_a_CPPFLAGS = $(libfilesystem_a_CPPFLAGS) -I$(top_srcdir)/src
libfilesystemcommand_a_SOURCES  = filesystemcommand.h filesystemcommand.cpp

EXTRA_DIST		= cookies.mp3
