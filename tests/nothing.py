#! /usr/bin/env python
import dtest,time

def test():
    """Just start the server and then stop it a few seconds later"""
    dtest.start_daemon()
    time.sleep(2)

if __name__ == '__main__':
    dtest.run(test)
