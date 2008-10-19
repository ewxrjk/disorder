#! /usr/bin/env python
#
# This file is part of DisOrder.
# Copyright (C) 2007, 2008 Richard Kettlewell
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
import dtest,time,disorder,sys

def test():
    """Check that the file listing comes out right"""
    dtest.start_daemon()
    dtest.create_user()
    dtest.rescan()
    assert dtest.check_files() == 0, "dtest.check_files"
    print " checking regexp file listing"
    c = disorder.client()
    f = c.files("%s/Joe Bloggs/First Album" % dtest.tracks,
                "second")
    assert len(f) == 1, "checking for one match"
    assert f[0] == "%s/Joe Bloggs/First Album/02:Second track.ogg" % dtest.tracks
    print " and again to exercise cache"
    f = c.files("%s/Joe Bloggs/First Album" % dtest.tracks,
                "second")
    assert len(f) == 1, "checking for one match"
    assert f[0] == "%s/Joe Bloggs/First Album/02:Second track.ogg" % dtest.tracks
    print " checking unicode regexp file listing"
    f = c.files("%s/Joe Bloggs/First Album" % dtest.tracks,
                "first")
    assert len(f) == 0, "checking for 0 matches"
    print " and again to exercise cache"
    f = c.files("%s/Joe Bloggs/First Album" % dtest.tracks,
                "first")
    assert len(f) == 0, "checking for 0 matches"
    # This is rather unsatisfactory but it is the current behavior.  We could
    # for instance go to NFD for regexp matching but we'd have to do the same
    # to the regexp, including replacing single characters with (possibly
    # bracketed) decomposed forms.  Really the answer has to be a more
    # Unicode-aware regexp library.
    f = c.files("%s/Joe Bloggs/First Album" % dtest.tracks,
                "fi\\p{Mn}*rst")
    assert len(f) == 0, "checking for 0 matches"

if __name__ == '__main__':
    dtest.run()
