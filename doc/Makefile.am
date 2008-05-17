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

noinst_DATA=$(HTMLMAN)
pkgdata_DATA=$(TMPLMAN)
man_MANS=disorderd.8 disorder.1 disorder.3 disorder_config.5 disorder-dump.8 \
	disorder_protocol.5 disorder-deadlock.8 \
	disorder-rescan.8 disobedience.1 disorderfm.1 disorder-speaker.8 \
	disorder-playrtp.1 disorder-normalize.8 disorder-decode.8 \
	disorder-stats.8 disorder-dbupgrade.8 disorder_templates.5
noinst_MANS=tkdisorder.1

SEDFILES=disorder.1 disorderd.8 disorder_config.5 \
	disorder-dump.8 disorder_protocol.5 disorder-deadlock.8 \
	disorder-rescan.8 disobedience.1 disorderfm.1 disorder-playrtp.1 \
	disorder-decode.8 disorder-stats.8 disorder-dbupgrade.8

include ${top_srcdir}/scripts/sedfiles.make

HTMLMAN=$(foreach man,$(man_MANS),$(man).html)
$(HTMLMAN) : %.html : % $(top_srcdir)/scripts/htmlman
	rm -f $@.new
	$(top_srcdir)/scripts/htmlman $< >$@.new
	chmod 444 $@.new
	mv -f $@.new $@

TMPLMAN=$(foreach man,$(man_MANS),$(man).tmpl)
$(TMPLMAN) : %.tmpl : % $(top_srcdir)/scripts/htmlman
	rm -f $@.new
	$(top_srcdir)/scripts/htmlman -stdhead $< >$@.new
	chmod 444 $@.new
	mv -f $@.new $@

disorder_templates.5: disorder_templates.5.head disorder_templates.5.tail \
		$(top_srcdir)/lib/macros-builtin.c \
		$(top_srcdir)/server/macros-disorder.c \
		$(top_srcdir)/scripts/macro-docs
	rm -f disorder_templates.5.new
	cat disorder_templates.5.head >> disorder_templates.5.new
	$(top_srcdir)/scripts/macro-docs >> disorder_templates.5.new \
		$(top_srcdir)/lib/macros-builtin.c \
		$(top_srcdir)/server/macros-disorder.c 
	cat disorder_templates.5.tail >> disorder_templates.5.new
	mv disorder_templates.5.new disorder_templates.5

EXTRA_DIST=disorderd.8.in disorder.1.in disorder_config.5.in \
	   disorder.3 disorder-dump.8.in disorder_protocol.5.in \
	   tkdisorder.1 disorder-deadlock.8.in disorder-rescan.8.in \
	   disobedience.1.in disorderfm.1.in disorder-speaker.8 \
	   disorder-playrtp.1.in disorder-decode.8.in disorder-normalize.8 \
	   disorder-stats.8.in disorder-dbupgrade.8.in

CLEANFILES=$(SEDFILES) $(HTMLMAN) $(TMPLMAN)

export GNUSED
