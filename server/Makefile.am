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

sbin_PROGRAMS=disorderd disorder-deadlock disorder-rescan disorder-dump \
	      disorder-speaker disorder-decode disorder-normalize \
	      disorder-stats disorder-dbupgrade disorder-choose
noinst_PROGRAMS=disorder.cgi trackname

AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib

disorderd_SOURCES=disorderd.c				\
	api.c api-server.c				\
	daemonize.c daemonize.h				\
	play.c play.h 					\
	server.c server.h     				\
	server-queue.c server-queue.h			\
	state.c state.h					\
	exports.c 					\
	../lib/memgc.c
disorderd_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBPCRE) $(LIBDB) $(LIBAO) $(LIBGC) $(LIBGCRYPT) $(LIBICONV) \
	$(LIBASOUND) $(COREAUDIO)
disorderd_LDFLAGS=-export-dynamic
disorderd_DEPENDENCIES=../lib/libdisorder.a

disorder_deadlock_SOURCES=deadlock.c
disorder_deadlock_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_deadlock_DEPENDENCIES=../lib/libdisorder.a

disorder_speaker_SOURCES=speaker.c speaker.h \
		         speaker-command.c \
			 speaker-network.c \
			 speaker-coreaudio.c \
			 speaker-oss.c \
			 speaker-alsa.c
disorder_speaker_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBASOUND) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT) $(COREAUDIO)
disorder_speaker_DEPENDENCIES=../lib/libdisorder.a

disorder_decode_SOURCES=decode.c
disorder_decode_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBMAD) $(LIBVORBISFILE) $(LIBFLAC)
disorder_decode_DEPENDENCIES=../lib/libdisorder.a

disorder_normalize_SOURCES=normalize.c
disorder_normalize_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_normalize_DEPENDENCIES=../lib/libdisorder.a

disorder_rescan_SOURCES=rescan.c                        \
	api.c api-server.c				\
	exports.c			                \
	../lib/memgc.c
disorder_rescan_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBGC) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_rescan_LDFLAGS=-export-dynamic
disorder_rescan_DEPENDENCIES=../lib/libdisorder.a

disorder_choose_SOURCES=choose.c                        \
	server-queue.c server-queue.h			\
        api.c api-server.c                              \
        exports.c					\
	../lib/memgc.c
disorder_choose_LDADD=$(LIBOBJS) ../lib/libdisorder.a   \
	$(LIBDB) $(LIBGC) $(LIBPCRE) $(LIBGCRYPT)
disorder_choose_LDFLAGS=-export-dynamic
disorder_choose_DEPENDENCIES=../lib/libdisorder.a

disorder_stats_SOURCES=stats.c
disorder_stats_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_stats_DEPENDENCIES=../lib/libdisorder.a

disorder_dump_SOURCES=dump.c                           	\
	../lib/memgc.c
disorder_dump_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBPCRE) $(LIBDB) $(LIBICONV) $(LIBGC) $(LIBGCRYPT)
disorder_dump_DEPENDENCIES=$(LIBOBJS) ../lib/libdisorder.a

disorder_dbupgrade_SOURCES=dbupgrade.c ../lib/memgc.c
disorder_dbupgrade_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBGC) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_dbupgrade_DEPENDENCIES=../lib/libdisorder.a

disorder_cgi_SOURCES=macros-disorder.c macros-disorder.h lookup.c	\
	lookup.h options.c options.h actions.c actions.h api.c		\
	api-client.c api-client.h cgimain.c exports.c
disorder_cgi_LDADD=../lib/libdisorder.a \
	$(LIBPCRE) $(LIBGCRYPT) $(LIBDL) $(LIBDB)
disorder_cgi_LDFLAGS=-export-dynamic
disorder_cgi_DEPENDENCIES=../lib/libdisorder.a

trackname_SOURCES=trackname.c
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

# My sox doesn't know MP3 or FLAC unfortunately
check-decode: disorder-decode disorder-normalize
	echo "speaker_backend network" > config
	echo "broadcast 127.255.255.255 discard" > config
	./disorder-decode ${top_srcdir}/sounds/scratch.ogg | \
	  ./disorder-normalize --config config > decoded.raw
	oggdec -b 16 -e 1 -R -s 1 -o oggdec.raw ${top_srcdir}/sounds/scratch.ogg
	cmp decoded.raw oggdec.raw
	sox ${top_srcdir}/sounds/scratch.ogg scratch.wav
	./disorder-decode scratch.wav | \
	  ./disorder-normalize --config config > decoded.raw
	ls -l *.raw
	cmp decoded.raw oggdec.raw
	rm -f scratch.wav config decoded.raw oggdec.raw

cgi.o: ../lib/definitions.h

EXTRA_DIST=README.dbversions
