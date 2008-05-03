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
import dtest,time,disorder,sys

failures = 0

def check_search_results(terms, expected):
    global failures
    # We want a consistent encoding and ordering
    print "terms:    %s" % terms
    got = client.search(terms)
    got = map(dtest.nfc, got)
    expected = map(lambda s: "%s/%s" % (dtest.tracks, s), expected)
    expected = map(dtest.nfc, expected)
    got.sort()
    expected.sort()
    if got != expected:
        print "expected: %s" % expected
        print "got:      %s" % got
        print
        failures += 1

def test():
    """Check that the search produces the right results"""
    dtest.start_daemon()
    dtest.create_user()
    dtest.rescan()
    global client
    client = disorder.client()
    first = ["Joe Bloggs/First Album/01:F\xC3\x8Crst track.ogg",
             "Joe Bloggs/First Album/02:Second track.ogg",
             "Joe Bloggs/First Album/03:ThI\xCC\x81rd track.ogg",
             "Joe Bloggs/First Album/04:Fourth track.ogg",
             "Joe Bloggs/First Album/05:Fifth track.ogg",
             "Joe Bloggs/Second Album/01:First track.ogg",
             "Joe Bloggs/Third Album/01:First_track.ogg"]
    second = ["Joe Bloggs/First Album/02:Second track.ogg",
              "Joe Bloggs/Second Album/01:First track.ogg",
              "Joe Bloggs/Second Album/02:Second track.ogg",
              "Joe Bloggs/Second Album/03:Third track.ogg",
              "Joe Bloggs/Second Album/04:Fourth track.ogg",
              "Joe Bloggs/Second Album/05:Fifth track.ogg",
              "Joe Bloggs/Third Album/02:Second_track.ogg"]
    third = ["Joe Bloggs/First Album/03:ThI\xCC\x81rd track.ogg",
             "Joe Bloggs/Second Album/03:Third track.ogg",
             "Joe Bloggs/Third Album/01:First_track.ogg",
             "Joe Bloggs/Third Album/02:Second_track.ogg",
             "Joe Bloggs/Third Album/03:Third_track.ogg",
             "Joe Bloggs/Third Album/04:Fourth_track.ogg",
             "Joe Bloggs/Third Album/05:Fifth_track.ogg"]
    first_and_second = filter(lambda s: s in second, first)
    # ASCII matches
    check_search_results(["first"], first)
    check_search_results(["Second"], second)
    check_search_results(["THIRD"], third)
    # ASCII Conjunctions
    check_search_results(["FIRST", "SECOND"], first_and_second)
    # Non-ASCII Characters
    # 00CC is LATIN CAPITAL LETTER I WITH GRAVE
    # 00EC is LATIN SMALL LETTER I WITH GRAVE
    check_search_results([u"F\u00CCRST"], first)
    check_search_results([u"f\u00ECrst"], first)
    # 00CD is LATIN CAPITAL LETTER I WITH ACUTE
    # 00ED is LATIN SMALL LETTER I WITH ACUTE
    check_search_results([u"TH\u00CDRD"], third)
    check_search_results([u"th\u00EDrd"], third)
    # ...and again in denormalized form
    # 0300 is COMBINING GRAVE ACCENT
    # 0301 is COMBINING ACUTE ACCENT
    check_search_results([u"FI\u0300RST"], first)
    check_search_results([u"fi\u0300rst"], first)
    check_search_results([u"THI\u0301RD"], third)
    check_search_results([u"thI\u0301rd"], third)
    # stopwords shouldn't show up
    check_search_results(["01"], [])
    
    if failures > 0:
        sys.exit(1)

if __name__ == '__main__':
    dtest.run()
