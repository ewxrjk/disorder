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

EXTRA_DIST=TODO CHANGES.html README.streams BUGS \
README.upgrades README.client README.raw README.vhost README.developers
SUBDIRS=@subdirs@

DISTCHECK_CONFIGURE_FLAGS=httpdir=$(distdir)/_inst/httpdir

check-report: before-check check make-coverage-reports
before-check:
	rm -f */*.gcda */*.gcov
make-coverage-reports:
	@for subdir in lib plugins server clients disobedience; do \
	  echo cd $$subdir;\
	  cd $$subdir;\
	  echo ${GCOV} *.c | ${PYTHON} ../scripts/format-gcov-report --html . *.c ;\
	  ${GCOV} *.c | ${PYTHON} ../scripts/format-gcov-report --html . *.c ;\
	  echo cd ..;\
	  cd ..;\
	done

echo-distdir:
	@echo $(distdir)

echo-version:
	@echo $(VERSION)

uninstall-local:
	rmdir ${DESTDIR}${pkgdatadir} || true
	rmdir ${DESTDIR}${pkghttpdir} || true
	rmdir ${DESTDIR}${pkglibdir} || true
