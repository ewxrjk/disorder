#
# This file is part of DisOrder.
# Copyright (C) 2009-2011 Richard Kettlewell
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

TESTS=t-addr t-arcfour t-basen t-bits t-cache t-casefold t-charset	\
	t-cookies t-dateparse t-event t-filepart t-hash t-heap t-hex	\
	t-kvp t-mime t-printf t-regsub t-selection t-signame t-sink	\
	t-split t-syscalls t-trackname t-unicode t-url t-utf8 t-vector	\
	t-words t-wstat t-macros t-cgi t-eventdist t-resample 		\
	t-configuration t-timeval

noinst_PROGRAMS=$(TESTS)

AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib
LDADD=../lib/libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGC)

t_addr_SOURCES=t-addr.c test.c test.h
t_arcfour_SOURCES=t-arcfour.c test.c test.h
t_basen_SOURCES=t-basen.c test.c test.h
t_bits_SOURCES=t-bits.c test.c test.h
t_cache_SOURCES=t-cache.c test.c test.h
t_casefold_SOURCES=t-casefold.c test.c test.h
t_charset_SOURCES=t-charset.c test.c test.h
t_cgi_SOURCES=t-cgi.c test.c test.h
t_cookies_SOURCES=t-cookies.c test.c test.h
t_dateparse_SOURCES=t-dateparse.c test.c test.h
t_event_SOURCES=t-event.c test.c test.h
t_filepart_SOURCES=t-filepart.c test.c test.h
t_hash_SOURCES=t-hash.c test.c test.h
t_heap_SOURCES=t-heap.c test.c test.h
t_hex_SOURCES=t-hex.c test.c test.h
t_kvp_SOURCES=t-kvp.c test.c test.h
t_macros_SOURCES=t-macros.c test.c test.h
t_mime_SOURCES=t-mime.c test.c test.h
t_printf_SOURCES=t-printf.c test.c test.h
t_printf_CFLAGS=$(AM_CFLAGS) -Wno-format-security
t_regsub_SOURCES=t-regsub.c test.c test.h
t_selection_SOURCES=t-selection.c test.c test.h
t_signame_SOURCES=t-signame.c test.c test.h
t_sink_SOURCES=t-sink.c test.c test.h
t_split_SOURCES=t-split.c test.c test.h
t_syscalls_SOURCES=t-syscalls.c test.c test.h
t_trackname_SOURCES=t-trackname.c test.c test.h
t_unicode_SOURCES=t-unicode.c test.c test.h
t_url_SOURCES=t-url.c test.c test.h
t_utf8_SOURCES=t-utf8.c test.c test.h
t_vector_SOURCES=t-vector.c test.c test.h
t_words_SOURCES=t-words.c test.c test.h
t_wstat_SOURCES=t-wstat.c test.c test.h
t_eventdist_SOURCES=t-eventdist.c test.c test.h
t_resample_SOURCES=t-resample.c test.c test.h
t_resample_LDFLAGS=$(LIBSAMPLERATE)
t_configuration_SOURCES=t-configuration.c test.c test.h
t_configuration_LDFLAGS=$(LIBGCRYPT)
t_timeval_SOURCES=t-timeval.c test.c test.h

check-report: before-check check make-coverage-reports
before-check:
	rm -f *.gcda *.gcov
make-coverage-reports: check
	cd ../lib && ${GCOV} *.c | ${PYTHON} ../scripts/format-gcov-report --html . *.c

EXTRA_DIST=t-macros-1.tmpl t-macros-2

CLEANFILES=*.gcda *.gcov *.gcno *.c.html index.html

DISTCLEANFILES=GraphemeBreakTest.txt NormalizationTest.txt	\
	       WordBreakTest.txt
