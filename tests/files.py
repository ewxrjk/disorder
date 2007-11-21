#! /usr/bin/env python
import dtest,time,disorder,sys

def test():
    """Check that the file listing comes out right"""
    dtest.start_daemon()
    time.sleep(5)                       # give rescan a chance
    c = disorder.client()
    failures = 0
    for d in dtest.dirs_by_dir:
        xdirs = dtest.dirs_by_dir[d]
        dirs = c.directories(d)
        xdirs.sort()
        dirs.sort()
        if dirs != xdirs:
            print
            print "directory: %s" % d
            print "expected:  %s" % xdirs
            print "got:       %s" % dirs
            failures += 1
    for d in dtest.files_by_dir:
        xfiles = dtest.files_by_dir[d]
        files = c.files(d)
        xfiles.sort()
        files.sort()
        if files != xfiles:
            print
            print "directory: %s" % d
            print "expected:  %s" % xfiles
            print "got:       %s" % files
            failures += 1
    if failures:
        print
        sys.exit(1)

if __name__ == '__main__':
    dtest.run(test)
