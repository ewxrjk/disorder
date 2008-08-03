#! /usr/bin/env python
#
# This file is part of DisOrder.
# Copyright (C) 2008 Richard Kettlewell
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
import dtest,disorder

def test():
    """Playlist testing"""
    dtest.start_daemon()
    dtest.create_user()
    c = disorder.client()
    c.random_disable()
    #
    print " checking initial playlist set is empty"
    l = c.playlists()
    assert l == [], "checking initial playlist set is empty"
    #
    print " creating a shared playlist"
    c.playlist_lock("wibble")
    c.playlist_set("wibble", ["one", "two", "three"])
    c.playlist_unlock()
    print " checking new playlist appears in list"
    l = c.playlists()
    assert l == ["wibble"], "checking new playlists"
    print " checking new playlist contents is as assigned"
    l = c.playlist_get("wibble")
    assert l == ["one", "two", "three"], "checking playlist contents"
    #
    print " checking new playlist is shared"
    s = c.playlist_get_share("wibble")
    assert s == "shared", "checking playlist is shared"
    #
    print " checking cannot unshare un-owned playlist"
    try:
        c.playlist_set_share("wibble", "private")
        print "*** should not be able to adjust shared playlist's sharing ***"
        assert False
    except disorder.operationError:
        pass                            # good
    #
    print " modifying shared playlist"
    c.playlist_lock("wibble")
    c.playlist_set("wibble", ["three", "two", "one"])
    c.playlist_unlock()
    print " checking updated playlist contents is as assigned"
    l = c.playlist_get("wibble")
    assert l == ["three", "two", "one"], "checking modified playlist contents"
    #
    print " creating a private playlist"
    c.playlist_lock("fred.spong")
    c.playlist_set("fred.spong", ["a", "b", "c"])
    c.playlist_unlock()
    s = c.playlist_get_share("fred.spong")
    assert s == "private", "owned playlists are private by default"
    #
    print " creating a public playlist"
    c.playlist_lock("fred.foo")
    c.playlist_set("fred.foo", ["p", "q", "r"])
    c.playlist_set_share("fred.foo", "public")
    c.playlist_unlock()
    s = c.playlist_get_share("fred.foo")
    assert s == "public", "new playlist is now public"
    #
    print " checking fred can see all playlists"
    l = c.playlists()
    assert dtest.lists_have_same_contents(l,
                                          ["fred.spong", "fred.foo", "wibble"]), "playlist list is as expected"
    #
    print " adding a second user"
    c.adduser("bob", "bobpass")
    d = disorder.client(user="bob", password="bobpass")
    print " checking bob cannot see fred's private playlist"
    l = d.playlists()
    assert dtest.lists_have_same_contents(l,
                                          ["fred.foo", "wibble"]), "playlist list is as expected"
    #
    print " checking bob can read shared and public playlists"
    d.playlist_get("wibble")
    d.playlist_get("fred.foo")
    print " checking bob cannot read private playlist"
    try:
        d.playlist_get("fred.spong")
        print "*** should not be able to read private playlist ***"
        assert False
    except disorder.operationError:
        pass                            # good
    #
    print " checking bob cannot modify fred's playlists"
    try:
        d.playlist_lock("fred.foo")
        print "*** should not be able to lock fred's public playlist ***"
        assert False
    except disorder.operationError:
        pass                            # good
    try:
        d.playlist_lock("fred.spong")
        print "*** should not be able to lock fred's private playlist ***"
        assert False
    except disorder.operationError:
        pass                            # good
    print " checking unlocked playlists cannot be modified"
    #
    try:
        c.playlist_set("wibble", ["a"])
        print "*** should not be able to modify unlocked playlists ***"
        assert False
    except disorder.operationError:
        pass                            # good
    

if __name__ == '__main__':
    dtest.run()
