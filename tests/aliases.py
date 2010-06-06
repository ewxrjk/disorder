#! /usr/bin/env python
#
# This file is part of DisOrder.
# Copyright (C) 2008 Richard Kettlewell
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
import dtest,disorder

def test():
    """Exercise alias logic"""
    dtest.start_daemon()
    dtest.create_user()
    dtest.rescan()
    c = disorder.client()

    print " creating an alias in new directory"
    track = "%s/misc/blahblahblah.ogg" % dtest.tracks
    c.set(track,
          "trackname_display_artist",
          "Fred Smith")
    c.set(track,
          "trackname_display_album",
          "wibble")

    print " checking it shows up in the right place"
    alias = "%s/Fred Smith/wibble/blahblahblah.ogg" % dtest.tracks
    files = c.files("%s/Fred Smith/wibble" % dtest.tracks)
    assert files == [alias]

    print " checking part calculation"
    artist = c.part(track, "display", "artist")
    assert artist == "Fred Smith", "checking artist part"
    album = c.part(track, "display", "album")
    assert album == "wibble", "checking album part"
    title = c.part(track, "display", "title")
    assert title == "blahblahblah", "checking title part"

    print " checking part calculation on alias"
    artist = c.part(alias, "display", "artist")
    assert artist == "Fred Smith", "checking artist part"
    album = c.part(alias, "display", "album")
    assert album == "wibble", "checking album part"
    title = c.part(alias, "display", "title")
    assert title == "blahblahblah", "checking title part"

    # See defect #20
    print " checking that prefs always belong to the canonical name"
    c.set(alias, "wibble", "spong")
    value = c.get(track, "wibble")
    assert value == "spong", "checking pref ended up on resolved track"
    c.set(track, "foo", "bar")
    value = c.get(alias, "foo")
    assert value == "bar", "checking pref visible via alias"

if __name__ == '__main__':
    dtest.run()
