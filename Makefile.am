#
# This file is part of DisOrder.
# Copyright (C) 2004-2008, 2010, 2011, 2013 Richard Kettlewell
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

EXTRA_DIST=CHANGES.html README.streams BUGS \
README.upgrades.html README.client README.vhost README.developers \
docs.css \
disorder.sln
SUBDIRS=@subdirs@

DISTCHECK_CONFIGURE_FLAGS:=httpdir=$(distdir)/_inst/httpdir cgiexecdir=$(shell pwd)/$(distdir)/_inst/cgiexecdir

ACLOCAL_AMFLAGS=-I m4

check-report: before-check check make-coverage-reports
before-check:
	rm -f */*.gcda */*.gcov
make-coverage-reports: check
	@for subdir in lib plugins server clients disobedience; do \
	  echo cd $$subdir;\
	  cd $$subdir;\
	  echo ${GCOV} *.c | ${PYTHON} ../scripts/format-gcov-report --html . *.c ;\
	  ${GCOV} *.c | ${PYTHON} ../scripts/format-gcov-report --html . *.c ;\
	  echo cd ..;\
	  cd ..;\
	done

clean-gcov:
	rm -f */index.html
	rm -f */*.c.html
	rm -f */*.gcov

echo-distdir:
	@echo $(distdir)

echo-version:
	@echo $(VERSION)

uninstall-local:
	rmdir ${DESTDIR}${pkgdatadir} || true
	rmdir ${DESTDIR}${pkghttpdir} || true
	rmdir ${DESTDIR}${pkglibdir} || true
