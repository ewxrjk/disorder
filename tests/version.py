#! /usr/bin/env python
import dtest,time,disorder

def test():
    """Ask the server its version number"""
    time.sleep(2)                       # give the daemon a chance to start up
    c = disorder.client()
    v = c.version()
    print "Server version: %s" % v

if __name__ == '__main__':
    dtest.run(test)
