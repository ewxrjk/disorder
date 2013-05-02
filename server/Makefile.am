#
# This file is part of DisOrder.
# Copyright (C) 2004-2011 Richard Kettlewell
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

sbin_PROGRAMS=disorderd disorder-deadlock disorder-rescan disorder-dump \
	      disorder-speaker disorder-decode disorder-normalize \
	      disorder-stats disorder-dbupgrade disorder-choose
noinst_PROGRAMS=trackname endian

AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib
AM_CFLAGS=

disorderd_SOURCES=disorderd.c api.c api-server.c daemonize.c play.c	\
	server.c server-queue.c queue-ops.c state.c plugin.c		\
	schedule.c dbparams.c background.c mount.c \
	exports.c ../lib/memgc.c disorder-server.h
disorderd_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBPCRE) $(LIBDB) $(LIBAO) $(LIBGC) $(LIBGCRYPT) $(LIBICONV) \
	$(LIBASOUND) $(COREAUDIO) $(LIBPTHREAD) $(LIBDL)
disorderd_LDFLAGS=-export-dynamic
disorderd_DEPENDENCIES=../lib/libdisorder.a

disorder_deadlock_SOURCES=deadlock.c disorder-server.h
disorder_deadlock_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_deadlock_DEPENDENCIES=../lib/libdisorder.a

disorder_speaker_SOURCES=speaker.c
disorder_speaker_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBASOUND) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT) $(COREAUDIO) \
	$(LIBPTHREAD)
disorder_speaker_DEPENDENCIES=../lib/libdisorder.a

disorder_decode_SOURCES=decode.c decode.h disorder-server.h	\
decode-mp3.c decode-ogg.c decode-wav.c decode-flac.c
disorder_decode_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBMAD) $(LIBVORBISFILE) $(LIBFLAC)
disorder_decode_DEPENDENCIES=../lib/libdisorder.a

if GSTDECODE
AM_CFLAGS+=$(GSTREAMER_CFLAGS)
sbin_PROGRAMS+=disorder-gstdecode
disorder_gstdecode_SOURCES=gstdecode.c disorder-server.h
disorder_gstdecode_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(GSTREAMER_PLUGINS_BASE_LIBS) -lgstaudio-0.10 -lgstapp-0.10 \
	$(GSTREAMER_LIBS)
disorder_gstdecode_DEPENDENCIES=../lib/libdisorder.a
endif

disorder_normalize_SOURCES=normalize.c disorder-server.h
disorder_normalize_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBPCRE) $(LIBICONV) $(LIBGCRYPT) $(LIBSAMPLERATE)
disorder_normalize_DEPENDENCIES=../lib/libdisorder.a

disorder_rescan_SOURCES=rescan.c plugin.c api.c api-server.c exports.c	\
	../lib/memgc.c disorder-server.h
disorder_rescan_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBGC) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT) $(LIBDL)
disorder_rescan_LDFLAGS=-export-dynamic
disorder_rescan_DEPENDENCIES=../lib/libdisorder.a

disorder_choose_SOURCES=choose.c server-queue.c ../lib/memgc.c	\
			disorder-server.h
disorder_choose_LDADD=$(LIBOBJS) ../lib/libdisorder.a   \
	$(LIBDB) $(LIBGC) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_choose_LDFLAGS=-export-dynamic
disorder_choose_DEPENDENCIES=../lib/libdisorder.a

disorder_stats_SOURCES=stats.c disorder-server.h
disorder_stats_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_stats_DEPENDENCIES=../lib/libdisorder.a

disorder_dump_SOURCES=dump.c disorder-server.h ../lib/memgc.c
disorder_dump_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBPCRE) $(LIBDB) $(LIBICONV) $(LIBGC) $(LIBGCRYPT)
disorder_dump_DEPENDENCIES=$(LIBOBJS) ../lib/libdisorder.a

disorder_dbupgrade_SOURCES=dbupgrade.c ../lib/memgc.c disorder-server.h
disorder_dbupgrade_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBGC) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_dbupgrade_DEPENDENCIES=../lib/libdisorder.a

trackname_SOURCES=trackname.c disorder-server.h
trackname_LDADD=../lib/libdisorder.a $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
trackname_DEPENDENCIES=../lib/libdisorder.a

check: check-help check-decode

# check everything has working --help and --version
check-help: all
	./disorderd --help > /dev/null
	./disorderd --version > /dev/null
	./disorder-dump --help > /dev/null
	./disorder-dump --version > /dev/null
	./disorder-deadlock --help > /dev/null
	./disorder-deadlock --version > /dev/null
	./trackname --help > /dev/null
	./trackname --version > /dev/null
	./disorder-speaker --help > /dev/null
	./disorder-speaker --version > /dev/null
	./disorder-decode --help > /dev/null
	./disorder-decode --version > /dev/null
	./disorder-normalize --help > /dev/null
	./disorder-normalize --version > /dev/null
	./disorder-stats --help > /dev/null
	./disorder-stats --version > /dev/null
	./disorder-dbupgrade --help > /dev/null
	./disorder-dbupgrade --version > /dev/null
	./disorder-rescan --help > /dev/null
	./disorder-rescan --version > /dev/null
	./disorder-choose --help > /dev/null
	./disorder-choose --version > /dev/null

check-decode: check-wav check-flac check-mp3

check-mp3: disorder-decode disorder-normalize
	./disorder-decode ${top_srcdir}/sounds/scratch.mp3 | \
	  ./disorder-normalize --config ${srcdir}/test-config > mp3ed.raw
	cmp mp3ed.raw ${top_srcdir}/sounds/scratch-mp3.raw
	rm -f mp3ed.raw

# TODO ogg decoding comes out OK but different depending on platform, version
# or something.  Makes it tricky to test!
check-ogg: disorder-decode disorder-normalize
	./disorder-decode ${top_srcdir}/sounds/scratch.ogg | \
	  ./disorder-normalize --config ${srcdir}/test-config > ogged.raw
	cmp ogged.raw ${top_srcdir}/sounds/scratch.raw
	rm -f ogged.raw

check-wav: disorder-decode disorder-normalize
	./disorder-decode ${top_srcdir}/sounds/scratch.wav | \
	  ./disorder-normalize --config ${srcdir}/test-config > waved.raw
	cmp waved.raw ${top_srcdir}/sounds/scratch.raw
	rm -rf waved.raw

check-flac: disorder-decode disorder-normalize
	./disorder-decode ${top_srcdir}/sounds/scratch.flac | \
	  ./disorder-normalize --config ${srcdir}/test-config > flacced.raw
	cmp flacced.raw ${top_srcdir}/sounds/scratch.raw
	rm -f flacced.raw

EXTRA_DIST=README.dbversions test-config

CLEANFILES=*.gcda *.gcov *.gcno *.c.html index.html
