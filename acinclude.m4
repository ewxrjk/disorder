# This file is part of DisOrder.
# Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

AC_DEFUN([RJK_FIND_GC_H],[
  AC_CACHE_CHECK([looking for <gc.h>],[rjk_cv_gc_h],[
    AC_PREPROC_IFELSE([AC_LANG_PROGRAM([
		       #include <gc.h>
		      ],[])],
		      [rjk_cv_gc_h="on default include path"],[
      oldCPPFLAGS="${CPPFLAGS}"
      for dir in /usr/include/gc /usr/local/include/gc; do
	if test "x$GCC" = xyes; then
	  CPPFLAGS="${oldCPPFLAGS} -isystem $dir"
	else
	  CPPFLAGS="${oldCPPFLAGS} -I$dir"
	fi
	AC_PREPROC_IFELSE([AC_LANG_PROGRAM([
			   #include <gc.h>
			  ],[])],
			  [rjk_cv_gc_h=$dir;break],[rjk_cv_gc_h="not found"])
      done
      CPPFLAGS="${oldCPPFLAGS}"
   ])
  ])
  case "$rjk_cv_gc_h" in
  "not found" )
    missing_headers="$missing_headers gc.h"
    ;;
  /* )
    if test "x$GCC" = xyes; then
      CPPFLAGS="${CPPFLAGS} -isystem $rjk_cv_gc_h"
    else
      CPPFLAGS="${CPPFLAGS} -I$rjk_cv_gc_h"
    fi
    ;;
  esac
])

AC_DEFUN([RJK_CHECK_LIB],[
  AC_CACHE_CHECK([for $2 in -l$1],[rjk_cv_lib_$1_$2],[
    save_LIBS="$LIBS"
    LIBS="${LIBS} -l$1"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([$3],[$2;])],
                   [rjk_cv_lib_$1_$2=yes],
                   [rjk_cv_lib_$1_$2=no])
    LIBS="$save_LIBS"
  ])
  if test $rjk_cv_lib_$1_$2 = yes; then
    $4
  else
    $5
  fi
])

AC_DEFUN([RJK_REQUIRE_PCRE_UTF8],[
  AC_CACHE_CHECK([whether libpcre was built with UTF-8 support],
                 [rjk_cv_pcre_utf8],[
    save_LIBS="$LIBS"
    LIBS="$LIBS $1"
    AC_RUN_IFELSE([AC_LANG_PROGRAM([
                    #include <pcre.h>
                    #include <stdio.h>
                  ],
                  [
                    pcre *r;
                    const char *errptr;
                    int erroffset;

                    r = pcre_compile("\x80\x80", PCRE_UTF8,
                                     &errptr, &erroffset, 0);
                    if(!r) {
                      fprintf(stderr, "pcre_compile: %s at %d",
                              errptr, erroffset);
                      exit(0);
                    } else {
                      fprintf(stderr, "accepted bogus UTF-8 string\n");
                      exit(1);
                    }
                  ])],
                  [rjk_cv_pcre_utf8=yes],
                  [rjk_cv_pcre_utf8=no],
                  [AC_MSG_WARN([cross-compiling, cannot check libpcre behaviour])])
    LIBS="$save_LIBS"
  ])
  if test $rjk_cv_pcre_utf8 = no; then
    AC_MSG_ERROR([please rebuild your pcre library with --enable-utf8])
  fi
])

AC_DEFUN([RJK_REQUIRE_PCRE2_UTF8],[
  AC_CACHE_CHECK([whether libpcre2 was built with UTF-8 support],
                 [rjk_cv_pcre_utf8],[
    save_CFLAGS="$CFLAGS" save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $1" LIBS="$LIBS $2"
    AC_RUN_IFELSE([AC_LANG_PROGRAM([
                    #define PCRE2_CODE_UNIT_WIDTH 8
                    #include <pcre2.h>
                    #include <stdio.h>
                  ],
                  [
                    pcre2_code *r;
                    int errcode;
                    int erroffset;
                    char errbuf[[128]];

                    r = pcre2_compile("\x80\x80", 2, PCRE2_UTF,
                                     &errcode, &erroffset, 0);
                    if(!r) {
                      pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));
                      fprintf(stderr, "pcre2_compile: %s at %d",
                              errbuf, erroffset);
                      exit(0);
                    } else {
                      fprintf(stderr, "accepted bogus UTF-8 string\n");
                      exit(1);
                    }
                  ])],
                  [rjk_cv_pcre_utf8=yes],
                  [rjk_cv_pcre_utf8=no],
                  [AC_MSG_WARN([cross-compiling, cannot check libpcre2 behaviour])])
    CFLAGS="$save_CFLAGS" LIBS="$save_LIBS"
  ])
  if test $rjk_cv_pcre_utf8 = no; then
    AC_MSG_ERROR([please rebuild your pcre library with --enable-utf8])
  fi
])

AC_DEFUN([RJK_GCOV],[
  GCOV=${GCOV:-true}
  AC_ARG_WITH([gcov],
              [AS_HELP_STRING([--with-gcov],
                              [Enable coverage testing])],
              [if test $withval = yes; then
                 CFLAGS="${CFLAGS} -O0 -fprofile-arcs -ftest-coverage"
                 GCOV=`echo $CC | sed s'/gcc/gcov/;s/ .*$//'`;
               fi])
  AC_SUBST([GCOV],[$GCOV])
])
