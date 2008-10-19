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
import dtest,time,disorder,sys,re,subprocess

def test():
    """Database version tests"""
    # Start up with dbversion 1
    config = "%s/config" % dtest.testroot
    configsave = "%s.save" % config
    dtest.copyfile(config, configsave)
    open(config, "a").write("dbversion 1\n")
    dtest.start_daemon()
    dtest.create_user()
    dtest.rescan()
    dtest.stop_daemon()
    # Revert to default configuration
    dtest.copyfile(configsave, config)
    print " testing daemon manages to upgrade..."
    dtest.start_daemon()
    assert dtest.check_files() == 0, "dtest.check_files"
    print " getting server version"
    c = disorder.client()
    v = c.version()
    print "Server version: %s" % v
    print " getting server stats"
    s = c.stats()

if __name__ == '__main__':
    dtest.run()
