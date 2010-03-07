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

noinst_DATA=$(HTMLMAN)
pkgdata_DATA=$(TMPLMAN)
man_MANS=disorderd.8 disorder.1 disorder.3 disorder_config.5 disorder-dump.8 \
	disorder_protocol.5 disorder-deadlock.8 \
	disorder-rescan.8 disobedience.1 disorderfm.1 disorder-speaker.8 \
	disorder-playrtp.1 disorder-normalize.8 disorder-decode.8 \
	disorder-stats.8 disorder-dbupgrade.8 disorder_templates.5 \
	disorder_actions.5 disorder_options.5 disorder.cgi.8 \
	disorder_preferences.5 disorder-choose.8

SEDFILES=disorder.1 disorderd.8 disorder_config.5 \
	disorder-dump.8 disorder_protocol.5 disorder-deadlock.8 \
	disorder-rescan.8 disobedience.1 disorderfm.1 disorder-playrtp.1 \
	disorder-decode.8 disorder-stats.8 disorder-dbupgrade.8 \
	disorder_options.5 disorder.cgi.8 disorder_templates.5 \
	disorder_actions.5 disorder_preferences.5 disorder-choose.8

include ${top_srcdir}/scripts/sedfiles.make

HTMLMAN=disorderd.8.html disorder.1.html disorder.3.html		\
disorder_config.5.html disorder-dump.8.html disorder_protocol.5.html	\
disorder-deadlock.8.html disorder-rescan.8.html disobedience.1.html	\
disorderfm.1.html disorder-speaker.8.html disorder-playrtp.1.html	\
disorder-normalize.8.html disorder-decode.8.html disorder-stats.8.html	\
disorder-dbupgrade.8.html disorder_templates.5.html			\
disorder_actions.5.html disorder_options.5.html disorder.cgi.8.html	\
disorder_preferences.5.html disorder-choose.8.html

$(wordlist 2,9999,$(HTMLMAN)): disorderd.8.html
disorderd.8.html: $(man_MANS)
	$(top_srcdir)/scripts/htmlman -- $^

TMPLMAN=disorderd.8.tmpl disorder.1.tmpl disorder.3.tmpl		\
disorder_config.5.tmpl disorder-dump.8.tmpl disorder_protocol.5.tmpl	\
disorder-deadlock.8.tmpl disorder-rescan.8.tmpl disobedience.1.tmpl	\
disorderfm.1.tmpl disorder-speaker.8.tmpl disorder-playrtp.1.tmpl	\
disorder-normalize.8.tmpl disorder-decode.8.tmpl disorder-stats.8.tmpl	\
disorder-dbupgrade.8.tmpl disorder_templates.5.tmpl			\
disorder_actions.5.tmpl disorder_options.5.tmpl disorder.cgi.8.tmpl	\
disorder_preferences.5.tmpl disorder-choose.8.tmpl

$(wordlist 2,9999,$(TMPLMAN)): disorderd.8.tmpl
disorderd.8.tmpl: $(man_MANS)
	$(top_srcdir)/scripts/htmlman -stdhead -extension tmpl -- $^

disorder_templates.5.in: disorder_templates.5.head disorder_templates.5.tail \
		$(top_srcdir)/lib/macros-builtin.c \
		$(top_srcdir)/cgi/macros-disorder.c \
		$(top_srcdir)/scripts/macro-docs
	rm -f disorder_templates.5.new
	cat ${srcdir}/disorder_templates.5.head >> disorder_templates.5.new
	$(top_srcdir)/scripts/macro-docs >> disorder_templates.5.new \
		$(top_srcdir)/lib/macros-builtin.c \
		$(top_srcdir)/cgi/macros-disorder.c 
	cat ${srcdir}/disorder_templates.5.tail >> disorder_templates.5.new
	mv disorder_templates.5.new disorder_templates.5.in

disorder_actions.5.in: disorder_actions.5.head disorder_actions.5.tail \
		$(top_srcdir)/lib/macros-builtin.c \
		$(top_srcdir)/cgi/actions.c \
		$(top_srcdir)/scripts/macro-docs
	rm -f disorder_actions.5.new
	cat ${srcdir}/disorder_actions.5.head >> disorder_actions.5.new
	$(top_srcdir)/scripts/macro-docs >> disorder_actions.5.new \
		$(top_srcdir)/cgi/actions.c 
	cat ${srcdir}/disorder_actions.5.tail >> disorder_actions.5.new
	mv disorder_actions.5.new disorder_actions.5.in

if GROG
check: $(man_MANS)
	@set -e; for m in $(man_MANS); do \
	  echo \`$(GROG) $$m\` -Tutf8 -wall \>/dev/null ;\
	  echo `$(GROG) $$m` -Tutf8 -wall >/dev/null ;\
	done
endif

EXTRA_DIST=disorderd.8.in disorder.1.in disorder_config.5.in \
	   disorder.3 disorder-dump.8.in disorder_protocol.5.in \
	   disorder-deadlock.8.in disorder-rescan.8.in \
	   disobedience.1.in disorderfm.1.in disorder-speaker.8 \
	   disorder-playrtp.1.in disorder-decode.8.in disorder-normalize.8 \
	   disorder-stats.8.in disorder-dbupgrade.8.in \
	   disorder_actions.5.head disorder_templates.5.head \
	   disorder_actions.5.tail disorder_templates.5.tail \
	   disorder_options.5.in disorder.cgi.8.in disorder_preferences.5.in \
	   disorder-choose.8.in

CLEANFILES=$(SEDFILES) $(HTMLMAN) $(TMPLMAN) \
	disorder_actions.5.in disorder_templates.5.in

export GNUSED
