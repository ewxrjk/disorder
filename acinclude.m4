# This file is part of DisOrder.
# Copyright (C) 2004, 2005 Richard Kettlewell
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA
#

AC_DEFUN([RJK_FIND_GC_H],[
  AC_CACHE_CHECK([looking for <gc.h>],[rjk_cv_gc_h],[
    AC_PREPROC_IFELSE([
		       #include <gc.h>
		      ],
		      [rjk_cv_gc_h="on default include path"],[
      oldCPPFLAGS="${CPPFLAGS}"
      for dir in /usr/include/gc /usr/local/include/gc; do
	if test "x$GCC" = xyes; then
	  CPPFLAGS="${oldCPPFLAGS} -isystem $dir"
	else
	  CPPFLAGS="${oldCPPFLAGS} -I$dir"
	fi
	AC_PREPROC_IFELSE([
			   #include <gc.h>
			  ],
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
                  [AC_MSG_ERROR([cross-compiling, cannot check libpcre behaviour])])
    LIBS="$save_LIBS"
  ])
  if test $rjk_cv_pcre_utf8 = no; then
    AC_MSG_ERROR([please rebuild your pcre library with --enable-utf8])
  fi
])
