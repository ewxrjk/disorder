#! /usr/bin/env python -u
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
import dtest,disorder

def test():
    """Exercise cookie protocol"""
    dtest.start_daemon()
    dtest.create_user()
    print " connecting"
    c = disorder.client()
    v = c.version()
    print " getting cookie"
    k = c.make_cookie()
    print " cookie value is %s" % k
    print " connecting with cookie"
    c = disorder.client()
    c.connect(k)
    v = c.version()
    print " it worked"
    print " connecting with cookie again"
    c = disorder.client()
    c.connect(k)
    v = c.version()
    print " it worked"
    print " revoking cookie"
    c.revoke()
    v = c.version()
    print " connection still works"
    print " connecting with revoked cookie"
    c = disorder.client()
    try:
      c.connect(k)
      print "*** should not be able to connect with revoked cookie ***"
      assert False
    except disorder.operationError:
      pass                              # good
    print " revoked cookie was rejected"

if __name__ == '__main__':
    dtest.run()
