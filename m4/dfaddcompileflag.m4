# 
# Check to see if a compiler accepts a certain flag, and if it
# does, add it to the compilation flags.
# @file       dfaddcompileflag.cpp - DF_ADD_COMPILE_FLAG
# @author     Perette Barella
# @date       2020-12-07
# @copyright  Copyright 2020 Devious Fish. All rights reserved.
#
# DF_ADD_COMPILE_FLAG([language],[variable],[flag]])

AC_DEFUN([DF_ADD_COMPILE_FLAG], [
    AC_MSG_CHECKING([$1 compilation flag $3])
    AC_LANG_PUSH([$1])
    saved_$2_flags="[$]$2"
    $2="[$]$2 $3"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[int x = 3;]])],
      [ AC_MSG_RESULT(yes) ],
      [ AC_MSG_RESULT(no)
        $2="[$]saved_$2_flags" ]
    )
    AC_LANG_POP([$1])
])


