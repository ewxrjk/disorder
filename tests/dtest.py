#-*-python-*-
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

"""Utility module used by tests"""

import os,os.path,subprocess,sys,re

def fatal(s):
    """Write an error message and exit"""
    sys.stderr.write("ERROR: %s\n" % s)
    sys.exit(1)

# Identify the top build directory
cwd = os.getcwd()
if os.path.exists("config.h"):
    top_builddir = cwd
elif os.path.exists("alltests"):
    top_builddir = os.path.dirname(cwd)
else:
    fatal("cannot identify build directory")

# Make sure the Python build directory is on the module search path
sys.path.insert(0, os.path.join(top_builddir, "python"))
import disorder

# Make sure the server build directory is on the executable search path
ospath = os.environ["PATH"].split(os.pathsep)
ospath.insert(0, os.path.join(top_builddir, "server"))
os.environ["PATH"] = os.pathsep.join(ospath)

# Parse the makefile in the current directory to identify the source directory
top_srcdir = None
for l in file("Makefile"):
    r = re.match("top_srcdir *= *(.*)",  l)
    if r:
        top_srcdir = r.group(1)
        break
if not top_srcdir:
    fatal("cannot identify source directory")

# The tests source directory must be on the module search path already since
# we found dtest.py

# -----------------------------------------------------------------------------

def copyfile(a,b):
    """copyfile(A, B)
Copy A to B."""
    open(b,"w").write(open(a).read())

def maketrack(s):
    """maketrack(S)

Make track with relative path S exist"""
    trackpath = "%s/tracks/%s" % (testroot, s)
    trackdir = os.path.dirname(trackpath)
    if not os.path.exists(trackdir):
        os.makedirs(trackdir)
    copyfile("%s/sounds/slap.ogg" % top_srcdir, trackpath)

def stdtracks():
    maketrack("Joe Bloggs/First Album/01:First track.ogg")
    maketrack("Joe Bloggs/First Album/02:Second track.ogg")
    maketrack("Joe Bloggs/First Album/03:Third track.ogg")
    maketrack("Joe Bloggs/First Album/04:Fourth track.ogg")
    maketrack("Joe Bloggs/First Album/05:Fifth track.ogg")
    maketrack("Joe Bloggs/First Album/05:Fifth track.ogg")
    maketrack("Joe Bloggs/Second Album/01:First track.ogg")
    maketrack("Joe Bloggs/Second Album/02:Second track.ogg")
    maketrack("Joe Bloggs/Second Album/03:Third track.ogg")
    maketrack("Joe Bloggs/Second Album/04:Fourth track.ogg")
    maketrack("Joe Bloggs/Second Album/05:Fifth track.ogg")
    maketrack("Joe Bloggs/Second Album/05:Fifth track.ogg")
    maketrack("Joe Bloggs/First Album/01:First track.ogg")
    maketrack("Joe Bloggs/First Album/02:Second track.ogg")
    maketrack("Joe Bloggs/First Album/03:Third track.ogg")
    maketrack("Joe Bloggs/First Album/04:Fourth track.ogg")
    maketrack("Joe Bloggs/First Album/05:Fifth track.ogg")
    maketrack("Fred Smith/Boring/01:Dull.ogg")
    maketrack("Fred Smith/Boring/02:Tedious.ogg")
    maketrack("Fred Smith/Boring/03:Drum Solo.ogg")
    maketrack("Fred Smith/Boring/04:Yawn.ogg")
    maketrack("misc/blahblahblah.ogg")
    maketrack("Various/Greatest Hits/01:Jim Whatever - Spong.ogg")
    maketrack("Various/Greatest Hits/02:Joe Bloggs - Yadda.ogg")

def common_setup():
    remove_dir(testroot)
    os.mkdir(testroot)
    open("%s/config" % testroot, "w").write(
    """player *.ogg shell 'echo "$TRACK" >> %s/played.log'
home %s
collection fs UTF-8 %s/tracks
scratch %s/scratch.ogg
gap 0
stopword 01 02 03 04 05 06 07 08 09 10
stopword 1 2 3 4 5 6 7 8 9
stopword 11 12 13 14 15 16 17 18 19 20
stopword 21 22 23 24 25 26 27 28 29 30
stopword the a an and to too in on of we i am as im for is
username fred
password fredpass
allow fred fredpass
plugins %s/plugins
player *.mp3 execraw disorder-decode
player *.ogg execraw disorder-decode
player *.wav execraw disorder-decode
player *.flac execraw disorder-decode
tracklength *.mp3 disorder-tracklength
tracklength *.ogg disorder-tracklength
tracklength *.wav disorder-tracklength
tracklength *.flac disorder-tracklength
""" % (testroot, testroot, testroot, testroot, top_builddir))
    copyfile("%s/sounds/scratch.ogg" % top_srcdir,
             "%s/scratch.ogg" % testroot)

def start_daemon():
    """start_daemon()

Start the daemon."""
    global daemon
    assert daemon == None
    print " starting daemon"
    daemon = subprocess.Popen(["disorderd",
                               "--foreground",
                               "--config", "%s/config" % testroot],
                              stderr=errs)
    disorder._configfile = "%s/config" % testroot
    disorder._userconf = False

def stop_daemon():
    """stop_daemon()

Stop the daemon if it has not stopped already"""
    global daemon
    if daemon == None:
        return
    rc = daemon.poll()
    if rc == None:
        print " stopping daemon"
        os.kill(daemon.pid, 15)
        rc = daemon.wait()
    print " daemon has stopped"
    daemon = None

def run(module=None, report=True):
    """dtest.run(MODULE)

    Run the test in MODULE.  This can be a string (in which case the module
    will be imported) or a module object."""
    global tests
    tests += 1
    # Locate the test module
    if module is None:
        # We're running a test stand-alone
        import __main__
        module = __main__
        name = os.path.splitext(os.path.basename(sys.argv[0]))[0]
    else:
        # We've been passed a module or a module name
        if type(module) == str:
            module = __import__(module)
        name = module.__name__
    # Open the error log
    global errs
    errs = open("%s.log" % name, "w")
    # Ensure that disorder.py uses the test installation
    disorder._configfile = "%s/config" % testroot
    disorder._userconf = False
    # Make config file etc
    common_setup()
    # Create some standard tracks
    stdtracks()
    try:
        try:
            module.test()
        except AssertionError, e:
            global failures
            failures += 1
            print e
    finally:
        stop_daemon()
    if report:
        if failures:
            print " FAILED"
            sys.exit(1)
        else:
            print " OK"

def remove_dir(d):
    """remove_dir(D)

Recursively delete directory D"""
    if os.path.lexists(d):
        if os.path.isdir(d):
            for dd in os.listdir(d):
                remove_dir("%s/%s" % (d, dd))
            os.rmdir(d)
        else:
            os.remove(d)

# -----------------------------------------------------------------------------
# Common setup

tests = 0
failures = 0
daemon = None
testroot = "%s/tests/testroot" % top_builddir
tracks = "%s/tracks" % testroot
