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

CONFIGURE=--prefix=/usr --sysconfdir=/etc --localstatedir=/var/lib --mandir=/usr/share/man
LIBTOOL=./libtool

build

archpkg([disorder], [	m4_dnl
	$(MAKE) DESTDIR=`pwd`/debian/disorder staticdir=/var/www/disorder installdirs install
	mkdir -m 755 -p debian/disorder/etc/disorder
	mkdir -m 755 -p debian/disorder/etc/init.d
	mkdir -m 755 -p debian/disorder/usr/lib/cgi-bin/disorder
	mkdir -m 755 -p debian/disorder/var/lib/disorder
	mkdir -m 755 -p debian/disorder/usr/share/doc/disorder/ChangeLog.d
	$(INSTALL) -m 755 examples/disorder.init \
		debian/disorder/etc/init.d/disorder
	$(INSTALL) -m 644 debian/disorder.config \
		debian/disorder/etc/disorder/config
	$(INSTALL) -m 644 debian/options.debian \
		debian/disorder/etc/disorder/options
	$(LIBTOOL) --mode=install $(INSTALL) -m 755 server/disorder.cgi \
		$(shell pwd)/debian/disorder/usr/lib/cgi-bin/disorder/disorder
	dpkg-shlibdeps -Tdebian/substvars.disorder \
		debian/disorder/usr/bin/* \
		debian/disorder/usr/lib/cgi-bin/disorder/* \
		debian/disorder/usr/sbin/* \
		debian/disorder/usr/lib/*.so* \
		debian/disorder/usr/lib/disorder/*.so*
	$(INSTALL) -m 644 debian/htaccess \
		debian/disorder/usr/lib/cgi-bin/disorder/.htaccess
	$(INSTALL) -m 644 CHANGES README debian/README.Debian \
		BUGS README.* \
		debian/disorder/usr/share/doc/disorder/.
	$(INSTALL) -m 644 ChangeLog.d/*--* \
		debian/disorder/usr/share/doc/disorder/ChangeLog.d
	$(INSTALL) -m 644 COPYING debian/disorder/usr/share/doc/disorder/GPL
	gzip -9f debian/disorder/usr/share/doc/disorder/ChangeLog.d/*--* \
		 debian/disorder/usr/share/doc/disorder/CHANGES \
		 debian/disorder/usr/share/doc/disorder/README \
		 debian/disorder/usr/share/doc/disorder/README.* \
		 debian/disorder/usr/share/doc/disorder/BUGS \
		 debian/disorder/usr/share/doc/disorder/GPL \
		 debian/disorder/usr/share/man/man*/*
	$(INSTALL) -m 755 debian/postinst debian/prerm debian/postrm \
		debian/config \
		debian/disorder/DEBIAN/.
	$(INSTALL) -m 644 debian/conffiles debian/templates \
		debian/disorder/DEBIAN/.
])

binary

clean

regenerate
