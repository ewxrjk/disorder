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
	rm -f debian/disorder/usr/bin/disorder-playrtp
	rm -f debian/disorder/usr/bin/disobedience
	rm -f debian/disorder/usr/share/man/man1/disorder-playrtp.1
	rm -f debian/disorder/usr/share/man/man1/disobedience.1
	mkdir -m 755 -p debian/disorder/etc/disorder
	mkdir -m 755 -p debian/disorder/etc/init.d
	mkdir -m 755 -p debian/disorder/usr/lib/cgi-bin/disorder
	mkdir -m 755 -p debian/disorder/var/lib/disorder
	mkdir -m 755 -p debian/disorder/usr/share/doc/disorder/ChangeLog.d
	$(INSTALL) -m 755 examples/disorder.init \
		debian/disorder/etc/init.d/disorder
	$(INSTALL) -m 644 debian/etc.disorder.config \
		debian/disorder/etc/disorder/config
	$(INSTALL) -m 644 debian/etc.disorder.options \
		debian/disorder/etc/disorder/options
	$(LIBTOOL) --mode=install $(INSTALL) -m 755 server/disorder.cgi \
		$(shell pwd)/debian/disorder/usr/lib/cgi-bin/disorder/disorder
	dpkg-shlibdeps -Tdebian/substvars.disorder \
		debian/disorder/usr/bin/* \
		debian/disorder/usr/lib/cgi-bin/disorder/* \
		debian/disorder/usr/sbin/* \
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
])

archpkg([disorder-playrtp], [	m4_dnl
	mkdir -p debian/disorder-playrtp/usr/bin
	mkdir -p debian/disorder-playrtp/usr/share/man/man1
	$(INSTALL) -m 755 clients/disorder-playrtp \
		debian/disorder-playrtp/usr/bin/disorder-playrtp
	$(INSTALL) -m 755 doc/disorder-playrtp.1 \
		debian/disorder-playrtp/usr/share/man/man1/disorder-playrtp.1
	dpkg-shlibdeps -Tdebian/substvars.disorder-playrtp \
		debian/disorder-playrtp/usr/bin/*
	$(INSTALL) -m 644 debian/README.RTP \
		debian/disorder-playrtp/usr/share/doc/disorder-playrtp/README
	$(INSTALL) -m 644 COPYING debian/disorder-playrtp/usr/share/doc/disorder-playrtp/GPL
	$(INSTALL) -m 644 CHANGES debian/disorder-playrtp/usr/share/doc/disorder-playrtp/CHANGES
	gzip -9f debian/disorder-playrtp/usr/share/doc/disorder-playrtp/GPL \
	         debian/disorder-playrtp/usr/share/doc/disorder-playrtp/CHANGES \
		 debian/disorder-playrtp/usr/share/man/man*/*
])

archpkg([disobedience], [	m4_dnl
	mkdir -p debian/disobedience/usr/bin
	mkdir -p debian/disobedience/usr/share/man/man1
	$(INSTALL) -m 755 disobedience/disobedience \
		debian/disobedience/usr/bin/disobedience
	$(INSTALL) -m 755 doc/disobedience.1 \
		debian/disobedience/usr/share/man/man1/disobedience.1
	$(INSTALL) -m 644 debian/etc.disorder.config \
		debian/disorder/etc/disorder/config
	dpkg-shlibdeps -Tdebian/substvars.disobedience \
		debian/disobedience/usr/bin/*
	$(INSTALL) -m 644 COPYING debian/disobedience/usr/share/doc/disobedience/GPL
	$(INSTALL) -m 644 CHANGES debian/disobedience/usr/share/doc/disobedience/CHANGES
	gzip -9f debian/disobedience/usr/share/doc/disobedience/GPL \
	         debian/disobedience/usr/share/doc/disobedience/CHANGES \
		 debian/disobedience/usr/share/man/man*/*
])

binary

clean

regenerate
