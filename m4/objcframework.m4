# AC_CHECK_FRAMEWORK_OBJC(FRAMEWORK, INCLUDES, OBJC_CODE,
#              [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
#              [OTHER-LIBRARIES])
# ------------------------------------------------------
#
# It is on purpose we used `ac_check_framework_save_LIBS' and not just
# `ac_save_LIBS': there are many macros which don't want to see `LIBS'
# changed but still want to use AC_CHECK_FRAMEWORK, so they save `LIBS'.
# And ``ac_save_LIBS' is too tempting a name, so let's leave them some
# freedom.
AC_DEFUN([AC_CHECK_FRAMEWORK_OBJC], [
	AC_MSG_CHECKING([for $1 framework])
	ac_check_framework_objc_saved_LIBS="$LIBS"
	LIBS="-framework $1 $6 $LIBS"
	AC_LINK_IFELSE([AC_LANG_PROGRAM([$2], [$3])],[
		AC_MSG_RESULT(yes)
	        m4_default([$4], [AC_DEFINE(AS_TR_CPP([HAVE_FRAMEWORK_$1]), [1],
			[Define this symbol to if you have $1 framework.])])
	], [	AC_MSG_RESULT(no)
		LIBS="$ac_check_framework_objc_saved_LIBS"
		$5
	])
])

