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

TESTS=t-addr t-basen t-bits t-cache t-casefold t-cookies \
		t-filepart t-hash t-heap t-hex t-kvp t-mime t-printf \
		t-regsub t-selection t-signame t-sink t-split t-syscalls \
		t-trackname t-unicode t-url t-utf8 t-vector t-words t-wstat \
		t-macros t-cgi

noinst_LIBRARIES=libdisorder.a
include_HEADERS=disorder.h
noinst_PROGRAMS=$(TESTS)

if SERVER
TRACKDB=trackdb.c
else
TRACKDB=trackdb-stub.c
endif

libdisorder_a_SOURCES=charset.c charset.h		\
	addr.c addr.h					\
	alsabg.c alsabg.h				\
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
	macros.c macros-builtin.c macros.h		\
	mem.c mem.h mem-impl.h				\
	mime.h mime.c					\
	mixer.c mixer.h mixer-oss.c mixer-alsa.c	\
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
	$(TRACKDB) trackdb.h trackdb-int.h		\
	trackname.c trackorder.c trackname.h		\
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
	echo "#define PKGLIBDIR \"${pkglibdir}\"" > $@.new
	echo "#define PKGCONFDIR \"${sysconfdir}/\"PACKAGE" >> $@.new
	echo "#define PKGSTATEDIR \"${localstatedir}/\"PACKAGE" >> $@.new
	echo "#define PKGDATADIR \"${pkgdatadir}/\"" >> $@.new
	echo "#define SBINDIR \"${sbindir}/\"" >> $@.new
	echo "#define BINDIR \"${bindir}/\"" >> $@.new
	echo "#define FINKBINDIR \"${finkbindir}/\"" >> $@.new
	@if cmp $@.new $@; then \
		echo rm -f $@.new; rm -f $@.new; else \
		echo mv $@.new $@; mv $@.new $@; fi
defs.o: definitions.h versionstring.h
defs.lo: definitions.h versionstring.h

t_addr_SOURCES=t-addr.c test.c test.h
t_addr_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_addr_DEPENDENCIES=libdisorder.a

t_basen_SOURCES=t-basen.c test.c test.h
t_basen_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_basen_DEPENDENCIES=libdisorder.a

t_bits_SOURCES=t-bits.c test.c test.h
t_bits_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_bits_DEPENDENCIES=libdisorder.a

t_cache_SOURCES=t-cache.c test.c test.h
t_cache_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_cache_DEPENDENCIES=libdisorder.a

t_casefold_SOURCES=t-casefold.c test.c test.h
t_casefold_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_casefold_DEPENDENCIES=libdisorder.a

t_cgi_SOURCES=t-cgi.c test.c test.h
t_cgi_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_cgi_DEPENDENCIES=libdisorder.a

t_cookies_SOURCES=t-cookies.c test.c test.h
t_cookies_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_cookies_DEPENDENCIES=libdisorder.a

t_filepart_SOURCES=t-filepart.c test.c test.h
t_filepart_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_filepart_DEPENDENCIES=libdisorder.a

t_hash_SOURCES=t-hash.c test.c test.h
t_hash_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_hash_DEPENDENCIES=libdisorder.a

t_heap_SOURCES=t-heap.c test.c test.h
t_heap_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_heap_DEPENDENCIES=libdisorder.a

t_hex_SOURCES=t-hex.c test.c test.h
t_hex_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_hex_DEPENDENCIES=libdisorder.a

t_kvp_SOURCES=t-kvp.c test.c test.h
t_kvp_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_kvp_DEPENDENCIES=libdisorder.a

t_macros_SOURCES=t-macros.c test.c test.h
t_macros_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_macros_DEPENDENCIES=libdisorder.a

t_mime_SOURCES=t-mime.c test.c test.h
t_mime_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_mime_DEPENDENCIES=libdisorder.a

t_printf_SOURCES=t-printf.c test.c test.h
t_printf_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_printf_DEPENDENCIES=libdisorder.a

t_regsub_SOURCES=t-regsub.c test.c test.h
t_regsub_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_regsub_DEPENDENCIES=libdisorder.a

t_selection_SOURCES=t-selection.c test.c test.h
t_selection_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_selection_DEPENDENCIES=libdisorder.a

t_signame_SOURCES=t-signame.c test.c test.h
t_signame_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_signame_DEPENDENCIES=libdisorder.a

t_sink_SOURCES=t-sink.c test.c test.h
t_sink_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_sink_DEPENDENCIES=libdisorder.a

t_split_SOURCES=t-split.c test.c test.h
t_split_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_split_DEPENDENCIES=libdisorder.a

t_syscalls_SOURCES=t-syscalls.c test.c test.h
t_syscalls_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_syscalls_DEPENDENCIES=libdisorder.a

t_trackname_SOURCES=t-trackname.c test.c test.h
t_trackname_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_trackname_DEPENDENCIES=libdisorder.a

t_unicode_SOURCES=t-unicode.c test.c test.h
t_unicode_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_unicode_DEPENDENCIES=libdisorder.a

t_url_SOURCES=t-url.c test.c test.h
t_url_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_url_DEPENDENCIES=libdisorder.a

t_utf8_SOURCES=t-utf8.c test.c test.h
t_utf8_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_utf8_DEPENDENCIES=libdisorder.a

t_vector_SOURCES=t-vector.c test.c test.h
t_vector_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_vector_DEPENDENCIES=libdisorder.a

t_words_SOURCES=t-words.c test.c test.h
t_words_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_words_DEPENDENCIES=libdisorder.a

t_wstat_SOURCES=t-wstat.c test.c test.h
t_wstat_LDADD=libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)
t_wstat_DEPENDENCIES=libdisorder.a

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

EXTRA_DIST=trackdb.c trackdb-stub.c t-macros-1.tmpl t-macros-2
