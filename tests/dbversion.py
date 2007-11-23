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
import dtest,time,disorder,sys,re,subprocess

def test():
    """Database version tests"""
    # Start up with dbversion 1
    config = "%s/config" % dtest.testroot
    configsave = "%s.save" % config
    dtest.copyfile(config, configsave)
    open(config, "a").write("dbversion 1\n")
    dtest.start_daemon()
    dtest.stop_daemon()
    # Revert to default configuration
    dtest.copyfile(configsave, config)
    print "Testing daemon manages to upgrade..."
    dtest.start_daemon()
    assert dtest.check_files() == 0

if __name__ == '__main__':
    dtest.run()
