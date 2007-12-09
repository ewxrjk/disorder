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
    """Check the queue is padded to the (default) configured length"""
    dtest.start_daemon()
    c = disorder.client()
    print " getting queue via python module"
    q = c.queue()
    assert len(q) == 10, "queue is at proper length"
    print " getting queue via disorder(1)"
    q = dtest.command(["disorder",
                       "--config", disorder._configfile, "--no-per-user-config",
                       "queue"])
    tracks = filter(lambda s: re.match("^track", s), q)
    assert len(tracks) == 10, "queue is at proper length"

if __name__ == '__main__':
    dtest.run()
