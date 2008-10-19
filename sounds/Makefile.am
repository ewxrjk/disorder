#
# This file is part of DisOrder.
# Copyright (C) 2004, 2007, 2008 Richard Kettlewell
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

pkgdata_DATA=slap.ogg scratch.ogg
noinst_DATA=long.ogg

EXTRA_DIST=${pkgdata_DATA}

%.raw: %.ogg
	oggdec -b16 -R -o $@ $<

long.ogg: slap.raw
	cat slap.raw slap.raw slap.raw slap.raw slap.raw slap.raw \
		slap.raw slap.raw | oggenc -r -o $@ -

clean-local:
	rm -f long.ogg slap.raw
