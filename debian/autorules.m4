#! /usr/bin/make -f
#
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
# This file was generated automatically - edit rules.m4 instead
#

INSTALL=install
CONFIGURE=--prefix=/usr

m4_divert(-1)m4_dnl

m4_changequote([,])

m4_define([build], [.PHONY: [build]
[build]:
m4_syscmd([test -f ../configure || test -f ../config.status])m4_dnl
m4_ifelse(m4_sysval,0,[	if test -f config.status; then \
	  ./config.status; else\
	  ./configure ${CONFIGURE} ${CONFIGURE_EXTRA}; fi
])m4_dnl
	$(MAKE) prefix=/usr])m4_dnl

m4_define([binary], [.PHONY: [binary] [binary]-arch [binary]-indep
[binary]: [binary]-arch [binary]-indep
[binary]-arch: _archpkgs
[binary]-indep: _indeppkgs])

m4_define([anypkg], [m4_define([_package], $1)m4_dnl
m4_define([cleanup], cleanup [cleanpkg-$1])m4_dnl
.PHONY: cleanpkg-$1
cleanpkg-$1:
	rm -rf debian/$1

.PHONY: pkg-$1
pkg-$1: [build]
	rm -rf debian/$1
	mkdir -p debian/$1
	mkdir -p debian/$1/DEBIAN
	mkdir -p debian/$1/usr/share/doc/$1
	cp debian/copyright \
		debian/$1/usr/share/doc/$1/copyright
	cp debian/changelog \
		debian/$1/usr/share/doc/$1/changelog.Debian
	gzip -9 debian/$1/usr/share/doc/$1/changelog.Debian
	@for f in preinst postinst prerm postrm conffiles templates config; do\
	  if test -e debian/$$f.$1; then\
	    echo cp debian/$$f.$1 debian/$1/DEBIAN/$$f; \
	    cp debian/$$f.$1 debian/$1/DEBIAN/$$f; \
	  fi;\
	done
$2	dpkg-gencontrol -isp -p$1 -Pdebian/$1 -Tdebian/substvars.$1
	chown -R root:root debian/$1
	chmod -R g-ws debian/$1
	dpkg --[build] debian/$1 ..
])

m4_define([_target],
	[m4_ifelse([$2],[],[$1],[$2])])

m4_define([install_usrbin],
	 [$(INSTALL) -m 755 $1 \
		debian/_package/usr/bin/_target([$1],[$2])])

m4_define([install_usrsbin],
	 [$(INSTALL) -m 755 $1 \
		debian/_package/usr/sbin/_target([$1],[$2])])

m4_define([install_bin],
	 [$(INSTALL) -m 755 $1 \
		debian/_package/bin/_target([$1],[$2])])

m4_define([install_sbin],
	 [$(INSTALL) -m 755 $1 \
		debian/_package/sbin/_target([$1],[$2])])

m4_define([_mansect],
	[m4_patsubst([$1], [^.*\.\([^.]*\)], [\1])])

m4_define([install_usrman],
	[$(INSTALL) -m 644 $1 \
		debian/_package/usr/share/man/man[]_mansect(_target([$1],[$2]))/_target([$1],[$2])
	gzip -9 debian/_package/usr/share/man/man[]_mansect(_target([$1],[$2]))/_target([$1],[$2])])

m4_define([install_manlink],
	[ln -s ../man[]_mansect([$1])/$1.gz \
		debian/_package/usr/man/man[]_mansect([$2])/$2.gz])

m4_define([archpkg], [m4_define([_archpkgs], _archpkgs pkg-$1)m4_dnl
anypkg([$1],[$2])])

m4_define([indeppkg], [m4_define([_indeppkgs], _indeppkgs pkg-$1)m4_dnl
anypkg([$1],[$2])])

m4_define([clean], [.PHONY: [clean]
[clean]: cleanup
	-$(MAKE) distclean
	rm -f config.cache
	rm -f debian/files
	rm -f debian/substvars.*])

m4_define([cleanup], [])

m4_define([_archpkgs], [])

m4_define([_indeppkgs], [])

m4_define([regenerate], [debian/rules: debian/autorules.m4 debian/rules.m4
	rm -f debian/rules.tmp
	cd debian && \
		m4 -P autorules.m4 rules.m4 > rules.tmp
	chmod 555 debian/rules.tmp
	mv -f debian/rules.tmp debian/rules
])

m4_divert(0)m4_dnl
