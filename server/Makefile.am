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

sbin_PROGRAMS=disorderd disorder-deadlock disorder-rescan disorder-dump \
	      disorder-speaker disorder-decode disorder-normalize
noinst_PROGRAMS=disorder.cgi trackname
noinst_DATA=uk.org.greenend.rjk.disorder.plist

AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib

disorderd_SOURCES=disorderd.c				\
	api.c api-server.c				\
	daemonize.c daemonize.h				\
	play.c play.h 					\
	server.c server.h     				\
	state.c state.h					\
	trackdb.c trackdb.h trackdb-int.h exports.c 	\
	../lib/memgc.c
disorderd_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBPCRE) $(LIBDB) $(LIBAO) $(LIBGC) $(LIBGCRYPT) $(LIBICONV)
disorderd_LDFLAGS=-export-dynamic
disorderd_DEPENDENCIES=../lib/libdisorder.a

disorder_deadlock_SOURCES=deadlock.c                    \
	trackdb.c trackdb.h
disorder_deadlock_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBPCRE) $(LIBICONV)
disorder_deadlock_DEPENDENCIES=../lib/libdisorder.a

disorder_speaker_SOURCES=speaker.c speaker.h \
		         speaker-command.c \
			 speaker-network.c \
			 speaker-alsa.c
disorder_speaker_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBASOUND) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT)
disorder_speaker_DEPENDENCIES=../lib/libdisorder.a

disorder_decode_SOURCES=decode.c
disorder_decode_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBMAD) $(LIBVORBISFILE) $(LIBFLAC)
disorder_decode_DEPENDENCIES=../lib/libdisorder.a

disorder_normalize_SOURCES=normalize.c
disorder_normalize_LDADD=$(LIBOBJS) ../lib/libdisorder.a $(LIBPCRE) $(LIBICONV)
disorder_normalize_DEPENDENCIES=../lib/libdisorder.a

disorder_rescan_SOURCES=rescan.c                        \
	api.c api-server.c				\
	trackdb.c trackdb.h exports.c			\
	../lib/memgc.c
disorder_rescan_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBDB) $(LIBGC) $(LIBPCRE) $(LIBICONV)
disorder_rescan_LDFLAGS=-export-dynamic
disorder_rescan_DEPENDENCIES=../lib/libdisorder.a

disorder_dump_SOURCES=dump.c                           	\
        trackdb.c trackdb.h				\
	../lib/memgc.c
disorder_dump_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBPCRE) $(LIBDB) $(LIBICONV) $(LIBGC)
disorder_dump_DEPENDENCIES=$(LIBOBJS) ../lib/libdisorder.a

disorder_cgi_SOURCES=dcgi.c dcgi.h			\
	api.c api-client.c api-client.h			\
	cgi.c cgi.h cgimain.c exports.c
disorder_cgi_LDADD=../lib/libdisorder.a \
	$(LIBPCRE) $(LIBGCRYPT) $(LIBDL)
disorder_cgi_LDFLAGS=-export-dynamic
disorder_cgi_DEPENDENCIES=../lib/libdisorder.a

trackname_SOURCES=trackname.c
trackname_LDADD=../lib/libdisorder.a $(LIBPCRE) $(LIBICONV)
trackname_DEPENDENCIES=../lib/libdisorder.a

install-exec-hook:
	$(LIBTOOL) --mode=finish $(DESTDIR)$(libdir)

check: check-help

# check everything has working --help
check-help: all
	./disorderd --help > /dev/null
	./disorder-dump --help > /dev/null
	./disorder-deadlock --help > /dev/null
	./trackname --help > /dev/null
	./disorder-speaker --help > /dev/null

cgi.o: ../lib/definitions.h

# for Mac OS X >=10.4
SEDFILES=uk.org.greenend.rjk.disorder.plist
include ${top_srcdir}/scripts/sedfiles.make
EXTRA_DIST=uk.org.greenend.rjk.disorder.plist.in
LAUNCHD=/Library/LaunchDaemons

#install-data-hook:
#	@if [ -d ${LAUNCHD} ]; then \
#	  echo $(INSTALL) -m 644 uk.org.greenend.rjk.disorder.plist ${LAUNCHD};\
#	  $(INSTALL) -m 644 uk.org.greenend.rjk.disorder.plist ${LAUNCHD};\
#	  if [ "$$(NOLAUNCH)" != "" ]; then \
#	    echo launchctl unload ${LAUNCHD} \|\| true;\
#	    launchctl unload ${LAUNCHD} || true;\
#	    echo launchctl load ${LAUNCHD} \|\| true;\
#	    launchctl load ${LAUNCHD} || true;\
#         fi;\
#	fi
