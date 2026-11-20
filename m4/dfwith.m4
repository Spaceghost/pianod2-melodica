#
# Tri-state logic functions for autoconf with-/without-option handling.
# @file       dfwith.cpp - DF_DO_WITHOUT, DF_DECIDE_WITH, DF_UNNEEDED
# @author     Perette Barella
# @date       2015-11-19
# @copyright  Copyright 2015-2017 Devious Fish. All rights reserved.
#

# Overview
#   There's a paradigm for configurating with other libraries involving
#   three states: yes (include it), no (don't include it), and check
#   (see if it's available and include it).  Autoconf is missing any
#   meaningful configuration for these; I found myself repeatedly
#   reinventing wheels I'd seen otherwise, and lacking any explanation
#   or feeback to the user to boot.  These functions are intended to
#   fix this.
#
# Use
# - Early in your script, set your variable (with_<package-id>) to "check".
# - Use AC_ARG_WITH arguments to override the value to "yes" or "no".
# - In checks, use DF_DO_WITHOUT when you realize a package missing or
#   unusable, or DF_UNNEEDED if a package isn't needed (perhaps it's
#   redundant with one encountered earlier).
# - Invoke DF_DECIDE_WITH when the last of the checks are complete.
#   (Your with_-variable will now be either "yes" or "no", or configure
#   may have aborted if a conflict was encountered.).

# DF_DO_WITHOUT(package-id, [reason], [alternate package-name])
# Do without the specified package-id.
# - If with_<package-id> is "yes", gives an error.
# - If with_<package-id> is "check", gives a warning and sets it to "no".
# - If with_<package-id> is already "no", throws a warning.
# -------------------------------------------------------------
AC_DEFUN([DF_DO_WITHOUT],[
	m4_if([$1],[],[m4_fatal([DF_DO_WITHOUT called without arguments.])])
	AS_IF([test "$with_$1" = "no"],
		[AC_MSG_NOTICE([Redundant check for m4_default([$3],[$1]); still not available.])],
	      [test "$with_$1" = "yes"],
		[AS_IF([test "$enable_sanity" = "no"],
		    [AC_MSG_WARN([m4_default([$2],[m4_default([$3],[$1]) not available]), but allowing --with-$1 because insanity.])],
		    [AC_MSG_ERROR([m4_default([$2],[m4_default([$3],[$1]) not available]), conflicts with --with-$1.])]
		)],
	      [test "$with_$1" = "check"],
		[AC_MSG_NOTICE([m4_default([$2],[m4_default([$3],[$1]) not available]); removed m4_default([$3],[$1]) from build.])
		 with_$1=no],
	      [AC_MSG_WARN([m4_default([$2],[Unexpected value '$with_$1' in with_$1; removed m4_default([$2],[$1]) from build.])])
	        with_$1=no]
	)
])

# DF_UNNEEDED(package-id, [reason], [alternate package-name])
# Indicate the specified package-id isn't needed.
# - If with_<package-id> is "yes" or "no", do nothing
# - If with_<package-id> is "check", gives a note and sets it to "no".
# -------------------------------------------------------------
AC_DEFUN([DF_UNNEEDED],[
	m4_if([$1],[],[m4_fatal([DF_DO_WITHOUT called without arguments.])])
	AS_IF([test "$with_$1" != "yes" -a "$with_$1" != "no" ],
		[AC_MSG_NOTICE([m4_default([$2],[m4_default([$3],[$1]) not needed]); removed m4_default([$3],[$1]) from build.])
		 with_$1=no]
	)
])

# DF_DECIDE_WITH(package-id, [alternate package-name], [defined variable comment])
# Do without the specified package-id.
# - If with_<package-id> is "no", do nothing (reasons will previously have been given).
# - If with_<package-id> is "yes", notify that all is good.
# - If with_<package-id> is "check", set it to "yes" and announce.
# ----------------------------------------------------------------------
AC_DEFUN([DF_DECIDE_WITH],[
	m4_if([$1],[],[m4_fatal([DF_DECIDE_WITH called without arguments.])])
	AS_IF([test "$with_$1" = "yes"],
		[AC_MSG_NOTICE([m4_default([$2],[$1]) requirements have been met.])],
	      [test "$with_$1" = "check"],
		[AC_MSG_NOTICE([m4_default([$2],[$1]) requirements are met, including in build.])
		 with_$1=yes],
	      [test "$with_$1" != "no"],
	      [AC_MSG_WARN([Unexpected value '$with_$1' in with_$1; m4_default([$2],[$1]) disabled.])
	       with_$1=no]
	)
	AS_IF([test "$with_$1" = "yes" ],
		[AC_DEFINE(AS_TR_CPP([HAVE_$1]), [1], m4_quote(m4_default([$3],[Define to compile with m4_default([$2],[$1]) included.))])]
	)
])

