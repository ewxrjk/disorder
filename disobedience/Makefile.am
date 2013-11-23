#
# This file is part of DisOrder.
# Copyright (C) 2006-2013 Richard Kettlewell
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

bin_PROGRAMS=disobedience
SUBDIRS=manual

AUTOMAKE_OPTIONS=subdir-objects
AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib
AM_CFLAGS=$(GLIB_CFLAGS) $(GTK_CFLAGS)

disobedience_SOURCES=disobedience.h disobedience.c client.c queue.c	\
	recent.c added.c queue-generic.c queue-generic.h queue-menu.c	\
	choose.c choose-menu.c choose-search.c popup.c misc.c		\
	control.c properties.c menu.c log.c progress.c login.c rtp.c	\
	help.c ../lib/memgc.c settings.c users.c lookup.c choose.h      \
	popup.h playlists.c multidrag.c multidrag.h autoscroll.c	\
	autoscroll.h globals.c
disobedience_LDADD=../lib/libdisorder.a $(LIBPCRE) $(LIBGC) $(LIBGCRYPT) \
	$(LIBASOUND) $(COREAUDIO) $(LIBICONV) -lm \
	$(PULSEAUDIO_SIMPLE_LIBS) $(PULSEAUDIO_LIBS)
disobedience_LDFLAGS=$(GTK_LIBS)

install-data-local:
	$(MKDIR_P) ${DESTDIR}${datarootdir}/applications
	$(INSTALL) -m 644 ${srcdir}/disobedience.desktop \
		${DESTDIR}${datarootdir}/applications/disobedience.desktop
	$(MKDIR_P) ${DESTDIR}${datarootdir}/icons/hicolor/48x48/apps
	$(INSTALL) -m 644 ${top_srcdir}/images/disobedience48x48.png \
		${DESTDIR}${datarootdir}/icons/hicolor/48x48/apps/disobedience.png
	$(MKDIR_P) ${DESTDIR}${datarootdir}/icons/hicolor/64x64/apps
	$(INSTALL) -m 644 ${top_srcdir}/images/disobedience64x64.png \
		${DESTDIR}${datarootdir}/icons/hicolor/64x64/apps/disobedience.png
	$(MKDIR_P) ${DESTDIR}${datarootdir}/icons/hicolor/128x128/apps
	$(INSTALL) -m 644 ${top_srcdir}/images/disobedience128x128.png \
		${DESTDIR}${datarootdir}/icons/hicolor/128x128/apps/disobedience.png
	$(MKDIR_P) ${DESTDIR}${datarootdir}/icons/hicolor/256x256/apps
	$(INSTALL) -m 644 ${top_srcdir}/images/disobedience256x256.png \
		${DESTDIR}${datarootdir}/icons/hicolor/256x256/apps/disobedience.png

check: check-help

misc.o: ../images/images.h

EXTRA_DIST=disobedience.desktop

# check everything has working --help
check-help: all
	unset DISPLAY;./disobedience --version > /dev/null
	unset DISPLAY;./disobedience --help > /dev/null

CLEANFILES=*.gcda *.gcov *.gcno *.c.html index.html

export GNUSED
