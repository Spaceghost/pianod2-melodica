## Process this file with automake to produce Makefile.in

if WITH_TAGLIB
taglib_SOURCES		= taglibreader.h taglibreader.cpp

tagreader_SOURCES       = tagreader.cpp
tagreader_includes      = $(taglib_CXXFLAGS)
tagreader_LDADD         = $(taglib_LIBS)

tagreader_TESTS		= tagreader
else
taglib_SOURCES		=
if WITH_FFMPEG
ffmpeg_SOURCES		= ffmpegmetadata.h ffmpegmetadata.cpp
ffmpeg_includes		= ${avutil_CFLAGS} ${avcodec_CFLAGS} ${avformat_CFLAGS} ${avfilter_CFLAGS}

endif
if WITH_GSTREAMER
gstreamer_SOURCES	= gstreammetadata.h gstreammetadata.cpp
gstreamer_includes      = ${gstreamer_CFLAGS}
endif
endif

if WITH_AVFOUNDATION
avfoundation_SOURCES	= osxmetadata.h osxmetadata.mm
endif


noinst_LIBRARIES	= libmetadata.a

libmetadata_a_CXXFLAGS  = $(tagreader_includes) \
			  $(ffmpeg_includes) \
			  $(gstreamer_includes)

libmetadata_a_CPPFLAGS  = -I$(top_srcdir)/src/common \
			  -I$(top_builddir)/src/include \
                          -I$(top_srcdir)/src/mediaunits

libmetadata_a_SOURCES= metadata.cpp metadata.h \
			  $(taglib_SOURCES) \
			  $(ffmpeg_SOURCES) \
			  $(gstreamer_SOURCES) \
			  $(avfoundation_SOURCES)

noinst_PROGRAMS         = $(tagreader_TESTS)
TESTS                   = $(tagreader_TESTS)
