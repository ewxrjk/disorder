#
# This file is part of DisOrder.
# Copyright (C) 2004, 2005, 2007-2009 Richard Kettlewell
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

noinst_PROGRAMS=disorder-udplog

AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib

disorder_udplog_SOURCES=udplog.c ../lib/memgc.c
disorder_udplog_LDADD=$(LIBOBJS) ../lib/libdisorder.a $(LIBGC)
disorder_udplog_DEPENDENCIES=../lib/libdisorder.a

TESTS=cookie.py dbversion.py dump.py files.py play.py queue.py	\
	recode.py search.py user.py aliases.py	\
	schedule.py hashes.py playlists.py

TESTS_ENVIRONMENT=${PYTHON} -u

clean-local:
	rm -rf testroot *.log *.pyc

EXTRA_DIST=dtest.py ${TESTS} fail.py
CLEANFILES=*.gcda *.gcov *.gcno *.c.html index.html
