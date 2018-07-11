#
# This file is part of DisOrder
# Copyright (C) 2004-2008, 2010, 2013 Richard Kettlewell
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

VPATH+=${top_srcdir}/common

pkglib_LTLIBRARIES=disorder-tracklength.la fs.la notify.la exec.la shell.la \
		   execraw.la
check_PROGRAMS=
AUTOMAKE_OPTIONS=subdir-objects
AM_CPPFLAGS=-I${top_srcdir}/lib
AM_CFLAGS=

notify_la_SOURCES=notify.c
notify_la_LDFLAGS=-module

disorder_tracklength_la_SOURCES=tracklength.c tracklength.h	\
tracklength-mp3.c tracklength-ogg.c tracklength-wav.c		\
tracklength-flac.c mad.c madshim.h
nodist_disorder_tracklength_la_SOURCES=wav.c hreader.c
disorder_tracklength_la_LDFLAGS=-module
disorder_tracklength_la_LIBADD=$(LIBVORBISFILE) $(LIBMAD) $(LIBFLAC) -lm

fs_la_SOURCES=fs.c
fs_la_LDFLAGS=-module

exec_la_SOURCES=exec.c
exec_la_LDFLAGS=-module

execraw_la_SOURCES=execraw.c
execraw_la_LDFLAGS=-module

shell_la_SOURCES=shell.c
shell_la_LDFLAGS=-module

if GSTDECODE
AM_CFLAGS+=$(GSTREAMER_CFLAGS)
pkglib_LTLIBRARIES+=tracklength-gstreamer.la
tracklength_gstreamer_la_SOURCES=tracklength-gstreamer.c
tracklength_gstreamer_la_LDFLAGS=-module
tracklength_gstreamer_la_LIBADD=$(LIBOBJS) \
	$(GSTREAMER_PLUGINS_BASE_LIBS) \
	$(GSTREAMER_LIBS)
check_PROGRAMS+=tracklength-gstreamer-test
tracklength_gstreamer_test_SOURCES=tracklength-gstreamer.c
tracklength_gstreamer_test_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(GSTREAMER_PLUGINS_BASE_LIBS) \
	$(GSTREAMER_LIBS)
tracklength_gstreamer_test_CFLAGS=$(AM_CFLAGS) $(CFLAGS) -DSTANDALONE
endif
