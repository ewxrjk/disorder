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

noinst_LIBRARIES=libdisorder.a
include_HEADERS=disorder.h
noinst_PROGRAMS=test

libdisorder_a_SOURCES=charset.c charset.h		\
	addr.c addr.h					\
	authhash.c authhash.h				\
	basen.c basen.h					\
	base64.c base64.h				\
	cache.c cache.h					\
	client.c client.h				\
	client-common.c client-common.h			\
	configuration.c configuration.h			\
	cookies.c cookies.h				\
	defs.c defs.h					\
	eclient.c eclient.h				\
	event.c event.h 				\
	eventlog.c eventlog.h 				\
	filepart.c filepart.h				\
	hash.c hash.h					\
	heap.h						\
	hex.c hex.h					\
	hostname.c hostname.h				\
	ifreq.c ifreq.h					\
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
	rights.c queue-rights.c rights.h		\
	rtp.h						\
	selection.c selection.h				\
	sendmail.c sendmail.h				\
	signame.c signame.h				\
	sink.c sink.h					\
	speaker-protocol.c speaker-protocol.h		\
	split.c split.h					\
	syscalls.c syscalls.h				\
	types.h						\
	table.c table.h					\
	timeval.h					\
	trackdb.h trackdb.c trackdb-int.h		\
	trackname.c trackname.h				\
	url.h url.c					\
	user.h user.c					\
	unicode.h unicode.c				\
	unidata.h unidata.c				\
	vacopy.h					\
	vector.c vector.h				\
	wav.h wav.c					\
	wstat.c wstat.h					\
	disorder.h

version-string: ../config.status ${top_srcdir}/scripts/make-version-string
	CC="${CC}" ${top_srcdir}/scripts/make-version-string > $@.new
	@if cmp $@.new $@; then \
		echo rm -f $@.new; rm -f $@.new; else \
		echo mv $@.new $@; mv $@.new $@; fi

version.h: version-string ${top_srcdir}/scripts/text2c
	${top_srcdir}/scripts/text2c -extern disorder_version_string \
		version-string > $@.new
	@if cmp $@.new $@; then \
		echo rm -f $@.new; rm -f $@.new; else \
		echo mv $@.new $@; mv $@.new $@; fi

definitions.h: Makefile
	rm -f $@.new
	echo "#define PKGLIBDIR \"${pkglibdir}\"" > $@.new
	echo "#define PKGCONFDIR \"${sysconfdir}/\"PACKAGE" >> $@.new
	echo "#define PKGSTATEDIR \"${localstatedir}/\"PACKAGE" >> $@.new
	echo "#define PKGDATADIR \"${pkgdatadir}/\"" >> $@.new
	echo "#define SBINDIR \"${sbindir}/\"" >> $@.new
	echo "#define BINDIR \"${bindir}/\"" >> $@.new
	echo "#define FINKBINDIR \"${finkbindir}/\"" >> $@.new
	echo "#define DOCDIR \"${docdir}/\"" >> $@.new
	@if cmp $@.new $@; then \
		echo rm -f $@.new; rm -f $@.new; else \
		echo mv $@.new $@; mv $@.new $@; fi
defs.o: definitions.h version.h
defs.lo: definitions.h version.h

test_SOURCES=test.c memgc.c
test_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
test_DEPENDENCIES=libdisorder.a

check: test #test.i
	./test

check-report: before-check check make-coverage-reports
before-check:
	rm -f *.gcda *.gcov
make-coverage-reports:
	${GCOV} *.c | ${PYTHON} ../scripts/format-gcov-report --html . *.c

rebuild-unicode:
	cd ${srcdir} && ${top_srcdir}/scripts/make-unidata

%.i: %.c
	$(CPP) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) -c $< > $@.new
	mv $@.new $@

CLEANFILES=definitions.h definitions.h.new
