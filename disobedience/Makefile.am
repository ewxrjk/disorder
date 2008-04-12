#
# This file is part of DisOrder.
# Copyright (C) 2006-2008 Richard Kettlewell
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
pkgdata_DATA=disobedience.html

AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib
AM_CFLAGS=$(GLIB_CFLAGS) $(GTK_CFLAGS)
PNGS:=$(shell export LC_COLLATE=C;echo ${top_srcdir}/images/*.png)

disobedience_SOURCES=disobedience.h disobedience.c client.c queue.c	\
		  choose.c misc.c control.c properties.c menu.c \
		  log.c progress.c login.c rtp.c help.c \
		  ../lib/memgc.c settings.c users.c
disobedience_LDADD=../lib/libdisorder.a $(LIBPCRE) $(LIBGC) $(LIBGCRYPT) \
	$(LIBASOUND) $(COREAUDIO) $(LIBDB)
disobedience_LDFLAGS=$(GTK_LIBS)

install-exec-hook:
	$(LIBTOOL) --mode=finish $(DESTDIR)$(libdir)

check: check-help

disobedience.html: ../doc/disobedience.1 $(top_srcdir)/scripts/htmlman
	rm -f $@.new
	$(top_srcdir)/scripts/htmlman $< >$@.new
	chmod 444 $@.new
	mv -f $@.new $@

misc.o: images.h

images.h: $(PNGS)
	exec > @$.new;							\
	for png in $(PNGS); do						\
	  name=`echo $$png | $(GNUSED) 's,.*/,,;s,\.png,,;'`;			\
	  gdk-pixbuf-csource --raw --name=image_$$name $$png;		\
	done;								\
	echo "static const struct image images[] = {";			\
	for png in $(PNGS); do						\
	  name=`echo $$png | $(GNUSED) 's,.*/,,;s,\.png,,;'`;			\
	  echo "  { \"$$name.png\", image_$$name },";			\
	done;								\
	echo "};"
	mv @$.new $@

# check everything has working --help
check-help: all
	unset DISPLAY;./disobedience --version > /dev/null
	unset DISPLAY;./disobedience --help > /dev/null

export GNUSED
