#! /usr/bin/env python
#
# This file is part of DisOrder.
# Copyright (C) 2007, 2008 Richard Kettlewell
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
    """Play some tracks"""
    dtest.start_daemon()
    dtest.create_user()
    dtest.rescan()                      # ensure all files are scanned
    c = disorder.client()
    c.random_disable()
    assert c.random_enabled() == False
    track = u"%s/Joe Bloggs/First Album/02:Second track.ogg" % dtest.tracks
    print " adding track to queue"
    c.disable()
    assert c.enabled() == False
    c.play(track)
    print " checking track turned up in queue"
    q = c.queue()
    ts = filter(lambda t: t['track'] == track and 'submitter' in t, q)
    assert len(ts) == 1, "checking track appears exactly once in queue"
    t = ts[0]
    assert t['submitter'] == u'fred', "check queue submitter"
    i = t['id']
    print " waiting for track"
    c.enable()
    assert c.enabled() == True
    p = c.playing()
    r = c.recent()
    limit = 60
    while not((p is not None and p['id'] == i)
              or (len(filter(lambda t: t['track'] == track
                             and 'submitter' in t, r)) > 0)) and limit > 0:
        time.sleep(1)
        p = c.playing()
        r = c.recent()
        limit -= 1
    assert limit > 0, "check track did complete in a reasonable time"
    print " checking track turned up in recent list"
    while (p is not None and p['id'] == i):
        time.sleep(1)
        p = c.playing()
    r = c.recent()
    ts = filter(lambda t: t['track'] == track and 'submitter' in t, r)
    assert len(ts) == 1, "check track appears exactly once in recent"
    t = ts[0]
    assert t['submitter'] == u'fred', "check recent entry submitter"

    print " testing scratches"
    retry = False
    while True:
        c.disable()
        print " starting a track"
        c.play(track)
        c.enable()
        p = c.playing()
        if p is None:
            print " track played too quickly, trying again..."
            continue
        print " scratching track"
        i = p['id']
        c.scratch(i)
        print " waiting for track to finish"
        p = c.playing()
        limit = 60
        while (p is not None and p['id'] == i) and limit > 0:
            time.sleep(1)
            p = c.playing()
            limit -= 1
        assert limit > 0, "check track finishes in a reasonable period"
        print " checking scratched track turned up in recent list"
        r = c.recent()
        ts = filter(lambda t: t['id'] == i, r)
        assert len(ts) == 1, "check scratched track appears exactly once in recent"
        if ts[0]['state'] == 'ok':
            print " track played too quickly, trying again..."
            continue
        assert ts[0]['state'] == 'scratched', "checking track scratched"
        break
    print " waiting for scratch to complete"
    p = c.recent()
    while p is not None:
        time.sleep(1)
        p = c.playing()
    assert p is None, "checking nothing is playing"
    assert c.enabled() == True
    c.random_enable()
    assert c.random_enabled() == True

if __name__ == '__main__':
    dtest.run()
