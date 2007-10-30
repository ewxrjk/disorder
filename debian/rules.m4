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
	$(MKDIR) debian/disorder/etc/disorder
	$(MKDIR) debian/disorder/etc/init.d
	$(MKDIR) debian/disorder/usr/lib/cgi-bin/disorder
	$(MKDIR) debian/disorder/var/lib/disorder
	$(INSTALL_SCRIPT) examples/disorder.init \
		debian/disorder/etc/init.d/disorder
	$(INSTALL_DATA) debian/etc.disorder.config \
		debian/disorder/etc/disorder/config
	$(INSTALL_DATA) debian/etc.disorder.options \
		debian/disorder/etc/disorder/options
	$(LIBTOOL) --mode=install $(INSTALL_PROGRAM) server/disorder.cgi \
		$(shell pwd)/debian/disorder/usr/lib/cgi-bin/disorder/disorder
	dpkg-shlibdeps -Tdebian/substvars.disorder \
		debian/disorder/usr/bin/* \
		debian/disorder/usr/lib/cgi-bin/disorder/* \
		debian/disorder/usr/sbin/* \
		debian/disorder/usr/lib/disorder/*.so*
	$(INSTALL_DATA) debian/htaccess \
		debian/disorder/usr/lib/cgi-bin/disorder/.htaccess
	$(INSTALL_DATA) CHANGES README debian/README.Debian \
		BUGS README.* \
		debian/disorder/usr/share/doc/disorder/.
	bzr log > debian/disorder/usr/share/doc/disorder/changelog
	gzip -9f debian/disorder/usr/share/doc/disorder/changelog \
		 debian/disorder/usr/share/doc/disorder/CHANGES \
		 debian/disorder/usr/share/doc/disorder/README \
		 debian/disorder/usr/share/doc/disorder/README.* \
		 debian/disorder/usr/share/doc/disorder/BUGS \
		 debian/disorder/usr/share/man/man*/*
])

archpkg([disorder-playrtp], [	m4_dnl
	$(MKDIR) debian/disorder-playrtp/usr/bin
	$(MKDIR) debian/disorder-playrtp/usr/share/man/man1
	$(INSTALL_PROGRAM) clients/disorder-playrtp \
		debian/disorder-playrtp/usr/bin/disorder-playrtp
	$(INSTALL_DATA) doc/disorder-playrtp.1 \
		debian/disorder-playrtp/usr/share/man/man1/disorder-playrtp.1
	dpkg-shlibdeps -Tdebian/substvars.disorder-playrtp \
		debian/disorder-playrtp/usr/bin/*
	$(INSTALL_DATA) debian/README.RTP \
		debian/disorder-playrtp/usr/share/doc/disorder-playrtp/README
	$(INSTALL_DATA) CHANGES debian/disorder-playrtp/usr/share/doc/disorder-playrtp/CHANGES
	gzip -9f debian/disorder-playrtp/usr/share/doc/disorder-playrtp/CHANGES \
		 debian/disorder-playrtp/usr/share/man/man*/*
])

archpkg([disobedience], [	m4_dnl
	$(MKDIR) debian/disobedience/usr/bin
	$(MKDIR) debian/disobedience/usr/share/man/man1
	$(MKDIR) debian/disobedience/usr/share/pixmaps
	$(MKDIR) debian/disobedience/usr/share/menu
	$(MAKE) -C disobedience install DESTDIR=`pwd`/debian/disobedience
	$(INSTALL_DATA) doc/disobedience.1 \
		debian/disobedience/usr/share/man/man1/disobedience.1
	$(INSTALL_DATA) images/disobedience16x16.xpm \
		        images/disobedience32x32.xpm \
			debian/disobedience/usr/share/pixmaps
	$(INSTALL_DATA) debian/usr.share.menu.disobedience \
		debian/disobedience/usr/share/menu/disobedience
	$(INSTALL_DATA) debian/etc.disorder.config \
		debian/disorder/etc/disorder/config
	dpkg-shlibdeps -Tdebian/substvars.disobedience \
		debian/disobedience/usr/bin/*
	$(INSTALL_DATA) CHANGES debian/disobedience/usr/share/doc/disobedience/CHANGES
	gzip -9f debian/disobedience/usr/share/doc/disobedience/CHANGES \
		 debian/disobedience/usr/share/man/man*/*
])

binary

clean

regenerate
