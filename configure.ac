# Process this file with autoconf to produce a configure script.
#
# This file is part of DisOrder.
# Copyright (C) 2004, 2005, 2006, 2007 Richard Kettlewell
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

AC_INIT(disorder, 1.5.99+, richard+disorder@sfere.greenend.org.uk)
AC_CONFIG_AUX_DIR([config.aux])
AM_INIT_AUTOMAKE(disorder, 1.5.99+)
AC_CONFIG_SRCDIR([server/disorderd.c])
AM_CONFIG_HEADER([config.h])

# Find host type
AC_CANONICAL_HOST

# What we want to build
want_gtk=yes
want_python=yes

# Checks for programs.
AC_PROG_CC
AC_SET_MAKE
if test "x$GCC" = xyes; then
  gcc_werror=-Werror
else
  gcc_werror=""
fi

AC_MSG_CHECKING([for a known target platform])
case "$host" in
*empeg* )
  AC_DEFINE([EMPEG_HOST],[1],[define if host is an empeg car stereo])
  # work around broken toolchain
  AC_CHECK_LIB([gpg-error], [gpg_strerror])
  want_server=no
  AC_MSG_RESULT([empeg car stereo])
 ;;
*linux* | *Linux* )
  want_server=yes
  AC_MSG_RESULT([Linux])
  ;;
*-apple-darwin* )
  want_server=no
  COREAUDIO="-framework CoreAudio"
  AC_MSG_RESULT([Mac OS X])
  ;;
* )
  want_server=no
  AC_MSG_RESULT([unknown, winging it])
  ;;
esac
AC_SUBST([COREAUDIO])

AC_ARG_WITH([server],
	    [AS_HELP_STRING([--without-server],
			    [do not build server])],
	    [want_server=$withval])
AC_ARG_WITH([gtk],
	    [AS_HELP_STRING([--without-gtk],
			    [do not build GTK+ client])],
	    [want_gtk=$withval])
AC_ARG_WITH([python],
	    [AS_HELP_STRING([--without-python],
			    [do not build Python support])],
	    [want_python=$withval])

subdirs="scripts lib clients doc examples debian"

if test $want_server = yes; then
  subdirs="${subdirs} server plugins driver templates sounds images"
fi
if test $want_python = yes; then
  AM_PATH_PYTHON
  subdirs="${subdirs} python"
fi
if test $want_gtk = yes; then
  subdirs="${subdirs} disobedience"
  if test $want_server = no; then
    subdirs="${subdirs} images"
  fi
fi
subdirs="${subdirs} tests"
AC_SUBST([subdirs])

# libtool config
AC_LIBTOOL_DLOPEN
AC_DISABLE_STATIC

AC_PROG_LIBTOOL

missing_libraries=""
missing_headers=""
missing_functions=""

AC_DEFINE(_GNU_SOURCE, 1, [required for e.g. strsignal])

# Macs might have libraries under fink's root
AC_PATH_PROG([FINK],[fink],[none],[$PATH:/sw/bin])
if test "x$FINK" != xnone; then
  AC_CACHE_CHECK([fink install directory],[rjk_cv_finkprefix],[
    rjk_cv_finkprefix="`echo "$FINK" | sed 's,/bin/fink$,,'`"
  ])
  finkbindir="${rjk_cv_finkprefix}/bin"
  CPPFLAGS="${CPPFLAGS} -I${rjk_cv_finkprefix}/include/gc -I${rjk_cv_finkprefix}/include"
  if test $want_server = yes; then
    CPPFLAGS="${CPPFLAGS} -I${rjk_cv_finkprefix}/include/db4"
  fi
  LDFLAGS="${LDFLAGS} -L${rjk_cv_finkprefix}/lib"
else
  finkbindir=""
fi
AC_SUBST([finkbindir])

# Checks for libraries.
# We save up a list of missing libraries that we can't do without
# and report them all at once.
AC_CHECK_LIB(gc, GC_malloc,            [AC_SUBST(LIBGC,[-lgc])],
            [missing_libraries="$missing_libraries libgc"])
AC_CHECK_LIB(gcrypt, gcry_md_open,
             [AC_SUBST(LIBGCRYPT,[-lgcrypt])],
            [missing_libraries="$missing_libraries libgcrypt"])
AC_CHECK_LIB(pcre, pcre_compile,
	     [AC_SUBST(LIBPCRE,[-lpcre])],
	     [missing_libraries="$missing_libraries libpcre"])
AC_CHECK_LIB([asound], [snd_pcm_open],
             [AC_SUBST(LIBASOUND,[-lasound])])
if test $want_server = yes; then
  RJK_CHECK_LIB(db, db_create, [#include <db.h>],
	       [AC_SUBST(LIBDB,[-ldb])],
	       [missing_libraries="$missing_libraries libdb"])
  AC_CHECK_LIB(vorbis, vorbis_info_clear,
	       [:],
	       [missing_libraries="$missing_libraries libvorbis"])
  AC_CHECK_LIB(vorbisfile, ov_open,
	       [AC_SUBST(LIBVORBISFILE,["-lvorbisfile -lvorbis"])],
	       [missing_libraries="$missing_libraries libvorbisfile"],
	       [-lvorbis])
  AC_CHECK_LIB(mad, mad_stream_init,
	       [AC_SUBST(LIBMAD,[-lmad])],
	       [missing_libraries="$missing_libraries libmad"])
  AC_CHECK_LIB([ao], [ao_initialize],
	       [AC_SUBST(LIBAO,[-lao])],
	       [missing_libraries="$missing_libraries libao"])
  AC_CHECK_LIB([FLAC], [FLAC__stream_decoder_new],
	       [AC_SUBST(LIBFLAC,[-lFLAC])],
	       [missing_libraries="$missing_libraries libFLAC"])
fi

if test $want_gtk = yes; then
  AM_PATH_GLIB_2_0([],[],[missing_libraries="$missing_libraries libglib"])
  AM_PATH_GTK_2_0([],[],[missing_libraries="$missing_libraries libgtk"])
fi

# Some platforms have iconv already
AC_CHECK_FUNC(iconv_open, [:],
              [RJK_CHECK_LIB(iconv, iconv_open, [#include <iconv.h>],
                            [AC_SUBST(LIBICONV,[-liconv])],
                            [missing_functions="$missing_functions iconv_open"])])
AC_CHECK_FUNC([gethostbyname],[:],[
  AC_CHECK_LIB(nsl,gethostbyname,
               [AC_SUBST(LIBNSL,[-lnsl])],
               [missing_functions="$missing_functions gethostbyname"])])
AC_CHECK_FUNC([socket],[:],[
  AC_CHECK_LIB(socket,socket,
               [AC_SUBST(LIBSOCKET,[-lsocket])],
               [missing_functions="$missing_functions socket"])])
AC_CHECK_FUNC([dlopen],[:],[
  AC_CHECK_LIB(dl,dlopen,
	       [AC_SUBST(LIBDL,[-ldl])],
	       [missing_functions="$missing_functions dlopen"])])

if test ! -z "$missing_libraries"; then
  AC_MSG_ERROR([missing libraries:$missing_libraries])
fi

# We require that libpcre support UTF-8
RJK_REQUIRE_PCRE_UTF8([-lpcre])

# Checks for header files.
RJK_FIND_GC_H
AC_CHECK_HEADERS([inttypes.h CoreAudio/AudioHardware.h sys/soundcard.h alsa/asoundlib.h])
# We don't bother checking very standard stuff
# Compilation will fail if any of these headers are missing, so we
# check for them here and fail early.
if test $want_server = yes; then
  AC_CHECK_HEADERS([db.h],[:],[
    missing_headers="$missing_headers $ac_header"
  ])
fi
AC_CHECK_HEADERS([dlfcn.h gcrypt.h \
		 getopt.h iconv.h langinfo.h \
		 pcre.h sys/ioctl.h \
	         syslog.h unistd.h],[:],[
  missing_headers="$missing_headers $ac_header"
])

if test ! -z "$missing_headers"; then
  AC_MSG_ERROR([missing headers:$missing_headers])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_C_INLINE
AC_C_BIGENDIAN
AC_CHECK_TYPES([struct sockaddr_in6],,,[AC_INCLUDES_DEFAULT
#include <netinet/in.h>])

# enable -Werror when we check for certain characteristics:

old_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} $gcc_werror"
AC_CHECK_TYPES([long long,uint32_t,uint8_t,intmax_t,uintmax_t])

# Some GCC invocations warn for converting function pointers to void *.
# This is fair enough, as it's technically forbidden, but we use dlsym()
# which can pretty much only exist if object and function pointers are
# interconvertable.  So we disable -Werror if need be.
if test ! -z "$gcc_werror"; then
  AC_CACHE_CHECK([whether function pointers can be converted to void * without a warning],
		 [rjk_cv_function_pointer_cast],[
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT
  void somefunction(void);],
		      [(void *)somefunction])],
		      [rjk_cv_function_pointer_cast=yes],
		      [rjk_cv_function_pointer_cast=no])])
  if test $rjk_cv_function_pointer_cast = no; then
    gcc_werror=""
  fi
fi

CFLAGS="${old_CFLAGS}"

# gcrypt maintainers keep changing everything.  Design your interface
# first, then implement it once, rather than getting it wrong three or
# four times and shipping between each attempt.
AC_CACHE_CHECK([for hash handle type in <grypt.h>],
               [rjk_cv_gcrypt_hash_handle],[
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT
#include <gcrypt.h>
],
               [gcry_md_hd_t h;])],
               [rjk_cv_gcrypt_hash_handle=gcry_md_hd_t],[
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT
#include <gcrypt.h>
],
		 [GcryMDHd h;])],
		 [rjk_cv_gcrypt_hash_handle=GcryMDHd],
		 [rjk_cv_gcrypt_hash_handle=GCRY_MD_HD])])])
AC_DEFINE_UNQUOTED([gcrypt_hash_handle],[$rjk_cv_gcrypt_hash_handle],
                   [libgcrypt hash handle type])

AC_CACHE_CHECK([for gcry_error_t in <grypt.h>],
               [rjk_cv_have_gcry_error_t],[
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT
#include <gcrypt.h>
],
		 [gcry_error_t e;])],
		 [rjk_cv_have_gcry_error_t=yes],
		 [rjk_cv_have_gcry_error_t=no])])
if test $rjk_cv_have_gcry_error_t = yes; then
  AC_DEFINE([HAVE_GCRY_ERROR_T],1,[define if <gcrypt.h> defines gcry_error_t])
fi

# Checks for functions
if test $ac_cv_type_long_long = yes; then
  AC_CHECK_FUNCS([atoll strtoll],[:],[
    missing_functions="$missing_functions $ac_func"
  ])
  # Darwin sometimes fails to declare strtoll (e.g. if you ask for -std=c99)
  AC_CACHE_CHECK([whether strtoll is declared in <stdlib.h>],
                 [rjk_cv_strtoll_declared],[
    AC_EGREP_HEADER([strtoll], [stdlib.h],
                    [rjk_cv_strtoll_declared=yes],
                    [rjk_cv_strtoll_declared=no])])
  if test $rjk_cv_strtoll_declared = yes; then
    AC_DEFINE([DECLARES_STRTOLL],[1],[define if <stdlib.h> declares strtoll])
  fi
  AC_CACHE_CHECK([whether atoll is declared in <stdlib.h>],
                 [rjk_cv_atoll_declared],[
    AC_EGREP_HEADER([atoll], [stdlib.h],
                    [rjk_cv_atoll_declared=yes],
                    [rjk_cv_atoll_declared=no])])
  if test $rjk_cv_atoll_declared = yes; then
    AC_DEFINE([DECLARES_ATOLL],[1],[define if <stdlib.h> declares atoll])
  fi
fi
AC_CHECK_FUNCS([ioctl nl_langinfo strsignal],[:],[
  missing_functions="$missing_functions $ac_func"
])
# fsync will do if fdatasync not available
AC_CHECK_FUNCS([fdatasync],[:],[
  AC_CHECK_FUNCS([fsync],
                 [AC_DEFINE([fdatasync],[fsync],[define fdatasync to fsync if not available])],
                 [missing_functions="$missing_functions fdatasync"])])
if test ! -z "$missing_functions"; then
  AC_MSG_ERROR([missing functions:$missing_functions])
fi
if test $want_server = yes; then
  # <db.h> had better be version 3 or later
  AC_CACHE_CHECK([db.h version],[rjk_cv_db_version],[
    AC_PREPROC_IFELSE([
		       #include <db.h>
		       #ifndef DB_VERSION_MAJOR
		       # error cannot determine db version
		       #endif
		       #if DB_VERSION_MAJOR < 4
		       # error inadequate db version
		       #endif
		       #if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR < 2
		       # error inadequate db version
		       #endif
		      ],
		      [rjk_cv_db_version=ok],
		      [rjk_cv_db_version=inadequate])
  ])
  if test $rjk_cv_db_version != ok; then
    AC_MSG_ERROR([need db.h version at least 4.2])
  fi
fi

if test "x$GCC" = xyes; then
  # a reasonable default set of warnings
  CC="${CC} -Wall -W -Wpointer-arith -Wbad-function-cast \
	-Wwrite-strings -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs"

  # Fix up GTK+ and GLib compiler flags
  GTK_CFLAGS="`echo \"$GTK_CFLAGS\"|sed 's/-I/-isystem /g'`"
  GLIB_CFLAGS="`echo \"$GLIB_CFLAGS\"|sed 's/-I/-isystem /g'`"

  # GCC 2.95 doesn't know to ignore warnings from system headers
  AC_CACHE_CHECK([whether -Werror is usable],
                  rjk_cv_werror, [
    save_CFLAGS="${CFLAGS}"
    CFLAGS="${CFLAGS} ${GTK_CFLAGS} -Werror"
    AC_TRY_COMPILE([#include <gtk/gtk.h>],
                   [],
                   [rjk_cv_werror=yes],
                   [rjk_cv_werror=no])
    CFLAGS="${save_CFLAGS}"
  ])
  if test $rjk_cv_werror = no; then
    gcc_werror=''
  fi
  CC="${CC} $gcc_werror"

  # for older GCCs that don't know %ju (etc)
  AC_CACHE_CHECK([whether -Wno-format is required],
		 rjk_cv_noformat,
		 AC_TRY_COMPILE([#include <stdio.h>
#include <stdint.h>
],
			        [printf("%ju", (uintmax_t)0);],
				[rjk_cv_noformat=no],
				[rjk_cv_noformat=yes]))
  if test $rjk_cv_noformat = yes; then
    CC="${CC} -Wno-format"
  fi

  AC_CACHE_CHECK([whether -Wshadow is OK],
		 rjk_cv_shadow,
                 oldCC="${CC}"
		 CC="${CC} -Wshadow"
		 [AC_TRY_COMPILE([
#include <unistd.h>
#include <vorbis/vorbisfile.h>
],
				[],
				[rjk_cv_shadow=yes],
				[rjk_cv_shadow=no])
		 CC="${oldCC}"])
  if test $rjk_cv_shadow = yes; then
    CC="${CC} -Wshadow"
  fi
                  

fi

AH_BOTTOM([#ifdef __GNUC__
# define attribute(x) __attribute__(x)
#else
# define attribute(x)
#endif])

AC_CONFIG_FILES([Makefile
		 templates/Makefile
		 images/Makefile
		 scripts/Makefile
		 lib/Makefile
		 server/Makefile
		 clients/Makefile
		 disobedience/Makefile
		 doc/Makefile
		 plugins/Makefile
		 driver/Makefile
		 debian/Makefile
		 sounds/Makefile
		 python/Makefile
		 examples/Makefile
		 tests/Makefile])
AC_OUTPUT
