# 
# Support functions for autoconf library & include checking.
# @file       dfcheckipv6.cpp - DF_CHECK_IPV6
# @author     Perette Barella
# @date       2020-04-30
# @copyright  Copyright 2020 Devious Fish. All rights reserved.
#
# DF_CHECK_IPV6([proto],[additional-yes],[additional-no])
# Defines HAVE_IPV6 if it is available.
# If the [proto] parameter is empty, checks for basic support.
# Otherwise, may be a protocol value to ensure necessary header files
# are available. Supported protocol checks are:
# - ICMP

AC_DEFUN([DF_CHECK_IPV6], [
    AC_LANG_PUSH([C])
    AC_MSG_CHECKING([for IPv6 support]m4_if([$1],,,[ with $1]))
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <sys/socket.h>
#include <netinet/in.h>
m4_if([$1],,,[$1],[ICMP],[
#include <netinet/ip.h>
#include <netinet/icmp6.h>
],[m4_fatal([Invalid protocol in DF_CHECK_IPV6])])
          ],
          [ int f = PF_INET6; struct sockaddr_in6 i; i.sin6_family = AF_INET6; ]
      )],
      [ AC_MSG_RESULT(yes)
	$2
        AC_DEFINE(HAVE_IPV6, [1], [Define this symbol if you have IPV6 support]) ],
      [ AC_MSG_RESULT(no)
	$3])
    AC_LANG_POP([C])
])


