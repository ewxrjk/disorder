#
# This file is part of DisOrder.
# Copyright (C) 2004, 2005, 2006 Richard Kettlewell
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

lib_LTLIBRARIES=libdisorder.la
include_HEADERS=disorder.h
noinst_PROGRAMS=test

# This library is there to share code between the DisOrder applications,
# not to provide an interface to anyone else.  As such there are no
# guarantees regarding its ABI.
libdisorder_la_SOURCES=charset.c charset.h		\
	addr.c addr.h					\
	authhash.c authhash.h				\
	basen.c basen.h					\
	cache.c cache.h					\
	client.c client.h				\
	client-common.c client-common.h			\
	configuration.c configuration.h			\
	defs.c defs.h					\
	eclient.c eclient.h				\
	event.c event.h 				\
	eventlog.c eventlog.h 				\
	filepart.c filepart.h				\
	hash.c hash.h					\
	hex.c hex.h					\
	inputline.c inputline.h				\
	kvp.c kvp.h					\
	log.c log.h log-impl.h				\
	logfd.c logfd.h					\
	mem.c mem.h mem-impl.h				\
	mime.h mime.c					\
	mixer.c mixer.h					\
	plugin.c plugin.h				\
	printf.c printf.h				\
	asprintf.c fprintf.c snprintf.c			\
	queue.c queue.h					\
	regsub.c regsub.h				\
	selection.c selection.h				\
	signame.c signame.h				\
	sink.c sink.h					\
	speaker.c speaker.h				\
	split.c split.h					\
	syscalls.c syscalls.h				\
	types.h						\
	table.c table.h					\
	trackname.c trackname.h				\
	user.h user.c					\
	utf8.h utf8.c					\
	vacopy.h					\
	vector.c vector.h				\
	words.c words.h casefold.h unicodegc.h 		\
	wstat.c wstat.h					\
	disorder.h
libdisorder_la_LIBADD=$(LIBGCRYPT) $(LIBGC) $(LIBICONV) $(LIBNSL) \
	$(LIBSOCKET) $(LIBDL) $(LIBPCRE)
libdisorder_la_LDFLAGS=-release ${VERSION}

definitions.h: Makefile
	rm -f $@.new
	echo "#define PKGLIBDIR \"${pkglibdir}\"" > $@.new
	echo "#define PKGCONFDIR \"${sysconfdir}/\"PACKAGE" >> $@.new
	echo "#define PKGSTATEDIR \"${localstatedir}/\"PACKAGE" >> $@.new
	echo "#define PKGDATADIR \"${pkgdatadir}/\"" >> $@.new
	mv $@.new $@
defs.o: definitions.h
defs.lo: definitions.h

test_SOURCES=test.c
test_LDADD=libdisorder.la $(LIBPCRE)
test_DEPENDENCIES=libdisorder.la

check: test
	./test

CLEANFILES=definitions.h
# arch-tag:a8730cb4f8b4517b6e37b1f717956d7c
