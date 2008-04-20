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
import dtest,disorder

def test():
    """Test user database"""
    dtest.start_daemon()
    dtest.create_user()
    print " checking user creation"
    c = disorder.client()
    c.adduser("bob", "bobpass")
    users = c.users()
    assert dtest.lists_have_same_contents(users,
                                          ["fred", "bob", "root"])
    rights = c.userinfo("bob", "rights")
    print " new user rights are: %s" % rights
    print " checking new user can log in"
    c = disorder.client(user="bob", password="bobpass")
    c.version()
    print " checking bob can set their email address"
    c.edituser("bob", "email", "foo@bar")
    email = c.userinfo("bob", "email")
    assert email == "foo@bar", "checking bob's email address"
    print " checking user deletion"
    c = disorder.client()
    c.deluser("bob")
    print " checking new user can no longer log in"
    c = disorder.client(user="bob", password="bobpass")
    try:
      c.version()
      print "*** should not be able to log in after deletion ***"
      assert False
    except disorder.operationError:
      pass                              # good
    print " deleted user could no longer log in."
    c = disorder.client()
    users = c.users()
    assert dtest.lists_have_same_contents(users,
                                          ["fred", "root"])
    print " creating the guest user"
    dtest.command(["disorder",
                   "--config", disorder._configfile, "--no-per-user-config",
                   "--user", "root", "setup-guest"])
    print " logging in as guest user"
    gc = disorder.client(user="guest", password="")
    gc.version()
    print " testing user registration"
    cs = gc.register("joe", "joepass", "joe@nowhere.invalid")
    print " confirmation string is %s" % cs
    print " checking unconfirmed user cannot log in"
    jc = disorder.client(user="joe", password="joepass")
    try:
      jc.version()
      print "*** should not be able to log in before confirmation ***"
      assert False
    except disorder.operationError:
      pass                              # good
    print " confirming user"
    gc = disorder.client(user="guest", password="")
    gc.confirm(cs)
    print " checking confirmed user can log in"
    jc = disorder.client(user="joe", password="joepass")
    jc.version()
    print " checking new user's email address"
    assert c.userinfo("joe", "email") == "joe@nowhere.invalid"
    print " checking can change user email address"
    c.edituser("joe", "email", "joe@another.invalid")
    assert c.userinfo("joe", "email") == "joe@another.invalid"
    print " checking bad email address rejected"
    try:
      c.edituser("joe", "email", "invalid")
      print "*** should not be able to set invalid email address ***"
      assert False
    except disorder.operationError:
      pass                              # good
    assert c.userinfo("joe", "email") == "joe@another.invalid"
    print " checking removal of email address"
    c.edituser("joe", "email", "")
    assert c.userinfo("joe", "email") == None

if __name__ == '__main__':
    dtest.run()
