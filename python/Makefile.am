#
# This file is part of DisOrder.
# Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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

noinst_SCRIPTS=tkdisorder
nodist_python_PYTHON=disorder.py

SEDFILES=disorder.py

include ${top_srcdir}/scripts/sedfiles.make

EXTRA_DIST=disorder.py.in tkdisorder

BUILT_SOURCES=disorder.py

CLEANFILES=$(SEDFILES) $(HTMLMAN) *.pyc
