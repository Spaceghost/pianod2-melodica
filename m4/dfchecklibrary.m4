# 
# Support functions for autoconf library & include checking.
# @file       dfchecklibrary.m4 - DF_CHECK_LIBRARY
# @author     Perette Barella
# @date       2017-11-29
# @copyright  Copyright 2017 Devious Fish. All rights reserved.
#
# DF_CHECK_LIBRARY([variable-prefix], [modules],
#	[header-file], [includes],
#	[library], [function], [default-additional-libraries],
#	[additional-if-workable], [additional-if-not-workable]
#	
# This is similar to the AC_CHECK_LIB function, but combines a single
# check for the package, include files that are needed to compile,
# and the library file for linking.
#
# - variable-prefix and modules are as in PKG_CHECK_MODULES.
# - header-file and includes are as in AC_CHECK_HEADERS, *BUT* 
#   individual HAVE_<header>_H defines are not generated.
# - library and function are as in AC_CHECK_LIB.
#   AC_CHECK_LIB's other-libraries parameter is taken from
#   <variable-prefix>_CFLAGS, which is set by PKG_CHECK_MODULES.
# - In case package data isn't available, default-additional-libraries
#   are used to set <variable-prefix>_CFLAGS instead.
#
# On success, HAVE_LIB<library> is defined and the library is prepended
# to LIBS, as in AC_CHECK_LIB.  ADDITIONAL actions may be provided in
# additional-if-workable; user-supplied actions don't suppress existing
# behavior.
#
# On failure, additional-if-not-workable actions are taken.  All else is
# left in or returned to its original state.

AC_DEFUN([DF_CHECK_LIBRARY], [
    m4_if([$1$2],[],,
	[$1],,[AC_MSG_ERROR([No variable prefix given for DF_CHECK_LIBRARY $2])],
	[$2],,[AC_MSG_ERROR([No package name given for DF_CHECK_LIBRARY $1])],
	[PKG_CHECK_MODULES([$1], [$2],, [
	    AC_MSG_WARN([No pkg-config for $2])
	    $1[]_LIBS="-l$5 $7"
	])
    ])
    df_check_library_error="success"
    m4_if([$3],[],,[
	df_check_library_saved_cflags="$CFLAGS"
	CFLAGS="[$]$1[_CFLAGS]"
	AC_CHECK_HEADERS([$3], [# Do nothing on success] , [
		df_check_library_error="missing header file"; break;
	    ], [$4])
	CFLAGS="$df_check_library_saved_cflags"
    ])
    m4_if([$5$6],[],,
	[$5],,[AC_MSG_ERROR([No library name provided to DF_CHECK_LIBRARY for function $6])],
	[$6],,[AC_MSG_ERROR([No function name provided to DF_CHECK_LIBRARY for library $5])],
        [AS_IF([test "$df_check_library_error" = "success" ], [
	    AC_CHECK_LIB([$5], [$6], [
		    LIBS="[$][$1][_LIBS] $LIBS"
		    AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_LIB$5), [1], [Define this symbol for $2 package.])
		    $8
		], [
		    df_check_library_error="missing library"
		], [$][$1][_LIBS])
	])
    ])
    AS_IF([test "$df_check_library_error" != "success" ], [
	# One of the checks for $1/$2/$5 failed.
	$9
	m4_if([$1],[],,[
	    $1[]_CFLAGS=""
	    $1[]_LIBS=""
	])
    ])
])

