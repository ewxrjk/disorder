#
# This file is part of DisOrder.
# Copyright (C) 2004-2008 Richard Kettlewell
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

cgiexec_PROGRAMS=disorder

AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib

disorder_SOURCES=macros-disorder.c lookup.c options.c actions.c	\
	login.c cgimain.c disorder-cgi.h
disorder_LDADD=../lib/libdisorder.a \
	$(LIBPCRE) $(LIBGCRYPT) $(LIBDL) $(LIBDB)
disorder_LDFLAGS=-export-dynamic
disorder_DEPENDENCIES=../lib/libdisorder.a

install-exec-hook:
	$(LIBTOOL) --mode=finish $(DESTDIR)$(cgiexecdir)
