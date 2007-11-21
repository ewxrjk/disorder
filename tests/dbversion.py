#! /usr/bin/env python
import dtest,time,disorder,sys,re

def test():
    """Database version tests"""
    # Start up with dbversion 1
    config = "%s/config" % dtest.testroot
    configsave = "%s.save" % config
    dtest.copyfile(config, configsave)
    open(config, "a").write("dbversion 1\n")
    dtest.start_daemon()
    time.sleep(2)
    dtest.stop_daemon()
    # Revert to default configuration
    dtest.copyfile(configsave, config)
    dtest.start_daemon()
    time.sleep(2)
    c = disorder.client()
    try:
        v = c.version()
        print "unexpected success"
        ok = False
    except disorder.communicationError, e:
        if re.search("connection refused", str(e)):
            print "unexpected error: %s" % e
            ok = False
        else:
            ok = True
    dtest.stop_daemon()
    if not ok:
        sys.exit(1)

if __name__ == '__main__':
    dtest.run(test)
