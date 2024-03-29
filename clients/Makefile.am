#
# This file is part of DisOrder.
# Copyright (C) 2006-2009, 2011-2013 Richard Kettlewell
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

VPATH+=${top_srcdir}/common

bin_PROGRAMS=disorder disorderfm disorder-playrtp
noinst_PROGRAMS=filename-bytes rtpmon resample
noinst_SCRIPTS=dump2wav

AUTOMAKE_OPTIONS=subdir-objects
AM_CPPFLAGS=-I${top_srcdir}/lib -I../lib

disorder_SOURCES=disorder.c authorize.c authorize.h
nodist_disorder_SOURCES=memgc.c
disorder_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBGC) $(LIBGCRYPT) $(LIBPCRE) $(LIBICONV) $(LIBPTHREAD)
disorder_DEPENDENCIES=$(LIBOBJS) ../lib/libdisorder.a

disorderfm_SOURCES=disorderfm.c
nodist_disorderfm_SOURCES=memgc.c
disorderfm_LDADD=$(LIBOBJS) ../lib/libdisorder.a $(LIBGC) $(LIBICONV)
disorderfm_DEPENDENCIES=$(LIBOBJS) ../lib/libdisorder.a

disorder_playrtp_SOURCES=playrtp.c playrtp.h playrtp-mem.c
disorder_playrtp_CFLAGS=$(PULSEAUDIO_CFLAGS) $(PULSEAUDIO_SIMPLE_CFLAGS)
disorder_playrtp_LDADD=$(LIBOBJS) ../lib/libdisorder.a \
	$(LIBASOUND) $(LIBPCRE) $(LIBICONV) $(LIBGCRYPT) $(COREAUDIO) \
	$(LIBPTHREAD) $(PULSEAUDIO_SIMPLE_LIBS) $(PULSEAUDIO_LIBS) -lm
disorder_playrtp_DEPENDENCIES=$(LIBOBJS) ../lib/libdisorder.a

rtpmon_SOURCES=rtpmon.c
rtpmon_LDADD=$(LIBOBJS) ../lib/libdisorder.a

filename_bytes_SOURCES=filename-bytes.c

resample_SOURCES=resample.c
resample_LDADD=$(LIBOBJS) ../lib/libdisorder.a $(LIBSAMPLERATE)

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

CLEANFILES=,commands ,completions \
	   *.gcda *.gcov *.gcno *.c.html index.html

EXTRA_DIST=dump2wav disorder.vcxproj disorder.vcxproj.filters
