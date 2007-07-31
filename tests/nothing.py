#! /usr/bin/env python
import dtest,time

def test():
    """Just start the server and then stop it a few seconds later"""
    time.sleep(5)

if __name__ == '__main__':
    dtest.run(test)
