#
# This file is part of DisOrder.
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

noinst_LIBRARIES=libdisorder.a
include_HEADERS=disorder.h

if SERVER
TRACKDB=trackdb.c
else
TRACKDB=trackdb-stub.c
endif

libdisorder_a_SOURCES=charset.c charset.h		\
	addr.c addr.h					\
	alsabg.c alsabg.h				\
	arcfour.c arcfour.h				\
	authhash.c authhash.h				\
	basen.c basen.h					\
	base64.c base64.h				\
	bits.c bits.h					\
	cache.c cache.h					\
	cgi.c cgi.h					\
	client.c client.h				\
	client-common.c client-common.h			\
	configuration.c configuration.h			\
	cookies.c cookies.h				\
	coreaudio.c coreaudio.h				\
	dateparse.c dateparse.h xgetdate.c		\
	defs.c defs.h					\
	eclient.c eclient.h				\
	email.c						\
	eventdist.c eventdist.h				\
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
	macros.c macros-builtin.c macros.h		\
	mem.c mem.h mem-impl.h				\
	mime.h mime.c					\
	mixer.c mixer.h mixer-oss.c mixer-alsa.c	\
	printf.c printf.h				\
	asprintf.c fprintf.c snprintf.c			\
	queue.c queue.h					\
	random.c random.h				\
	regsub.c regsub.h				\
	rights.c queue-rights.c rights.h		\
	rtp.h						\
	selection.c selection.h				\
	sendmail.c sendmail.h				\
	signame.c signame.h				\
	sink.c sink.h					\
	speaker-protocol.c speaker-protocol.h		\
	split.c split.h					\
	strptime.c strptime.h				\
	syscalls.c syscalls.h				\
	common.h					\
	table.c table.h					\
	timeval.h					\
	$(TRACKDB) trackdb.h trackdb-int.h		\
	trackname.c trackorder.c trackname.h		\
	tracksort.c					\
	url.h url.c					\
	user.h user.c					\
	unicode.h unicode.c				\
	unidata.h unidata.c				\
	vacopy.h					\
	vector.c vector.h				\
	version.c version.h				\
	wav.h wav.c					\
	wstat.c wstat.h					\
	disorder.h

version-string: ../config.status ${top_srcdir}/scripts/make-version-string
	CC="${CC}" ${top_srcdir}/scripts/make-version-string > $@.new
	@if cmp $@.new $@; then \
		echo rm -f $@.new; rm -f $@.new; else \
		echo mv $@.new $@; mv $@.new $@; fi

versionstring.h: version-string ${top_srcdir}/scripts/text2c
	${top_srcdir}/scripts/text2c -extern disorder_version_string \
		version-string > $@.new
	@if cmp $@.new $@; then \
		echo rm -f $@.new; rm -f $@.new; else \
		echo mv $@.new $@; mv $@.new $@; fi

definitions.h: Makefile
	rm -f $@.new
	echo "/** @file lib/definitions.h" >> $@.new
	echo " * @brief Definitions exported from makefile" >> $@.new
	echo " *" >> $@.new
	echo " * DO NOT EDIT." >> $@.new
	echo " */" >> $@.new
	echo "#define PKGLIBDIR \"${pkglibdir}\"" >> $@.new
	echo "#define PKGCONFDIR \"${sysconfdir}/\"PACKAGE" >> $@.new
	echo "#define PKGSTATEDIR \"${localstatedir}/\"PACKAGE" >> $@.new
	echo "#define PKGDATADIR \"${pkgdatadir}/\"" >> $@.new
	echo "#define SBINDIR \"${sbindir}/\"" >> $@.new
	echo "#define BINDIR \"${bindir}/\"" >> $@.new
	echo "#define FINKBINDIR \"${finkbindir}/\"" >> $@.new
	echo "#define SENDMAIL \"${SENDMAIL}\"" >> $@.new
	@if cmp $@.new $@; then \
		echo rm -f $@.new; rm -f $@.new; else \
		echo mv $@.new $@; mv $@.new $@; fi
defs.o: definitions.h versionstring.h
defs.lo: definitions.h versionstring.h

rebuild-unicode:
	cd ${srcdir} && ${top_srcdir}/scripts/make-unidata

%.i: %.c
	$(CPP) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) -c $< > $@.new
	mv $@.new $@

CLEANFILES=definitions.h definitions.h.new version-string versionstring.h \
	   *.gcda *.gcov *.gcno *.c.html index.html

EXTRA_DIST=trackdb.c trackdb-stub.c
