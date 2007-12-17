#
# This file is part of DisOrder.
# Copyright (C) 2006, 2007 Richard Kettlewell
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

bin_PROGRAMS=disorder disorderfm disorder-playrtp
noinst_PROGRAMS=test-eclient filename-bytes
noinst_SCRIPTS=dump2wav

AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib

disorder_SOURCES=disorder.c authorize.c authorize.h \
	../lib/memgc.c
disorder_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBGC) $(LIBGCRYPT) $(LIBPCRE)
disorder_DEPENDENCIES=$(LIBOBJS) ../lib/libdisorder.a

disorderfm_SOURCES=disorderfm.c \
	../lib/memgc.c
disorderfm_LDADD=$(LIBOBJS) ../lib/libdisorder.a $(LIBGC) $(LIBICONV)
disorderfm_DEPENDENCIES=$(LIBOBJS) ../lib/libdisorder.a

disorder_playrtp_SOURCES=playrtp.c playrtp.h playrtp-mem.c \
			 playrtp-alsa.c playrtp-coreaudio.c playrtp-oss.c
disorder_playrtp_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBASOUND) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT) $(COREAUDIO)
disorder_playrtp_DEPENDENCIES=$(LIBOBJS) ../lib/libdisorder.a

filename_bytes_SOURCES=filename-bytes.c

test_eclient_SOURCES=test-eclient.c \
	../lib/memgc.c
test_eclient_LDADD=../lib/libdisorder.a \
	$(LIBGC) $(LIBGCRYPT) $(LIBPCRE)
test_eclient_DEPENDENCIES=../lib/libdisorder.a

install-exec-hook:
	$(LIBTOOL) --mode=finish $(DESTDIR)$(libdir)

check: check-help check-completions

# check everything has working --help and --version
check-help: all
	./disorder --version > /dev/null
	./disorder --help > /dev/null
	./disorder --help-commands > /dev/null
	./disorderfm --version > /dev/null
	./disorderfm --help > /dev/null
	./disorder-playrtp --version > /dev/null
	./disorder-playrtp --help > /dev/null

# check that the command completions are up to date
check-completions: disorder
	./disorder --help-commands \
		| awk '/^  [a-z]/ { print $$1 }' \
		| sort > ,commands
	( set -e;completions() { \
	    for x; do \
	      case $$x in\
		quack ) ;;\
		[a-z]* ) echo $$x; ;;\
	      esac;\
	    done;\
	   }; \
	   complete() { if [ "$$7" = disorder ]; then completions $$6; fi }; \
	   . ${top_srcdir}/scripts/completion.bash )\
		| sort > ,completions
	diff -u ,commands ,completions

CLEANFILES=,commands ,completions

EXTRA_DIST=dump2wav
