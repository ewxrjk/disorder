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

if __name__ == '__main__':
    dtest.run()
