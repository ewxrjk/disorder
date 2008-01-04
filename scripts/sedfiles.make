#
# This file is part of DisOrder.
# Copyright (C) 2004, 2005, 2007 Richard Kettlewell
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

$(SEDFILES) : % : %.in Makefile
	rm -f $@.new
	$(GNUSED) -e 's!sbindir!${sbindir}!g;' \
	    -e 's!bindir!${bindir}!g;' \
	    -e 's!pkgconfdir!${sysconfdir}/disorder!g;' \
	    -e 's!pkgstatedir!${localstatedir}/disorder!g;' \
	    -e 's!pkgdatadir!${pkgdatadir}!g;' \
	    -e 's!_version_!${VERSION}!g;' \
	        < $< > $@.new
	chmod 444 $@.new
	mv -f $@.new $@

# Local Variables:
# mode:makefile
# End:
