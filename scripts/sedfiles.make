#
# This file is part of DisOrder.
# Copyright (C) 2004-2008 Richard Kettlewell
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

$(SEDFILES) : % : %.in Makefile
	rm -f $@.new
	$(GNUSED) -e 's!sbindir!${sbindir}!g;' \
	    -e 's!bindir!${bindir}!g;' \
	    -e 's!jardir!${jardir}!g;' \
	    -e 's!pkgconfdir!${sysconfdir}/disorder!g;' \
	    -e 's!pkgstatedir!${localstatedir}/disorder!g;' \
	    -e 's!pkgdatadir!${pkgdatadir}!g;' \
	    -e 's!dochtmldir!${dochtmldir}!g;' \
	    -e 's!SENDMAIL!${SENDMAIL}!g;' \
	    -e 's!_version_!${VERSION}!g;' \
	        < $< > $@.new
	@if test -x $<; then \
	  echo chmod 555 $@.new;\
	  chmod 555 $@.new;\
	else \
	  echo chmod 444 $@.new;\
	  chmod 444 $@.new;\
	fi
	mv -f $@.new $@

# Local Variables:
# mode:makefile
# End:
