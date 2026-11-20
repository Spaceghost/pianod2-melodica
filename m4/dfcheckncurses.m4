# 
# Support functions for ncurses library & include checking.
# @file       dfncurses.cpp - DF_CHECK_NCURSES
# @author     Perette Barella
# @date       2020-05-11
# @copyright  Copyright 2020-2022 Devious Fish. All rights reserved.
#
# DF_CHECK_NCURSES([additional-yes],[additional-no])
# Defines HAVE_NCURSES and sets ncurses_LIBS if it is available
# (both header and library).
# Precursor: Set the compilation language must be set to C or C++.

AC_DEFUN([DF_CHECK_NCURSES], [
    ncurses_check_saved_LIBS="$LIBS"
    LIBS="$LIBS -lncurses"
    AC_MSG_CHECKING([for ncurses])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <curses.h>
#include <term.h>], [
	int foo = setupterm ("foo", 0, NULL);
	const char *bar = tiparm ("baka", 72);
     ])], [
	AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_NCURSES], [1], [Define this symbol if you have NCURSES support])
        AC_SUBST([ncurses_LIBS], [-lncurses])
        $2
    ], [
	AC_MSG_RESULT(no)
        $3
    ])
    LIBS="$ncurses_check_saved_LIBS"
])
