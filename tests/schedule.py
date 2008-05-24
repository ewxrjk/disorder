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
import dtest,disorder,time

def test():
    """Exercise schedule support"""
    dtest.start_daemon()
    dtest.create_user()
    c = disorder.client()
    c.random_disable()
    dtest.rescan()
    # Wait until there's no track playing
    print " waiting for nothing to be playing"
    while c.playing() is not None:
        time.sleep(1)
    now = int(time.time())
    track = "%s/Joe Bloggs/First Album/05:Fifth track.ogg" % dtest.tracks
    print " scheduling a track for the future"
    c.schedule_add(now + 4, "normal", "play", track)
    print " waiting for it to play"
    waited = 0
    p = c.playing()
    while p is None and waited < 10:
        time.sleep(1)
        waited += 1
        p = c.playing()
    assert waited < 10, "checking track played within a reasonable period"
    assert waited > 2, "checking track didn't play immediately"
    assert p["track"] == track, "checking right track played"
    print " waiting for nothing to be playing"
    while c.playing() is not None:
        time.sleep(1)
    print " scheduling an enable-random for the future"
    now = int(time.time())
    c.schedule_add(now + 4, "normal", "set-global", "random-play", "yes")
    print " waiting for it to take effect"
    waited = 0
    p = c.playing()
    while p is None and waited < 10:
        time.sleep(1)
        waited += 1
        p = c.playing()
    assert waited < 10, "checking a track played within a reasonable period"
    assert waited > 2, "checking a track didn't play immediately"

if __name__ == '__main__':
    dtest.run()
