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
import dtest,time,disorder,re

def test():
    """Exercise database dumper"""
    dtest.start_daemon()
    c = disorder.client()
    track = "%s/Joe Bloggs/First Album/02:Second track.ogg" % dtest.tracks
    dump = "%s/dumpfile" % dtest.testroot
    print "setting a track pref"
    c.set(track, "foo", "before")
    assert c.get(track, "foo") == "before", "checking track foo=before"
    print "setting a global pref"
    c.setglobal("foo", "before");
    assert c.getglobal("foo") == "before", "checking global foo=before"
    print "adding a tag"
    # Exercise the tags-changed code
    c.set(track, "tags", "  first   tag, Another Tag")
    assert dtest.lists_have_same_contents(c.tags(),
                                          [u"another tag", u"first tag"]),\
           "checking tag list(1)"
    c.set(track, "tags", "wibble,   another tag   ")
    assert dtest.lists_have_same_contents(c.tags(),
                                          [u"another tag", u"wibble"]),\
           "checking tag list(2)"
    print "checking track appears in tag search"
    tracks = c.search(["tag:wibble"])
    assert len(tracks) == 1, "checking there is exactly one search result"
    assert tracks[0] == track, "checking for right search result"
    print "dumping database"
    print dtest.command(["disorder-dump", "--config", disorder._configfile,
                         "--dump", dump])
    print "changing track pref"
    c.set(track, "foo", "after");
    assert c.get(track, "foo") == "after", "checking track foo=before"
    print "changing global pref"
    c.setglobal("foo", "after");
    assert c.getglobal("foo") == "after", "checking global foo=before"
    print "adding fresh track pref"
    c.set(track, "bar", "after")
    print "adding fresh global pref"
    c.setglobal("bar", "after")
    dtest.stop_daemon();
    print "restoring database"
    print dtest.command(["disorder-dump", "--config", disorder._configfile,
                         "--undump", dump])
    dtest.start_daemon(); 
    c = disorder.client()
    print "checking track pref"
    assert c.get(track, "foo") == "before", "checking track foo=before after undump"
    print "checking global pref"
    assert c.getglobal("foo") == "before", "checking global foo=before after undump"
    print "checking fresh track pref"
    assert c.get(track, "bar") is None, "checking fresh track pref has gone"
    print "checking fresh global pref"
    assert c.getglobal("bar") is None, "checking fresh global pref has gone"
    print "checking tag search still works"
    tracks = c.search(["tag:wibble"])
    assert len(tracks) == 1, "checking there is exactly one search result"
    assert tracks[0] == track, "checking for right search result"
    assert dtest.lists_have_same_contents(c.tags(),
                                          [u"another tag", u"wibble"]),\
           "checking tag list(3)"

if __name__ == '__main__':
    dtest.run()
