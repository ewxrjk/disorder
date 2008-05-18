#! /usr/bin/env python
#
# This file is part of DisOrder.
# Copyright (C) 2008 Richard Kettlewell
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
import dtest,disorder,sys,os,unicodedata

def recode(name, f, t):
  recoded = 0
  # First recode basename
  d = os.path.dirname(name)
  b = os.path.basename(name)
  nb = unicodedata.normalize("NFC", unicode(b, f)).encode(t)
  if b != nb:
    print "  %s -> %s" % (repr(b), repr(nb))
    name = os.path.join(d, nb)
    os.rename(os.path.join(d, b), name)
    recoded += 1
  # Recurse into directories
  if os.path.isdir(name):
    for c in os.listdir(name):
      recoded += recode(os.path.join(name, c), f, t)
  return recoded

def test():
    """Test encoding conversion"""
    if sys.platform == "darwin":
      print "Sorry, cannot run this test on Darwin"
      # ...because local fs is always UTF-8
      sys.exit(77)
    dtest.start_daemon()
    dtest.create_user()
    dtest.rescan()
    dtest.check_files()
    dtest.stop_daemon()
    print " recoding as ISO-8859-1"
    recoded = recode(dtest.tracks, "UTF-8", "ISO-8859-1")
    print " ...recoded %d filenames" % recoded
    print " regenerating config"
    dtest.default_config(encoding="ISO-8859-1")
    print " checking recoded files"
    dtest.start_daemon()
    dtest.rescan()
    dtest.check_files()
    dtest.stop_daemon()
    print " recoding as UTF-8"
    recoded = recode(dtest.tracks, "ISO-8859-1", "UTF-8")
    print " ...recoded %d filenames" % recoded
    print " regenerating config"
    dtest.default_config(encoding="UTF-8")
    print " checking recoded files"
    dtest.start_daemon()
    dtest.rescan()
    dtest.check_files()
    

if __name__ == '__main__':
    dtest.run()
