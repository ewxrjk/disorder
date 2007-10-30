#
# This file is part of DisOrder
# Copyright (C) 2004, 2005 Richard Kettlewell
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

noinst_SCRIPTS=rules

autorules_m4=${srcdir}/autorules.m4
rules_m4=${srcdir}/rules.m4

rules: ${autorules_m4} ${rules_m4}
	rm -f rules.tmp
	m4 -P ${autorules_m4} ${rules_m4} > rules.tmp
	chmod 555 rules.tmp
	mv -f rules.tmp rules

EXTRA_DIST=README.Debian autorules.m4 config.disorder control copyright \
	   etc.disorder.options etc.disorder.config htaccess \
	   postinst.disorder prerm.disorder postrm.disorder \
	   templates.disorder conffiles.disorder \
	   rules.m4 rules changelog usr.share.menu.disobedience
