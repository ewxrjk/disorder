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
import dtest,disorder

def test():
    """Test upgrade to new user database"""
    print " testing upgrade from old versions"
    open("%s/config" % dtest.testroot, "a").write(
      """allow fred fredpass
trust fred
""")
    dtest.start_daemon()
    print " checking can log in after upgrade"
    c = disorder.client()
    c.version()
    dtest.stop_daemon()
    dtest.default_config()
    dtest.start_daemon()
    print " checking can log in after removing 'allow'"
    c = disorder.client()
    c.version()

if __name__ == '__main__':
    dtest.run()
