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
    """Play some tracks"""
    dtest.start_daemon()
    dtest.create_user()
    c = disorder.client()
    track = u"%s/Joe Bloggs/First Album/02:Second track.ogg" % dtest.tracks
    print " adding track to queue"
    c.play(track)
    print " checking track turned up in queue"
    q = c.queue()
    ts = filter(lambda t: t['track'] == track and 'submitter' in t, q)
    assert len(ts) == 1, "checking track appears exactly once in queue"
    t = ts[0]
    assert t['submitter'] == u'fred', "check queue submitter"
    i = t['id']
    print " waiting for track"
    p = c.playing()
    r = c.recent()
    while not((p is not None and p['id'] == i)
              or (len(filter(lambda t: t['track'] == track and 'submitter' in t, r)) > 0)):
        time.sleep(1)
        p = c.playing()
        r = c.recent()
    print " checking track turned up in recent list"
    while (p is not None and p['id'] == i):
        time.sleep(1)
        p = c.playing()
    r = c.recent()
    ts = filter(lambda t: t['track'] == track and 'submitter' in t, r)
    assert len(ts) == 1, "check track appears exactly once in recent"
    t = ts[0]
    assert t['submitter'] == u'fred', "check recent entry submitter"
    print " disabling play"
    c.disable()
    print " scratching current track"
    p = c.playing()
    i = p['id']
    c.scratch(i)
    print " checking scratched track turned up in recent list"
    while (p is not None and p['id'] == i):
        time.sleep(1)
        p = c.playing()
    r = c.recent()
    ts = filter(lambda t: t['id'] == i, r)
    assert len(ts) == 1, "check scratched track appears exactly once in recent"
    assert ts[0]['state'] == 'scratched', "checking track scratched"
    print " waiting for scratch to complete"
    while (p is not None and p['state'] == 'isscratch'):
        time.sleep(1)
        p = c.playing()
    assert p is None, "checking nothing is playing"
    c.random_disable()
    assert c.random_enabled() == False
    assert c.enabled() == False
    c.enable()
    assert c.enabled() == True
    time.sleep(1)
    p = c.playing()
    assert p is None, "checking nothing playing when random disabled but playing enabled"
    c.random_enable()
    assert c.random_enabled() == True

if __name__ == '__main__':
    dtest.run()
