#
# This file is part of DisOrder.
# Copyright (C) 2004-2009 Richard Kettlewell
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

disorderd_SOURCES=disorderd.c api.c api-server.c daemonize.c play.c	\
	server.c server-queue.c queue-ops.c state.c plugin.c		\
	schedule.c dbparams.c background.c \
	exports.c ../lib/memgc.c disorder-server.h
disorderd_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBPCRE) $(LIBDB) $(LIBAO) $(LIBGC) $(LIBGCRYPT) $(LIBICONV) \
	$(LIBASOUND) $(COREAUDIO)
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

disorder_decode_SOURCES=decode.c disorder-server.h
disorder_decode_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBMAD) $(LIBVORBISFILE) $(LIBFLAC)
disorder_decode_DEPENDENCIES=../lib/libdisorder.a

disorder_normalize_SOURCES=normalize.c disorder-server.h
disorder_normalize_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_normalize_DEPENDENCIES=../lib/libdisorder.a

disorder_rescan_SOURCES=rescan.c plugin.c api.c api-server.c exports.c	\
	../lib/memgc.c disorder-server.h
disorder_rescan_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBGC) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
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

install-exec-hook:
	$(LIBTOOL) --mode=finish $(DESTDIR)$(libdir)

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

check-decode: disorder-decode disorder-normalize endian
	echo "speaker_backend network" > config
	echo "broadcast 127.255.255.255 9" > config
	oggdec -b 16 -e `./endian` -R -s 1 -o oggdec.raw ${top_srcdir}/sounds/scratch.ogg
	@$(MAKE) check-ogg
	@$(MAKE) check-wav
	@if which flac >/dev/null 2>&1; then $(MAKE) check-flac; fi
	rm -f config oggdec.raw

# TODO mp3 - but sox doesn't (reliably) know how to encode to it

check-ogg:
	./disorder-decode ${top_srcdir}/sounds/scratch.ogg | \
	  ./disorder-normalize --config config > ogged.raw
	cmp ogged.raw oggdec.raw
	rm -f ogged.raw

check-wav:
	sox ${top_srcdir}/sounds/scratch.ogg scratch.wav
	./disorder-decode scratch.wav | \
	  ./disorder-normalize --config config > waved.raw
	cmp waved.raw oggdec.raw
	rm -rf scratch.wav waved.raw

check-flac:
	rm -f scratch.wav scratch.flac
	sox ${top_srcdir}/sounds/scratch.ogg scratch.wav
	flac scratch.wav
	./disorder-decode scratch.flac | \
	  ./disorder-normalize --config config > flacced.raw
	cmp flacced.raw oggdec.raw
	rm -f scratch.flac scratch.wav flacced.raw

EXTRA_DIST=README.dbversions

CLEANFILES=*.gcda *.gcov *.gcno *.c.html index.html
