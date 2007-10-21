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

SEDFILES=disorder.1 disorderd.8 disorder_config.5 \
	disorder-dump.8 disorder_protocol.5 disorder-deadlock.8 \
	disorder-rescan.8 disobedience.1 disorderfm.1 disorder-playrtp.1 \
	disorder-decode.8

include ${top_srcdir}/scripts/sedfiles.make

man_MANS=disorderd.8 disorder.1 disorder.3 disorder_config.5 disorder-dump.8 \
	disorder_protocol.5 disorder-deadlock.8 \
	disorder-rescan.8 disobedience.1 disorderfm.1 disorder-speaker.8 \
	disorder-playrtp.1 disorder-normalize.8 disorder-decode.8

noinst_MANS=tkdisorder.1

HTMLMAN=$(foreach man,$(man_MANS),$(man).html)

$(HTMLMAN) : %.html : % $(top_srcdir)/scripts/htmlman
	rm -f $@.new
	$(top_srcdir)/scripts/htmlman -stdhead $< >$@.new
	chmod 444 $@.new
	mv -f $@.new $@

pkgdata_DATA=$(HTMLMAN)

EXTRA_DIST=disorderd.8.in disorder.1.in disorder_config.5.in \
	   disorder.3 disorder-dump.8.in disorder_protocol.5.in \
	   tkdisorder.1 disorder-deadlock.8.in disorder-rescan.8.in \
	   disobedience.1.in disorderfm.1.in disorder-speaker.8 \
	   disorder-playrtp.1.in disorder-decode.8.in disorder-normalize.8

CLEANFILES=$(SEDFILES) $(HTMLMAN)
