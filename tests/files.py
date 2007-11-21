#! /usr/bin/env python
#
# This file is part of DisOrder.
# Copyright (C) 2007 Richard Kettlewell
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
import dtest,time,disorder,sys

def test():
    """Check that the file listing comes out right"""
    dtest.start_daemon()
    time.sleep(5)                       # give rescan a chance
    c = disorder.client()
    failures = 0
    for d in dtest.dirs_by_dir:
        xdirs = dtest.dirs_by_dir[d]
        dirs = c.directories(d)
        xdirs.sort()
        dirs.sort()
        if dirs != xdirs:
            print
            print "directory: %s" % d
            print "expected:  %s" % xdirs
            print "got:       %s" % dirs
            failures += 1
    for d in dtest.files_by_dir:
        xfiles = dtest.files_by_dir[d]
        files = c.files(d)
        xfiles.sort()
        files.sort()
        if files != xfiles:
            print
            print "directory: %s" % d
            print "expected:  %s" % xfiles
            print "got:       %s" % files
            failures += 1
    if failures:
        print
        sys.exit(1)

if __name__ == '__main__':
    dtest.run()
