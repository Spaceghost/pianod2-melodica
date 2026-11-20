## Process this file with automake to produce Makefile.in

wwwdir          = $(datadir)/pianod/html

if WITH_TONEGENERATOR
tonegenerator_dir	= mediaunits/tonegenerator
tonegenerator_libs	= $(tonegenerator_dir)/libtonegencommand.a \
			  $(tonegenerator_dir)/libtonegen.a
tonegenerator_ldadd	=
else
tonegenerator_dir	= 
tonegenerator_libs	=
tonegenerator_ldadd	=
endif


if WITH_PANDORA
pandora_dir		= mediaunits/pandora
pandora_libs		= $(pandora_dir)/libpandoracommand.a \
			  $(pandora_dir)/libpandora.a
pandora_ldadd		= $(libcurl_LIBS)
else
pandora_dir		= 
pandora_libs		=
pandora_ldadd		=
endif


if WITH_FILESYSTEM
filesystem_dir		= mediaunits/filesystem
filesystem_libs		= $(filesystem_dir)/libfilesystemcommand.a \
			  $(filesystem_dir)/libfilesystem.a
filesystem_ldadd	= $(taglib_LIBS)
else
filesystem_dir		= 
filesystem_libs		=
filesystem_ldadd	=
endif

if WITH_LIBAO
libao_ldadd		= $(ao_LIBS)
endif

if WITH_LIBSDL
libsdl_ldadd		= $(libsdl_LIBS)
endif

if WITH_LIBAVDEVICE
libavdevice_ldadd	= $(avdevice_LIBS)
endif


AUTOMAKE_OPTIONS = subdir-objects

SUBDIRS		= common football parsnip \
		  user \
		  mediaunits/audio \
		  mediaunits/metadata \
		  $(tonegenerator_dir) $(pandora_dir) $(filesystem_dir)

bin_PROGRAMS	= pianod

pianod_CPPFLAGS	= -I$(srcdir)/src \
		  -I$(srcdir)/common \
		  -I$(srcdir)/user \
		  -I$(srcdir)/songs \
		  -I$(srcdir)/mediaunits \
		  -I$(srcdir)/mediaunits/manager \
		  ${gstreamer_CFLAGS} \
		  $(avutil_CFLAGS) $(avcodec_CFLAGS) $(avformat_CFLAGS) $(avfilter_CFLAGS) \
		  $(gnutls_CFLAGS) $(openssl_CFLAGS) $(mbedtls_CFLAGS) \
		  -D_GNU_SOURCE \
		  -DDEFAULT_WWWDIR='"$(wwwdir)"'

pianod_LDADD	= $(pandora_libs) $(tonegenerator_libs) $(filesystem_libs) \
		  mediaunits/audio/libaudiooutput.a \
		  mediaunits/metadata/libmetadata.a \
		  football/libfootball.a \
		  common/libutility.a \
		  parsnip/libparsnip.a \
		  user/libpianoduser.a \
		  $(pandora_ldadd) $(tonegenerator_ldadd) $(filesystem_ldadd) \
		  $(libao_ldadd) $(libsdl_ldadd) $(libavdevice_ldadd) \
		  $(gstreamer_LIBS) \
		  $(avutil_LIBS) $(avcodec_LIBS) $(avformat_LIBS) $(avfilter_LIBS) \
		  $(gnutls_LIBS) $(openssl_LIBS) $(mbedtls_LIBS)

pianod_SOURCES	= \
                songs/musickeys.h songs/musickeys.cpp \
		songs/musictypes.h songs/musictypes.cpp \
		songs/encapmusic.h songs/encapmusic.cpp \
		songs/musiclibrary.h songs/musiclibrary.cpp \
		songs/musiclibraryparameters.h \
		  songs/musiclibraryparameters.cpp \
		songs/musiclibraryhash.h \
		songs/musiclibrarytypes.cpp \
		songs/retainer.h \
		songs/retainedlist.h songs/retainedlist.cpp \
		songs/musiccache.h songs/musiccache.cpp \
		songs/filter.h songs/filter.cpp \
		songs/querylist.h songs/querylist.cpp \
		songs/ratings.h songs/ratings.cpp \
		\
		mediaunits/mediaunit.h \
		mediaunits/mediaunit.cpp \
		mediaunits/mediaplayer.h \
		mediaunits/mediasource.cpp \
		mediaunits/mediaplayer.cpp \
		mediaunits/threadedmediaplayer.cpp \
		mediaunits/playerwrapper.h \
		mediaunits/playerwrapper.cpp \
		mediaunits/mediaparameters.h \
		mediaunits/mediaparameters.cpp \
		\
		mediaunits/mediamanager.h \
		mediaunits/manager/managersource.cpp \
		mediaunits/manager/mediamanager.cpp \
		mediaunits/manager/managercommand.cpp \
		\
		mediaunits/sources.h \
		mediaunits/sources.cpp \
		\
		connection.h connection.cpp \
		conductor.h conductor.cpp \
		engine.h engine.cpp enginecommand.cpp engineresponder.cpp \
		predicate.h predicate.cpp \
		audiooptionsparser.h audiooptionsparser.cpp \
		tuner.h tuner.cpp \
		response.h response.cpp responseline.cpp responsejson.cpp \
		servicemanager.h servicemanager.cpp \
		pianod.cpp

