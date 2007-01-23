#
# This file is part of DisOrder.
# Copyright (C) 2004, 2005 Richard Kettlewell
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

bin_SCRIPTS=tkdisorder
nodist_python_PYTHON=disorder.py

SEDFILES=disorder.py

include ${top_srcdir}/scripts/sedfiles.make

EXTRA_DIST=disorder.py.in tkdisorder

BUILT_SOURCES=disorder.py

CLEANFILES=$(SEDFILES) $(HTMLMAN) *.pyc
# arch-tag:6b3bfd0829ab35f1a30e7d8bf9111b95
