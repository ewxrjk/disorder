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
import dtest,time,disorder,re

def test():
    """Exercise database dumper"""
    dtest.start_daemon()
    dtest.create_user()
    dtest.rescan()
    c = disorder.client()
    track = "%s/Joe Bloggs/First Album/02:Second track.ogg" % dtest.tracks
    dump = "%s/dumpfile" % dtest.testroot
    print " setting a track pref"
    c.set(track, "foo", "before")
    assert c.get(track, "foo") == "before", "checking track foo=before"
    print " setting a global pref"
    c.setglobal("foo", "before");
    assert c.getglobal("foo") == "before", "checking global foo=before"
    print " adding a tag"
    # Exercise the tags-changed code
    c.set(track, "tags", "  first   tag, Another Tag")
    assert dtest.lists_have_same_contents(c.tags(),
                                          [u"another tag", u"first tag"]),\
           "checking tag list(1)"
    c.set(track, "tags", "wibble,   another tag   ")
    assert dtest.lists_have_same_contents(c.tags(),
                                          [u"another tag", u"wibble"]),\
           "checking tag list(2)"
    print " checking track appears in tag search"
    tracks = c.search(["tag:wibble"])
    assert len(tracks) == 1, "checking there is exactly one search result(1)"
    assert tracks[0] == track, "checking for right search result(1)"
    tracks = c.search(["tag:  another    tAg  "])
    assert len(tracks) == 1, "checking there is exactly one search result(2)"
    assert tracks[0] == track, "checking for right search result(2)"
    print " dumping database"
    print dtest.command(["disorder-dump", "--config", disorder._configfile,
                         "--dump", dump])
    print " changing track pref"
    c.set(track, "foo", "after dump");
    assert c.get(track, "foo") == "after dump", "checking track foo=after dump"
    print " changing global pref"
    c.setglobal("foo", "after dump");
    assert c.getglobal("foo") == "after dump", "checking global foo=after dump"
    print " adding fresh track pref"
    c.set(track, "bar", "after dump")
    print " adding fresh global pref"
    c.setglobal("bar", "after dump")
    dtest.stop_daemon();
    print "restoring database"
    print dtest.command(["disorder-dump", "--config", disorder._configfile,
                         "--undump", dump])
    dtest.start_daemon(); 
    c = disorder.client()
    print " checking track pref"
    assert c.get(track, "foo") == "before", "checking track foo=before after undump"
    print " checking global pref"
    assert c.getglobal("foo") == "before", "checking global foo=before after undump"
    print " checking fresh track pref"
    assert c.get(track, "bar") is None, "checking fresh track pref has gone"
    print " checking fresh global pref"
    assert c.getglobal("bar") is None, "checking fresh global pref has gone"
    print " checking tag search still works"
    tracks = c.search(["tag:wibble"])
    assert len(tracks) == 1, "checking there is exactly one search result"
    assert tracks[0] == track, "checking for right search result(3)"
    assert dtest.lists_have_same_contents(c.tags(),
                                          [u"another tag", u"wibble"]),\
           "checking tag list(3)"

if __name__ == '__main__':
    dtest.run()
