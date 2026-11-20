## Process this file with automake to produce Makefile.in

noinst_LIBRARIES	= libfootball.a
noinst_PROGRAMS		= 

if WITH_LIBGNUTLS
transport_files = fb_transport_gnutls.c
endif
if WITH_LIBMBEDTLS
transport_files = fb_transport_mbedtls.c
endif
if WITH_LIBRESSL
transport_files = fb_transport_libressl.c
endif
if WITH_SECURETRANSPORT
transport_files = fb_transport_osx.c
endif
if WITH_OPENSSL
transport_files = fb_transport_openssl.c
endif



libfootball_a_CPPFLAGS	= -D_GNU_SOURCE
libfootball_a_SOURCES	= fb_event.c fb_service.c fb_socketmgr.c \
			  fb_transport.c $(transport_files) \
			  fb_http.c fb_message.c fb_utility.c sha1.c \
			  fb_arena.cpp fb_connection.cpp fb_servicepp.cpp \
			  fb_public.h fb_service.h fb_transport.h \
			  football.h \
			  sha1.h

TESTS			= 


