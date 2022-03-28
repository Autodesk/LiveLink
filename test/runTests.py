# MIT License

# Copyright (c) 2022 Autodesk, Inc.

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""
This script runs python unit tests in the test directory.
Usage: mayapy runTests.py
"""
import unittest
import sys

sys.dont_write_bytecode = True

import maya.standalone
maya.standalone.initialize(name='python')

"""
Use unitTestloader to dicover the test cases in current directory
and add it to a test suite.
"""
def discoverTestSuite():
    loader = unittest.TestLoader()
    testSuite = loader.discover(start_dir='./')
    return testSuite

"""
Discover and run unit tests.
"""
if __name__ == '__main__':
    runner = unittest.TextTestRunner()
    result = runner.run(discoverTestSuite())
    sys.exit(not result.wasSuccessful())

maya.standalone.uninitialize()