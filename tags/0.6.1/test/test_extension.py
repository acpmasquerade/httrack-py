#!/usr/bin/python
""" simple test and example for the usage of the httrack Python extension
"""

import sys, os
# the next line is only required for tests. Normally, the httrack 
# extension module should be installed in the Python site-packages directory 
# which is in the default search path
sys.path.append(".")

# httracklib defines the Python function httrack, which start
# the httrack engine
import httracklib

# I am lazy: let's use the same callback class as in the plugin example
import httrack

# don't check for errors -- simply bail out if the directory exists
# This is only intended for tests...
os.mkdir("httrack-test")
os.chdir("httrack-test")
callback = httrack.Htcb()

# we simply specify the command line arguments as the second parameter
# the httrack engine expects at least one parameter, the program name
httracklib.httrack(callback, sys.argv)

