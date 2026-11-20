#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
# Autoconf configuration for pianod2.
# @file       configure.ac
# @author     Perette Barella
# @copyright  Copyright 2013-2022 Devious Fish. All rights reserved.
# @version    $Id: configure.ac 419 2025-12-28 22:03:40Z perette $
#
# ***NOTE***
# pkg-config is sometimes used, but not always.  We try to find linker flags
# with it, but it is not an error if the flags are missing.
# PKG_CHECK_MODULES only validates that pkg-config returns stuff; it doesn't
# validate that these are correct, so check lib to make sure they're right.

#
# Configure autoconf
#

AC_PREREQ([2.71])
AC_CONFIG_MACRO_DIRS([m4])
AC_INIT([pianod2],
	m4_esyscmd([svn info svn://svn.deviousfish.com/pianod2/trunk | grep Revision: | awk '{print $2}' | tr -d '\n']),
	[pianod@lists.deviousfish.com],
	,[http://deviousfish.com/pianod2])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_HEADERS([config.h])


#
# Build switches
#

# Feature switches
AC_ARG_ENABLE(debug,
	[AS_HELP_STRING(
		[--enable-debug],
		[use assertions and compile for debugging [disable]]
	)],
	,
	[enable_debug=false]
)
AC_ARG_ENABLE(sanity,
	[AS_HELP_STRING(
		[--enable-sanity],
		[disable to allow crazy, probably broken configurations (for testing) [enable]]
	)], [ AS_IF([test "$enable_sanity" != "yes" -a "$enable_sanity" != "no" ],
		[AC_MSG_ERROR([I am happy to live in a split level head, but that is just nuts.])])
	], [enable_sanity=true]
)
AC_ARG_ENABLE(shadow,
	[AS_HELP_STRING(
		[--enable-shadow],
		[user account shadowing [autodetect]]
	)], [
	    AS_IF([test "$enable_shadow" = "pam"], [
		enable_shadow=PAM
	    ])
	    AS_IF([test "$enable_shadow" != "yes" -a "$enable_shadow" != "no" -a "$enable_shadow" != "check" -a "$enable_shadow" != "passwd" -a "$enable_shadow" != "PAM" ], [
		AC_MSG_ERROR([Invalid option --enable-shadow=$enable_shadow])
	    ])
	], [enable_shadow=check]
)
AC_ARG_WITH(compression,
	[AS_HELP_STRING(
		[--with-compression],
		[with zlib/data file compression [autodetect]]
	)], [], [with_compression=check]
)
AC_ARG_WITH(accesscontrol,
	[AS_HELP_STRING(
		[--with-accesscontrol],
		[with hosts_access access control [autodetect]]
	)], [], [with_accesscontrol=check]
)


# Media Engine Selection: yes, no, check, or package name
AC_ARG_WITH(engine,
	[AS_HELP_STRING(
		[--with-engine],
		[specify media engine [autodetect].  Options are: ffmpeg, avfoundation (osx), gstreamer.]
	)], [
	    AS_IF([test "$with_engine" = "avfoundation"], [
		with_engine=osx
	    ])
	    AS_IF([test "$with_engine" != "yes" -a "$with_engine" != "no" -a "$with_engine" != "check" -a "$with_engine" != "ffmpeg" -a "$with_engine" != "gstreamer" -a "$with_engine" != "osx" ], [
		AC_MSG_ERROR([Invalid option --with-engine=$with_engine])
	    ])
	], [with_engine=check]
)

# Audio output package switches
AC_ARG_WITH(libavdevice,
	[AS_HELP_STRING(
		[--with-libavdevice],
		[include libavdevice for audio output (ffmpeg) [autodetect]]
	)], [], [with_libavdevice=check]
)
AC_ARG_WITH(libao,
	[AS_HELP_STRING(
		[--with-libao],
		[include libao for audio output [autodetect]]
	)], [], [with_libao=check]
)
AC_ARG_WITH(libsdl,
	[AS_HELP_STRING(
		[--with-libsdl],
		[include libsdl for audio output [autodetect]]
	)], [], [with_libsdl=check]
)

# Sources switches
AC_ARG_WITH(tonegenerator,
	[AS_HELP_STRING(
		[--with-tonegenerator],
		[include tone generator source [autodetect]]
	)], [], [with_tonegenerator=check]
)
AC_ARG_WITH(pandora,
	[AS_HELP_STRING(
		[--with-pandora],
		[include Pandora support [autodetect]]
	)], [], [with_pandora=check]
)
AC_ARG_WITH(filesystem,
	[AS_HELP_STRING(
		[--with-filesystem],
		[include filesystem support [included]]
	)], [], [with_filesystem=check]
)

# TLS/SSL Selection: yes, no, check, or package name
AC_ARG_WITH(tls,
	[AS_HELP_STRING(
		[--with-tls],
		[specify TLS encryption package [autodetect].  Package options are: gnutls, mbedtls, openssl, securetransport (osx).]
	)], [
	    AS_IF([test "$with_tls" = "securetransport"], [
		with_tls=osx
	    ], [test "$with_tls" = "libre" ], [
		with_tls=libressl
	    ], [test "$with_tls" = "mbed"], [
		with_tls=mbedtls
	    ], [test "$with_tls" = "gnu"], [
		with_tls=gnutls
	    ])
	    AS_IF([test "$with_tls" != "yes" -a "$with_tls" != "no" -a "$with_tls" != "check" -a "$with_tls" != "mbedtls" -a "$with_tls" != "openssl" -a "$with_tls" != "osx" -a "$with_tls" != "gnutls" -a "$with_tls" != "libressl" ], [
		AC_MSG_ERROR([Invalid option --with-tls=$with_tls])
	    ])
	], [with_tls=check]
)


# Other switches
# Allow taglib to be disabled and use ffmpeg; loses CDDB ID & iTunNORM.
AC_ARG_WITH(taglib,
	[AS_HELP_STRING(
		[--with-taglib],
		[include taglib support [autodetect]]
	)], [], [with_taglib=check]
)


#
# Check for programs needed to build the software.
#
AC_PROG_CC
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX(14, noext, mandatory)
AC_PROG_OBJCXX
AX_OBJCXX_COMPILE_STDCXX(14, noext, mandatory)

AS_IF([test "$enable_debug" == "yes"], [
    # Turn down optimization
    CFLAGS=`echo X${CFLAGS} -g | sed -e 's/^X//' -e 's/-O2/-O0/'`
    CXXFLAGS=`echo X${CXXFLAGS} -g | sed -e 's/^X//' -e 's/-O2/-O0/'`
    OBJCXXFLAGS=`echo X${OBJCXXFLAGS} -g | sed -e 's/^X//' -e 's/-O2/-O0/'`
    # Turn up the warnings
    DF_ADD_COMPILE_FLAG(C, CFLAGS, -Wall)
    DF_ADD_COMPILE_FLAG([C++], CXXFLAGS, -Wall)
    DF_ADD_COMPILE_FLAG([Objective C++], OBJCXXFLAGS, -Wall)
])

AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB
AM_PROG_AR

# Clang on Linux may require explicitly linking the LLVM C++ library.
AC_LANG([C++])
AC_MSG_CHECKING([the C++ compiler can find include files])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <iostream>], [return (0);])],[
		AC_MSG_RESULT([yes])
	],[
		AC_MSG_RESULT([no])
		AC_MSG_NOTICE([This can result from mismatched C/C++ compiler versions.])
		AC_MSG_NOTICE([This might help diagnose this issue:])
		${CXX} -v -E
		AC_MSG_ERROR([Please resolve this and try again.])
	]
)

# Clang on Linux may require explicitly linking the LLVM C++ library.
AC_LANG([C++])
AC_MSG_CHECKING([for required -lstdc++])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <iostream>], [std::cout << "Hi mom!\n"])],[
		AC_MSG_RESULT([no])
	],[
		AC_MSG_RESULT([yes])
		LIBS="-lstdc++"
	]
)

# See if compiler understands Objective C's @autoreleasepool construct
AC_LANG([Objective C++])
AC_MSG_CHECKING([for @autoreleasepool])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [@autoreleasepool { int x; }])], [
    AC_MSG_RESULT(yes)
    AC_DEFINE(AUTORELEASEPOOL, [@autoreleasepool],
	      [Define to @autoreleasepool if supported, else empty.])
], [
    AC_MSG_RESULT(no)
    AC_DEFINE(AUTORELEASEPOOL, [/* unsupported*/])
])


# Link the math library now, because later checks may depend on finding it.
AC_LANG([C])
AC_SEARCH_LIBS([pow], [m])

# Check for pthreads now, as library checks may depend on linking with it.
AS_IF([test "$with_engine" != "osx" ], [
      AC_SEARCH_LIBS([pthread_create], [pthread],,[
	    AC_MSG_NOTICE([Cannot find libpthread, hoping other libraries are built without it])])
])

# ncurses is used by testing library, if available.
# Make sure C++ can link against it.
AC_LANG([C++])
DF_CHECK_NCURSES

#
# Check for file compression support
#
# Check for zlib.
# Dependent checks: ffmpeg
AC_LANG([C])
AS_IF([test "$with_compression" != "no"], [
    DF_CHECK_LIBRARY([zlib], [zlib],
	[zlib.h],,
    	[z], [gzopen],,
	[], [DF_DO_WITHOUT([compression], [zlib not found])]
    )]
)
DF_DECIDE_WITH([compression])


#
# Check for access control support
#
# Check for libwrap
# Dependent checks: None.
AS_IF([test "$with_accesscontrol" != "no"], [
    DF_CHECK_LIBRARY([wrap], [wrap],
	[tcpd.h],,
    	[wrap], [request_init],,
	[], [DF_DO_WITHOUT([accesscontrol], [libwrap not found])]
    )]
)
DF_DECIDE_WITH([accesscontrol])

#
# Check for TLS crypto and TLS support.
#
# If TLS disabled, do nothing.
# If particular TLS is requested, look for that.
# If not, try in order: gnutls, mbedtls, libressl, OS X, open SSL.
# Dependent checks: ffmpeg

AC_LANG([C])
AS_IF([test "$with_tls" = "yes" -o "$with_tls" = "check" -o "$with_tls" = "gnutls" ], [
    DF_CHECK_LIBRARY([gnutls], [gnutls],
	[gnutls/gnutls.h],,
    	[gnutls], [gnutls_record_recv], [],
	[with_tls="gnutls"], [
	    AS_IF([test "$with_tls" = "gnutls"], [
		AC_MSG_ERROR([TLS package GNUTLS not found.])
	    ])
        ]
    )
])

AS_IF([test "$with_tls" = "gnutls"], [
    AC_CHECK_FUNCS([gnutls_transport_set_int2 gnutls_sec_param_to_pk_bits],
	    [], [AC_MSG_ERROR([Missing required function-- gnutls too old?])])
])

AS_IF([test "$with_tls" = "yes" -o "$with_tls" = "check" -o "$with_tls" = "mbedtls" ], [
    DF_CHECK_LIBRARY([mbedtls], [mbedtls],
	[mbedtls/net.h mbedtls/ssl.h mbedtls/entropy.h mbedtls/ctr_drbg.h mbedtls/pem.h mbedtls/pk.h mbedtls/error.h mbedtls/debug.h],,
    	[mbedtls], [mbedtls_ssl_set_session], [-lmbedcrypto -lmbedx509],
	[with_tls=mbedtls], [
	    AS_IF([test "$with_tls" = "mbedtls"], [
		AC_MSG_ERROR([TLS package mBedTLS not found.])
	    ])
	]
    )
])

AS_IF([test "$with_tls" = "yes" -o "$with_tls" = "check" -o "$with_tls" = "libressl" ], [
    
    DF_CHECK_LIBRARY([libressl], [libtls],
	[tls.h],,
	[tls], [tls_config_new],,
	[with_tls=libressl], [
	    AS_IF([test "$with_tls" = "libressl"], [
		AC_MSG_ERROR([TLS package LibreSSL not found.])
	    ])
	]
    )
])

AS_IF([test "$with_tls" = "yes" -o "$with_tls" = "check" -o "$with_tls" = "osx" ], [
    AC_CHECK_FRAMEWORK([CoreFoundation], [ CFDataCreate ], [
	AC_CHECK_FRAMEWORK([Security], [SSLCreateContext], [
	    with_tls=osx
	    LIBS="-framework CoreFoundation -framework Security $LIBS"
	    AC_DEFINE(HAVE_SECURETRANSPORT, [1],
		       [Define this symbol for SecureTransport (OS X)])
	], [
	    AS_IF([test "$with_tls" = "securetransport"], [
	          AC_MSG_ERROR([TLS package SecureTransport not found.])
	    ])
	])
    ])
])

AS_IF([test "$with_tls" = "yes" -o "$with_tls" = "check" -o "$with_tls" = "openssl" ], [
    DF_CHECK_LIBRARY([openssl], [openssl],
	[openssl/ssl.h openssl/err.h],,
	[ssl], [SSL_accept], [-lcrypto],
	[
	    with_tls=openssl
	    AC_CHECK_FUNCS([SSLv3_server_method TLSv1_server_method TLSv1_1_server_method TLSv1_2_server_method SSLv23_server_method TLS_server_method])
	], [
	    AS_IF([test "$with_tls" = "openssl"], [
		AC_MSG_ERROR([TLS package OpenSSL not found.])
	    ])
	]
    )
])


AS_IF([test "$with_tls" = "yes"], [
	AC_MSG_ERROR([No TLS package found.])
], [test "$with_tls" = "check"], [
	with_tls=no
	AC_MSG_WARN([No TLS package found.])
	DF_DO_WITHOUT([pandora], [No TLS package found.], [Pandora])
])

#
# Determine which media engine to use.
#

# Since a user might install gstreamer to work around a borked libav, check
# for it first.
AC_LANG([C])
AS_IF([test "$with_engine" = "gstreamer" -o "$with_engine" = "check" ], [
    DF_CHECK_LIBRARY([gstreamer], [gstreamer-1.0],
	[gst/gst.h],,
	[gstreamer-1.0], [gst_element_set_state],,
	[
	    with_engine=gstreamer
	    DF_UNNEEDED([libao])
	    DF_UNNEEDED([libsdl])
	], [
	    AS_IF([test "$with_engine" = "gstreamer"], [
		AC_MSG_ERROR([Media engine gstreamer not found.])
	   ])
	]
    )
])


# Try ffmpeg, which consists of several related libraries.
# Combine the last 4 checks into a single flag.
# Depends on: gnutls, zlib checks, possibly pthreads

# ffmpeg 
AC_LANG([C])
with_ffmpeg=check
use_ffmpeg=maybe
AS_IF([test "$with_engine" = "ffmpeg"],
	 [with_ffmpeg=yes],
      [test "$with_engine" != "check"],
	 [with_ffmpeg=no
	  use_ffmpeg=no]
)

AS_IF([test "$with_ffmpeg" != "no" ], [
    DF_CHECK_LIBRARY([avutil], [libavutil],
	[libavutil/avutil.h libavutil/avstring.h libavutil/opt.h libavutil/channel_layout.h libavutil/replaygain.h libavutil/dict.h],,
	[avutil], [av_get_sample_fmt_name], [-lswscale],
	[], [use_ffmpeg=no])
    DF_CHECK_LIBRARY([avcodec], [libavcodec],
	[libavcodec/avcodec.h],,
	[avcodec], [avcodec_free_context], [-lswresample],
	[], [use_ffmpeg=no])
    # avformat may need bz2
    AC_CHECK_LIB([bz2], [BZ2_bzReadOpen])
    DF_CHECK_LIBRARY([avformat], [libavformat],
	[libavformat/avformat.h],,
	[avformat], [avformat_new_stream],,
	[], [use_ffmpeg=no])
    DF_CHECK_LIBRARY([avfilter], [libavfilter],
	[libavfilter/buffersink.h libavfilter/buffersrc.h libavfilter/avfilter.h],,
        [avfilter], [avfilter_version],,
	[], [use_ffmpeg=no])
])

AS_IF([test "$use_ffmpeg" = "no"], [
  DF_DO_WITHOUT([ffmpeg], [Missing or incomplete ffmpeg], [ffmpeg])
])

# Make sure we have recent enough libraries, and adapt to more recent changes.
AS_IF([test "x$with_ffmpeg" != "xno"], [
    # ffmpeg 3.1: avcodec_receive_frame and AVCodecParameters were introduced
    AC_CHECK_FUNC([avcodec_receive_frame], [], [
	DF_DO_WITHOUT([ffmpeg], [ffmpeg appears too old; 3.1 and later are supported.])
    ])
])

AS_IF([test "x$with_ffmpeg" != "xno"], [
    # Check for function changes:
    # ffmpeg 4.0: av_muxer_iterate replace av_oformat_next
    AC_CHECK_FUNCS([av_muxer_iterate av_register_all])

    # libavfilter/avfiltergraph.h disappeared in ffmpeg 4.0:
    AC_CHECK_HEADERS([libavfilter/avfiltergraph.h])
])

DF_DECIDE_WITH([ffmpeg])
AS_IF([test "$with_ffmpeg" = "yes"], [
      with_engine=ffmpeg
],[
      DF_DO_WITHOUT([libavdevice], [libavdevice works only with ffmpeg])
])


# Check for OS X media frameworks
AC_LANG([Objective C++])
AS_IF([test "$with_engine" = "check" -o "$with_engine" = "osx" ], [
    avfoundation_check_result=false
    AC_CHECK_FRAMEWORK_OBJC([Cocoa],
	[#include <AppKit/AppKit.h>],
	[ NSLog (@"Yep, it works.") ],
	[ AC_CHECK_FRAMEWORK_OBJC([AVFoundation],
	    [ #include <AVFoundation/AVFoundation.h> ],
	    [[ NSURL *u = [NSURL alloc];
	      AVAudioPlayer *a = [ [AVAudioPlayer alloc] initWithContentsOfURL:u error:nil]; ]],
	    [ AC_CHECK_FRAMEWORK_OBJC([CoreMedia],
	        [ #include <CoreMedia/CMTime.h> ],
		[ CMTime foobar;
		  float f = CMTimeGetSeconds (foobar) ],
		[ avfoundation_check_result=true
		  with_engine=osx ]
	    )]
	 )]
    )
    AS_IF([test "$avfoundation_check_result" = "false" ],
	[AC_MSG_NOTICE([AVFoundation not available; removed from build.])],
	[AC_MSG_NOTICE([AVFoundation requirements are met, including in build.])]
    )
])
AS_IF([test "$with_engine" = "osx"],
	[DF_DO_WITHOUT([libavdevice], [AVFoundation does not use libavdevice])
	 DF_DO_WITHOUT([taglib], [AVFoundation does not use taglib])
	 AS_IF([test "$with_tonegenerator" = "no"],
		 [ DF_UNNEEDED([libao])
	           DF_UNNEEDED([libsdl])
	         ]
	      )
	]
)

# Make sure we've got at least one media engine, unless insane
AS_IF([test "$with_engine" = "check"], [
    AS_IF([test "$enable_sanity" = "no"],
	[AC_MSG_WARN([There is no media engine, but purple skies with flying dinosaurs.])],
	[AC_MSG_ERROR([There is no media engine available])]
    )
    with_engine=no
])

#
# Audio Output Libraries
#

# Check for avdevice and use built-in devices for ffmpeg.
AC_LANG([C])
AS_IF([test "$with_libavdevice" != "no" ], [
    # avdevice may need xcb
    AC_CHECK_LIB([xcb], [xcb_create_window])
    DF_CHECK_LIBRARY([avdevice], [libavdevice],
	[libavdevice/avdevice.h],,
    	[avdevice], [avdevice_register_all],,
	[], [DF_DO_WITHOUT([libavdevice])]
    )
])

AS_IF([test "$with_libavdevice" != "no" ], [
    # Check for function name change for outputting.
    libavdevice_check_saved_LIBS="$LIBS"
    LIBS="$LIBS $avdevice_LIBS"
    AC_CHECK_FUNCS([avformat_alloc_output_context2 av_output_audio_device_next])
    AC_CHECK_FUNC([avformat_new_stream],[], [
	    AC_CHECK_FUNC([av_new_stream],
		    [AC_DEFINE([av_new_stream], [avformat_new_stream], [Define to av_new_stream if using an older ffmpeg])],
		    [DF_DO_WITHOUT([libavdevice], [Can find neither avformat_new_stream nor av_new_stream])]
	    )
    ])
    LIBS="$libavdevice_check_saved_LIBS"
    DF_DECIDE_WITH([libavdevice])
])

# Check for libao.
AS_IF([test "$with_libao" != "no" ], [
    DF_CHECK_LIBRARY([ao], [ao],
	[ao/ao.h],,
	[ao], [ao_play],,
	[ DF_UNNEEDED([libsdl]) ], [DF_DO_WITHOUT([libao])]
    )
    DF_DECIDE_WITH([libao])
])

# Check for libsdl
AS_IF([test "$with_libsdl" != "no" ], [
    DF_CHECK_LIBRARY([libsdl], [libsdl],
	[SDL/SDL.h SDL/SDL_version.h SDL/SDL_audio.h],,
	[SDL], [SDL_OpenAudio],,
	[], [DF_DO_WITHOUT([libsdl])]
    )
    DF_DECIDE_WITH([libsdl])
    AS_IF([test "$with_libsdl" = "yes" ], [
	AC_CHECK_FUNCS([SDL_GetVersion])
    ])
])

# Make sure we've got at least one output, unless insane
AS_IF([test "$with_engine" != "osx" -a "$with_engine" != "gstreamer" -a "$with_libavdevice" = "no" -a "$with_libao" = "no" -a "$with_libsdl" = "no" ],
	[AS_IF([test "$enable_sanity" = "no"],
	    [AC_MSG_WARN([There are no audio outputs, but insanity reigns])],
	    [AC_MSG_ERROR([There are no audio output libraries])]
	)]
)

# Disable tonegenerator if no compatible outputs.
AS_IF([test "$with_engine" != "gstreamer" -a "$with_libao" = "no" -a "$with_libsdl" = "no" -a "$with_libavdevice" = "no" ],
	 [DF_DO_WITHOUT([tonegenerator], [Tone generator requires libao or libsdl])]
)

#
# Other Pandora support
#

AS_IF([test "$with_pandora" != "no" ], [
    AC_LANG([C])
    DF_CHECK_LIBRARY([libcurl], [libcurl],
	[curl/curl.h],,
	[curl], [curl_easy_strerror], [],
	[], [DF_DO_WITHOUT([pandora])]
    )
    DF_DECIDE_WITH([pandora],[Pandora])
])

#
# Other filesystem source support
#
AC_CHECK_HEADERS([dirent.h], [], [
    DF_DO_WITHOUT([filesystem], [Required header files not found.])
])

# Audio metadata reader, needed for local audio.
# Library has its own build configuration checker, and may not use pkg-config.
# Try pkg-config first, then fall back to the package-installed config checker.
AC_LANG([C++])
PKG_CHECK_MODULES([taglib], [taglib],,
	[AC_MSG_WARN([No pkg-config for taglib])])
AC_PATH_PROG(TAGLIB_CONFIG, taglib-config, [], [$PATH$PATH_SEPARATOR$prefix/bin])
AS_IF([test "$with_filesystem" = "no" ],
	[DF_UNNEEDED([taglib])],
      [test "$with_taglib" = "no" ],
	[# do nothing],
      [test "x$taglib_CFLAGS" != "x"],
	[AC_MSG_NOTICE([Using taglib settings from pkg-config])],
      [test "$TAGLIB_CONFIG" = ""],
	[AC_MSG_NOTICE([Found neither pkg-config nor taglib-config for taglib])],
      [
	AC_MSG_NOTICE([Using taglib settings from taglib-config])
	taglib_CFLAGS="`$TAGLIB_CONFIG --cflags`"
	taglib_LIBS="`$TAGLIB_CONFIG --libs`"
      ]
)

# Taglib include points to an include/taglib dir, which contains
# generic filenames like "fileref.h" and "tag.h".  Point the
# preprocessor to the next level up to avoid potential namespace
# collisions.

AS_IF([test "$with_taglib" != "no"], [
    AS_IF([test "$taglib_LIBS" = ""],
	    [taglib_LIBS="-ltag"]
    )
    taglib_CXXFLAGS="`echo ${taglib_CFLAGS} | sed -e 's&/taglib\s& &' -e 's&/taglib$&&'`"
    AC_SUBST([taglib_CXXFLAGS])
    AC_MSG_CHECKING([taglib])
    taglib_check_saved_LIBS="$LIBS"
    LIBS="$LIBS $taglib_LIBS"
    AC_LINK_IFELSE(
	   [AC_LANG_PROGRAM([#include <taglib/fileref.h>],
			    [ TagLib::FileRef f("foobar"); ])],
	   [ AC_MSG_RESULT(yes)
	     AC_DEFINE(HAVE_TAGLIB, [1],
		       [Define this symbol if you have taglib support]) ],
	   [ AC_MSG_RESULT(no)
	     taglib_LIBS=""
	     LIBS="$taglib_check_saved_LIBS"
	     DF_DO_WITHOUT([taglib])]
    )
])
DF_DECIDE_WITH([taglib])
DF_DECIDE_WITH([filesystem])
DF_DECIDE_WITH([tonegenerator])

# Make sure we've got at least one source, unless insane
AS_IF([test "$with_filesystem" = "no" -a "$with_pandora" = "no" -a "$with_tonegenerator" = "no" ], [
    AS_IF([test "$enable_sanity" = "no"],
	[AC_MSG_WARN([There are no sources, pianod will be a few fries short of a happy meal.])],
	[AC_MSG_ERROR([There are no sources])]
    )
])

#
# Check for miscellaneous variations which exist across systems.
#

# Link the password encryption library
AC_SEARCH_LIBS([crypt],[crypt], [],
    [AC_MSG_ERROR([Cannot find required library: crypt (DES password encryption).])])

# Check for PAM (Pluggable Authentication Modules)
# Note: enable_shadow may be yes, no, check, passwd, or PAM
shadowing_type=none
AS_IF([test "$enable_shadow" != "no"], [
    demand_shadow="$enable_shadow"
    AS_IF([test "$enable_shadow" = "yes" ], [
	enable_shadow=check
    ])
    AS_IF([test "$enable_shadow" = "check" -o "$enable_shadow" = "PAM" ], [
	AC_SEARCH_LIBS([pam_start],[pam], [
	    AC_CHECK_HEADER([security/pam_appl.h], [
		enable_shadow=yes
		shadowing_type=PAM
		AC_DEFINE([PAM_AUTHENTICATION], [1], [Define if PAM authentication is applicable.])
	    ])
	])
    ])
    AS_IF([test "$enable_shadow" = "check" -o "$enable_shadow" = "passwd" ], [
        AC_CHECK_FUNC([getpwnam], [
		enable_shadow=yes
		shadowing_type=passwd
		AC_DEFINE([TRADITIONAL_PASSWD], [1], [Define if traditional password authentication applies])
	    ])
    ])
    AS_IF([test "$enable_shadow" = "yes"], [
	AC_DEFINE([SHADOW_CAPABLE], [1], [Define if system capable of shadowing system users.])
    ], [test "$demand_shadow" = "check"], [
	enable_shadow=no
    ], [test "$demand_shadow" = "yes"], [
	AC_MSG_ERROR([No password shadowing support found.])
    ], [
	AC_MSG_ERROR([No Support found for shadow mechanism $enable_shadow.])
    ])
])

# getprogname/setprogname for pianod main:
# May be in standard libraries, in -lbsd, or not exist at all
AC_SEARCH_LIBS([getprogname],[bsd],
    [AC_DEFINE([HAVE_GETPROGNAME],[1],[Set to 1 if getprogname/setprogname is available])])

# IPv6 support
DF_CHECK_IPV6

# Check for MSG_NOSIGNAL and SO_NOSIGPIPE socket options.
# Thanks to Philipp Kern <phil from 0x539.de>
# There are 'net references to an SO_NOSIGNAL, but it is an
# erroneous portmanteau of the other two.
AC_MSG_CHECKING([MSG_NOSIGNAL])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[#include <sys/socket.h>],
	[ int f = MSG_NOSIGNAL;]
   )],
   [ AC_MSG_RESULT(yes)
     AC_DEFINE(HAVE_MSG_NOSIGNAL, [1],
	       [Define this symbol if you have MSG_NOSIGNAL]) ],
   [ AC_MSG_RESULT(no)])

AC_MSG_CHECKING([SO_NOSIGPIPE])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[#include <sys/socket.h>],
	[ int f = SO_NOSIGPIPE; ]
   )],
   [ AC_MSG_RESULT(yes)
     AC_DEFINE(HAVE_SO_NOSIGPIPE, [1],
	       [Define this symbol if you have SO_NOSIGPIPE]) ],
   [ AC_MSG_RESULT(no)])


# Checks for header files.
# Mandatory headers
AC_CHECK_HEADERS([arpa/inet.h assert.h errno.h fcntl.h getopt.h grp.h limits.h math.h netinet/in.h pwd.h signal.h stdarg.h stdbool.h stdint.h stdio.h stdlib.h string.h strings.h sys/socket.h sys/stat.h sys/types.h time.h],
	[], [AC_MSG_ERROR([Required header file not found.])]
)

# Optional headers
AC_CHECK_HEADERS([crypt.h sys/resource.h])


# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([int32_t, int64_t])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset select socket strcasecmp strncasecmp strchr strdup \
	    strerror strrchr strtol strtoul getrusage],
	    [], [AC_MSG_ERROR([Missing required function])])

# If no strtoll, we use strtol.  srandomdev is used when available.
AC_CHECK_FUNCS([strtoll srandomdev])

# Check for ways to canonicalize paths
AC_CHECK_FUNCS([realpath canonicalize_file_name])

#
# Emit config.h and the makefiles.

# Pass configuration options for Automake
AS_IF([test "$enable_debug" == "no"], [
    AC_DEFINE([NDEBUG],[1],[Define to disable debugging assertions])
])
AM_CONDITIONAL([ENABLE_DEBUG],[test "$enable_debug" = 'yes'])

AM_CONDITIONAL([WITH_ACCESSCONTROL],[test "$with_accesscontrol" != 'no'])
AM_COND_IF([WITH_ACCESSCONTROL], [
    AC_DEFINE([WITH_ACCESSCONTROL], [1],[Define to enable access control via hosts_access(5)])
])

AM_CONDITIONAL([WITH_TONEGENERATOR],[test "$with_tonegenerator" != 'no'])
AM_COND_IF([WITH_TONEGENERATOR], [
    AC_DEFINE([WITH_TONEGENERATOR],[1],[Define to enable tone generator])
])

AM_CONDITIONAL([WITH_PANDORA],[test "$with_pandora" '!=' 'no'])
AM_COND_IF([WITH_PANDORA], [
    AC_DEFINE([WITH_PANDORA],[1],[Define to enable Pandora])
])

AM_CONDITIONAL([WITH_FILESYSTEM],[test "$with_filesystem" '!=' 'no'])
AM_COND_IF([WITH_FILESYSTEM], [
    AC_DEFINE([WITH_FILESYSTEM],[1],[Define to enable filesystem source])
])

AM_CONDITIONAL([WITH_LIBAVDEVICE],[test "$with_libavdevice" '!=' 'no'])
AM_COND_IF([WITH_LIBAVDEVICE], [
    AC_DEFINE([WITH_LIBAVDEVICE],[1],[Define to enable libavdevice audio output])
])

AM_CONDITIONAL([WITH_LIBAO],[test "$with_libao" '!=' 'no'])
AM_COND_IF([WITH_LIBAO], [
    AC_DEFINE([WITH_LIBAO],[1],[Define to enable libao audio output])
])

AM_CONDITIONAL([WITH_LIBSDL],[test "$with_libsdl" '!=' 'no'])
AM_COND_IF([WITH_LIBSDL], [
    AC_DEFINE([WITH_LIBSDL],[1],[Define to enable libsdl audio output])
])

AM_CONDITIONAL([WITH_GSTREAMER],[test "$with_engine" = 'gstreamer'])
AM_COND_IF([WITH_GSTREAMER], [
    AC_DEFINE([WITH_GSTREAMER],[1],[Define to enable gstreamer source])
])

AM_CONDITIONAL([WITH_FFMPEG],[test "$with_engine" = 'ffmpeg'])
AM_COND_IF([WITH_FFMPEG], [
    AC_DEFINE([WITH_FFMPEG],[1],[Define to enable ffmpeg source])
])

AM_CONDITIONAL([WITH_AVFOUNDATION],[test "$with_engine" = 'osx'])
AM_COND_IF([WITH_AVFOUNDATION], [
    AC_DEFINE([WITH_AVFOUNDATION],[1],[Define to enable avfoundation source])
])

AM_CONDITIONAL([WITH_TAGLIB],[test "$with_taglib" '!=' 'no'])
AM_COND_IF([WITH_TAGLIB], [
    AC_DEFINE([WITH_TAGLIB],[1],[Define to use taglib for metadata])
])

AM_CONDITIONAL([WITH_LIBGNUTLS], [test "$with_tls" = 'gnutls'])
AM_CONDITIONAL([WITH_LIBMBEDTLS], [test "$with_tls" = 'mbedtls'])
AM_CONDITIONAL([WITH_LIBRESSL], [test "$with_tls" = 'libressl'])
AM_CONDITIONAL([WITH_SECURETRANSPORT], [test "$with_tls" = 'osx'])
AM_CONDITIONAL([WITH_OPENSSL], [test "$with_tls" = 'openssl'])

# There are presently no bug workarounds
# AC_ARG_ENABLE(bugs,
# 	[AS_HELP_STRING(
# 		[--enable-bugs],
# 		[enable bug workarounds]
# 	)]
# )
# AM_CONDITIONAL([ENABLE_BUGS],[test "$enable_bugs" = 'yes'])
# 
# AS_IF([test "$enable_bugs" = "yes"], [
# 	AC_DEFINE([ENABLE_BUGS],[1],[Define to enable bug workarounds])
# ])

# Write the files
AC_CONFIG_FILES([Makefile man/Makefile contrib/Makefile
	     src/Makefile
	     src/common/Makefile
	     src/parsnip/Makefile
	     src/user/Makefile
	     src/football/Makefile
	     src/mediaunits/audio/Makefile
	     src/mediaunits/metadata/Makefile
	     src/mediaunits/pandora/Makefile
	     src/mediaunits/tonegenerator/Makefile
	     src/mediaunits/filesystem/Makefile])

AC_OUTPUT

#
# Generate a report summarizing what's to be built.
#

cat << EOF

======================================================================
pianod2 configuration summary:

Audio engine:		   $with_engine
TLS encryption package:	   $with_tls
Password shadowing:        $shadowing_type

Sources:
	Tone generator:	   $with_tonegenerator
	Pandora:           $with_pandora
	Filesystem:        $with_filesystem

Outputs:
	libavdevices:      $with_libavdevice
	libsdl:            $with_libsdl
	libao:             $with_libao

Features:
	Taglib library:    $with_taglib
	Debug:             $enable_debug
	Compression:       $with_compression
	Access control:    $with_accesscontrol
======================================================================

EOF

