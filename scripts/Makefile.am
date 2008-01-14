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

dist_pkgdata_DATA=completion.bash
noinst_SCRIPTS=setup

SEDFILES=setup

include ${top_srcdir}/scripts/sedfiles.make

EXTRA_DIST=htmlman sedfiles.make text2c oggrename make-unidata \
	format-gcov-report make-version-string setup.in

CLEANFILES=$(SEDFILES)
