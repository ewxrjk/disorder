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
import dtest,disorder,time,string

def now():
    """Return the current time in whole seconds"""
    return int(time.time())

def next_playing(c):
    print " waiting for track to play"
    p = c.playing()
    waited = 0
    while p is None and waited < 10:
        time.sleep(1)
        print "  ."
        p = c.playing()
    assert waited < 10, "track played in a reasonable time"
    return p

def wait_idle(c):
    print " waiting for nothing to be playing"
    p = c.playing()
    waited = 0
    while p is not None and waited < 20:
        time.sleep(1)
        print "  ."
        p = c.playing()
    assert waited < 20, "idled in a reasonable time"

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
        print "  ."
    track = "%s/Joe Bloggs/First Album/05:Fifth track.ogg" % dtest.tracks
    print " scheduling a track for the future"
    when = now() + 3
    c.schedule_add(when, "normal", "play", track)
    print " disorder schedule-list output:"
    print string.join(dtest.command(["disorder",
                                     "--config", disorder._configfile,
                                     "--no-per-user-config",
                                     "schedule-list"]), ""),
    p = next_playing(c)
    assert p["track"] == track, "checking right track played"
    assert int(p["when"]) >= when, "checking track played at right time"
    assert c.schedule_list() == [], "checking schedule is empty"
    wait_idle(c)
    print " scheduling an enable-random for the future"
    c.schedule_add(now() + 3, "junk", "set-global", "random-play", "yes")
    print " disorder schedule-list output:"
    print string.join(dtest.command(["disorder",
                                     "--config", disorder._configfile,
                                     "--no-per-user-config",
                                     "schedule-list"]), ""),
    next_playing(c)
    print " disabling random play"
    c.random_disable()
    wait_idle(c)
    print " scheduling track to play later via command line"
    when = now() + 3
    dtest.command(["disorder",
                   "--config", disorder._configfile,
                   "--no-per-user-config",
                   "schedule-play",
                   time.strftime("%Y-%m-%d %H:%M:%S",
                                 time.localtime(when)),
                   "normal",
                   track])
    print " disorder schedule-list output:"
    print string.join(dtest.command(["disorder",
                                     "--config", disorder._configfile,
                                     "--no-per-user-config",
                                     "schedule-list"]), ""),
    p = next_playing(c)
    assert p["track"] == track, "checking right track played"
    assert p["when"] >= when, "checking track played at right time"
    assert c.schedule_list() == [], "checking schedule is empty"
    wait_idle(c)
    print " scheduling an enable-random for later via command line"
    dtest.command(["disorder",
                   "--config", disorder._configfile,
                   "--no-per-user-config",
                   "schedule-set-global",
                   time.strftime("%Y-%m-%d %H:%M:%S",
                                 time.localtime(now() + 3)),
                   "normal",
                   "random-play",
                   "yes"])
    print " disorder schedule-list output:"
    print string.join(dtest.command(["disorder",
                                     "--config", disorder._configfile,
                                     "--no-per-user-config",
                                     "schedule-list"]), ""),
    p = next_playing(c)
    print " disabling random play"
    c.random_disable()
    print " waiting for nothing to be playing"
    while c.playing() is not None:
        time.sleep(1)
        print "  ."
    print " scheduling a track for the future"
    c.schedule_add(now() + 3, "normal", "play", track)
    print " schedule via python:"
    s = c.schedule_list()
    for event in s:
        e = c.schedule_get(event)
        print "item %s: %s" % (event, e)
    print " deleting item %s" % s[0]
    c.schedule_del(s[0])
    print " checking it's really gone"
    s = c.schedule_list()
    assert s == [], "checking schedule is empty"
    waited = 0
    p = c.playing()
    while p is None and waited < 5:
        time.sleep(1)
        print "  ."
        waited += 1
        p = c.playing()
    assert p is None, "checking deleted scheduled event did not run"
    print " checking you can't schedule events for the past"
    try:
        c.schedule_add(now() - 4, "normal", "play", track)
        assert False, "checking schedule_add failed"
    except disorder.operationError:
      pass                              # good
    print " checking scheduled events survive restarts"
    when = now() + 3
    c.schedule_add(when, "normal", "play", track)
    dtest.stop_daemon()
    print " dumping database"
    dump = "%s/dumpfile" % dtest.testroot
    print dtest.command(["disorder-dump", "--config", disorder._configfile,
                         "--dump", dump])
    print "restoring database"
    print dtest.command(["disorder-dump", "--config", disorder._configfile,
                         "--undump", dump])
    dtest.start_daemon()
    c = disorder.client()
    p = next_playing(c)
    print " waiting for track to play"
    assert p["track"] == track, "checking right track played"
    assert p["when"] >= when, "checking track played at right time"
    assert c.schedule_list() == [], "checking schedule is empty"
    print " checking junk events do not survive restarts"
    c.schedule_add(now() + 2, "junk", "play", track)
    s = c.schedule_list()
    print s
    dtest.stop_daemon()
    time.sleep(3)
    dtest.start_daemon()
    c = disorder.client()
    print " checking schedule is empty"
    s = c.schedule_list()
    print s
    assert s == [], "checking schedule is empty"

if __name__ == '__main__':
    dtest.run()
