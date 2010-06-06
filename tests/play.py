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
import dtest,time,disorder,re,sys

def test():
    """Play some tracks"""
    dtest.start_daemon()
    dtest.create_user()
    dtest.rescan()                      # ensure all files are scanned
    c = disorder.client()
    c.random_disable()
    assert c.random_enabled() == False
    track = u"%s/Joe Bloggs/First Album/02:Second track.ogg" % dtest.tracks
    track2 = u"%s/Joe Bloggs/First Album/04:Fourth track.ogg" % dtest.tracks
    track3 = u"%s/Joe Bloggs/First Album/05:Fifth track.ogg" % dtest.tracks
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

    print " ensuring queue is clear"
    c.disable()
    while c.playing() is not None:
        time.sleep(1)
    q = c.queue()
    for qe in q:
        c.remove(qe["id"])

    print " testing playafter"
    print "  adding to empty queue"
    c.playafter(None, [track])
    q = c.queue()
    print '\n'.join(map(lambda n: "%d: %s" % (n, q[n]["track"]),
                        range(0, len(q))))
    assert len(q) == 1
    assert q[0]['track'] == track
    print "  insert at start of queue"
    c.playafter(None, [track2])
    q = c.queue()
    print '\n'.join(map(lambda n: "%d: %s" % (n, q[n]["track"]),
                        range(0, len(q))))
    assert len(q) == 2
    assert q[0]['track'] == track2
    assert q[1]['track'] == track
    print "  insert in middle of queue"
    c.playafter(q[0]['id'], [track3])
    q = c.queue()
    print '\n'.join(map(lambda n: "%d: %s" % (n, q[n]["track"]),
                        range(0, len(q))))
    assert len(q) == 3
    assert q[0]['track'] == track2
    assert q[1]['track'] == track3
    assert q[2]['track'] == track
    print "  insert multiple tracks at end of queue"
    c.playafter(q[2]['id'], [track2, track])
    q = c.queue()
    print '\n'.join(map(lambda n: "%d: %s" % (n, q[n]["track"]),
                        range(0, len(q))))
    assert len(q) == 5
    assert q[0]['track'] == track2
    assert q[1]['track'] == track3
    assert q[2]['track'] == track
    assert q[3]['track'] == track2
    assert q[4]['track'] == track

    print " clearing queue"
    for qe in q:
        c.remove(qe["id"])

    print " testing scratches"
    retry = False
    scratchlimit = 5
    while scratchlimit > 0:
        scratchlimit -= 1
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
    if scratchlimit == 0:
        # TODO this is really not a great approach!
        print " didn't complete in a reasonable time"
        sys.exit(77)
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
