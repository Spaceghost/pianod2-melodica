## Process this file with automake to produce Makefile.in

if WITH_LIBAO
libao_SOURCES		= libaooutput.h libaooutput.cpp
libao_includes		= ${libao_CFLAGS}
endif

if WITH_LIBSDL
libsdl_SOURCES		= libsdloutput.h libsdloutput.cpp
libsdl_includes		= ${libsdl_CFLAGS}
endif

if WITH_FFMPEG
ffmpeg_SOURCES		= ffmpegplayer.h ffmpegcpp.h \
			ffmpegplayer.cpp ffmpegadapter.cpp
ffmpeg_includes		= ${avutil_CFLAGS} ${avcodec_CFLAGS} ${avformat_CFLAGS} ${avfilter_CFLAGS}
endif

if WITH_LIBAVDEVICE
libavdevice_SOURCES	= avdeviceoutput.h avdeviceoutput.cpp
libavdevice_includes	= ${libavdevice_CFLAGS}
endif

if WITH_AVFOUNDATION
avfoundation_SOURCES	= osxplayer.h osxplayer.mm
endif

if WITH_GSTREAMER
gstreamer_SOURCES	= gstreamplayer.h gstreamplayer.cpp
gstreamer_includes	= ${gstreamer_CFLAGS}
endif

noinst_LIBRARIES	= libaudiooutput.a

libaudiooutput_a_CXXFLAGS=${libao_includes} \
			  ${libsdl_includes} \
			  ${ffmpeg_includes} \
			  ${libavdevices_includes} \
			  ${gstreamer_includes}

libaudiooutput_a_CPPFLAGS=-I$(top_srcdir)/src/common \
			  -I$(top_srcdir)/src/mediaunits

libaudiooutput_a_SOURCES= audiooutput.h audiooutput.cpp \
			  $(libao_SOURCES) \
			  $(libsdl_SOURCES) \
			  $(gstreamer_SOURCES) \
			  $(ffmpeg_SOURCES) \
			  $(libavdevice_SOURCES) \
			  $(avfoundation_SOURCES)



