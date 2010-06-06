#! /usr/bin/env python
#
# This file is part of DisOrder.
# Copyright (C) 2009 Richard Kettlewell
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
import dtest,disorder,re

def set_auth_algo(h):
    configpath = "%s/config" % dtest.testroot
    config = open(configpath, "r").readlines()
    config = filter(lambda l: not re.match('authorization_algorithm', l),
                    config)
    config.append('authorization_algorithm %s\n' % h)
    open(configpath, "w").write("".join(config))

def test():
    """Authentication hash tests"""
    created = False
    for h in ['sha1', 'SHA1', 'sha256', 'SHA256',
              'sha384', 'SHA384', 'sha512', "SHA512" ]:
        print " setting authorization hash to %s" % h
        set_auth_algo(h)
        dtest.start_daemon()
        if not created:
            dtest.create_user()
            created = True
        print " exercising C implementation"
        dtest.command(["disorder",
                       "--config", disorder._configfile, "--no-per-user-config",
                       "--user", "root", "version"])
        print " exercising Python implementation"
        c = disorder.client()
        c.version()
        dtest.stop_daemon()

if __name__ == '__main__':
    dtest.run()
