#-*-python-*-

"""Utility module used by tests"""

import os,os.path,subprocess,sys,disorder

def copyfile(a,b):
    """copyfile(A, B)
Copy A to B."""
    open(b,"w").write(open(a).read())

def maketrack(s):
    """maketrack(S)

Make track with relative path S exist"""
    trackpath = "%s/%s" % (tracks, s)
    trackdir = os.path.dirname(trackpath)
    if not os.path.exists(trackdir):
        os.makedirs(trackdir)
    copyfile("%s/sounds/slap.ogg" % topsrcdir, trackpath)
    # We record the tracks we created so they can be tested against
    # server responses
    bits = s.split('/')
    dp = tracks
    for d in bits [0:-1]:
        dd = "%s/%s" % (dp,  d)
        if dp not in dirs_by_dir:
            dirs_by_dir[dp] = []
        if dd not in dirs_by_dir[dp]:
            dirs_by_dir[dp].append(dd)
        dp = "%s/%s" % (dp, d)
    if dp not in files_by_dir:
        files_by_dir[dp] = []
    files_by_dir[dp].append("%s/%s" % (dp, bits[-1]))

def stdtracks():
    # We create some tracks with non-ASCII characters in the name and
    # we (currently) force UTF-8.
    #
    # On a traditional UNIX filesystem, that treats filenames as byte strings
    # with special significant for '/', this should just work, though the
    # names will look wrong to ls(1) in a non UTF-8 locale.
    #
    # On Apple HFS+ filenames normalized to a decomposed form that isn't quite
    # NFD, so our attempts to have both normalized and denormalized filenames
    # is frustrated.  Provided we test on traditional filesytsems too this
    # shouldn't be a problem.
    # (See http://developer.apple.com/qa/qa2001/qa1173.html)

    global dirs_by_dir, files_by_dir
    dirs_by_dir={}
    files_by_dir={}
    
    # C3 8C = 00CC LATIN CAPITAL LETTER I WITH GRAVE
    # (in NFC)
    maketrack("Joe Bloggs/First Album/01:F\xC3\x8Crst track.ogg")

    maketrack("Joe Bloggs/First Album/02:Second track.ogg")

    # CC 81 = 0301 COMBINING ACUTE ACCENT
    # (giving an NFD i-acute)
    maketrack("Joe Bloggs/First Album/03:ThI\xCC\x81rd track.ogg")
    # ...hopefuly giving C3 8D = 00CD LATIN CAPITAL LETTER I WITH ACUTE
    maketrack("Joe Bloggs/First Album/04:Fourth track.ogg")
    maketrack("Joe Bloggs/First Album/05:Fifth track.ogg")
    maketrack("Joe Bloggs/Second Album/01:First track.ogg")
    maketrack("Joe Bloggs/Second Album/02:Second track.ogg")
    maketrack("Joe Bloggs/Second Album/03:Third track.ogg")
    maketrack("Joe Bloggs/Second Album/04:Fourth track.ogg")
    maketrack("Joe Bloggs/Second Album/05:Fifth track.ogg")
    maketrack("Joe Bloggs/Third Album/01:First track.ogg")
    maketrack("Joe Bloggs/Third Album/02:Second track.ogg")
    maketrack("Joe Bloggs/Third Album/03:Third track.ogg")
    maketrack("Joe Bloggs/Third Album/04:Fourth track.ogg")
    maketrack("Joe Bloggs/Third Album/05:Fifth track.ogg")
    maketrack("Fred Smith/Boring/01:Dull.ogg")
    maketrack("Fred Smith/Boring/02:Tedious.ogg")
    maketrack("Fred Smith/Boring/03:Drum Solo.ogg")
    maketrack("Fred Smith/Boring/04:Yawn.ogg")
    maketrack("misc/blahblahblah.ogg")
    maketrack("Various/Greatest Hits/01:Jim Whatever - Spong.ogg")
    maketrack("Various/Greatest Hits/02:Joe Bloggs - Yadda.ogg")

def notracks():
    pass

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
plugins ../plugins
player *.mp3 execraw disorder-decode
player *.ogg execraw disorder-decode
player *.wav execraw disorder-decode
player *.flac execraw disorder-decode
tracklength *.mp3 disorder-tracklength
tracklength *.ogg disorder-tracklength
tracklength *.wav disorder-tracklength
tracklength *.flac disorder-tracklength
""" % (testroot, testroot, testroot, testroot))
    copyfile("%s/sounds/scratch.ogg" % topsrcdir,
             "%s/scratch.ogg" % testroot)

def start_daemon(test):
    """start_daemon(TEST)
Start the daemon for test called TEST."""
    global daemon
    assert daemon == None
    if test == None:
        errs = sys.stderr
    else:
        errs = open("%s.log" % test, "w")
    server = None
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
    rc = daemon.poll()
    if rc == None:
        print " stopping daemon"
        os.kill(daemon.pid, 15)
        rc = daemon.wait()
    print " daemon has stopped"
    daemon = None

def run(test, setup=None, report=True, name=None): 
    global tests
    tests += 1
    if setup == None:
        setup = stdtracks
    common_setup()
    setup()
    start_daemon(name)
    try:
        try:
            test()
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
testroot = "%s/testroot" % os.getcwd()
tracks = "%s/tracks" % testroot
topsrcdir = os.path.abspath(os.getenv("topsrcdir"))
