#
# This file is part of DisOrder.
# Copyright (C) 2006, 2007 Richard Kettlewell
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

bin_PROGRAMS=disobedience

AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib
AM_CFLAGS=$(GLIB_CFLAGS) $(GTK_CFLAGS)

disobedience_SOURCES=disobedience.h disobedience.c client.c queue.c	\
		  choose.c misc.c style.h control.c properties.c menu.c \
		  log.c progress.c \
		  ../lib/memgc.c
disobedience_LDADD=../lib/libdisorder.a $(LIBPCRE) $(LIBGC) $(LIBGCRYPT)
disobedience_LDFLAGS=$(GTK_LIBS)

install-exec-hook:
	$(LIBTOOL) --mode=finish $(DESTDIR)$(libdir)

check: check-help

disobedience.o: style.h

style.h: ${srcdir}/disobedience.rc ${top_srcdir}/scripts/text2c
	${top_srcdir}/scripts/text2c style ${srcdir}/disobedience.rc > style.h.tmp
	mv style.h.tmp style.h

EXTRA_DIST=disobedience.rc

# check everything has working --help
check-help: all
	./disobedience --version > /dev/null
