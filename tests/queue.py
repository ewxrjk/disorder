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

class wait_monitor(disorder.monitor):
    def queue(self, q):
        return False

def test():
    """Check the queue is padded to the (default) configured length"""
    dtest.start_daemon()
    dtest.create_user()
    c = disorder.client()
    print " disabling play"
    c.disable()
    print " waiting for queue to be populated..."
    q = c.queue()
    while len(q) < 5:
        print "  queue at %d tracks" % len(q)
        wait_monitor().run()
        q = c.queue()
    print " getting queue via disorder(1)"
    q = dtest.command(["disorder",
                       "--config", disorder._configfile, "--no-per-user-config",
                       "queue"])
    tracks = filter(lambda s: re.match("^track", s), q)
    assert len(tracks) == 5, "queue is at proper length"
    print " disabling random play"
    c.random_disable()
    print " emptying queue"
    for t in c.queue():
        c.remove(t['id'])
    print " checking queue is now empty"
    q = c.queue()
    assert q == [], "checking queue is empty"
    print " enabling random play"
    c.random_enable()
    print " waiting for queue to refill..."
    q = c.queue()
    while len(q) < 5:
        print "  queue at %d tracks" % len(q)
        wait_monitor().run()
        q = c.queue()
    print " disabling all play"
    c.random_disable()
    c.disable()
    print " emptying queue"
    for t in c.queue():
        c.remove(t['id'])
    t1 = "%s/Joe Bloggs/Third Album/01:First_track.ogg" % dtest.tracks
    t2 = "%s/Joe Bloggs/Third Album/02:Second_track.ogg" % dtest.tracks
    t3 = "%s/Joe Bloggs/Third Album/02:Second_track.ogg" % dtest.tracks
    print " adding tracks"
    i1 = c.play(t1)
    i2 = c.play(t2)
    i3 = c.play(t3)
    q = c.queue()
    assert map(lambda e:e['id'], q) == [i1, i2, i3], "checking queue order(1)"
    print " moving last track to start"
    c.moveafter(None, [i3])
    q = c.queue()
    assert map(lambda e:e['id'], q) == [i3, i1, i2], "checking queue order(2)"
    print " moving two tracks"
    c.moveafter(i1, [i2, i3])
    q = c.queue()
    assert map(lambda e:e['id'], q) == [i1, i2 ,i3], "checking queue order(3)"
    print " removing a track"
    c.remove(i2)
    q = c.queue()
    assert map(lambda e:e['id'], q) == [i1 ,i3], "checking queue order(4)"

if __name__ == '__main__':
    dtest.run()
