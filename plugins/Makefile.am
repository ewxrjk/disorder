#
# This file is part of DisOrder
# Copyright (C) 2004-2008 Richard Kettlewell
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

pkglib_LTLIBRARIES=disorder-tracklength.la fs.la notify.la exec.la shell.la \
		   execraw.la
AM_CPPFLAGS=-I${top_srcdir}/lib

notify_la_SOURCES=notify.c
notify_la_LDFLAGS=-module

disorder_tracklength_la_SOURCES=tracklength.c tracklength.h	\
tracklength-mp3.c tracklength-ogg.c tracklength-wav.c		\
tracklength-flac.c mad.c madshim.h ../lib/wav.h ../lib/wav.c	\
../lib/hreader.h ../lib/hreader.c
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
