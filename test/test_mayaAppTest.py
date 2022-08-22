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

import maya.cmds as cmds
import unittest
import logging
from utils import *

class test_mayaAppTest(unittest.TestCase):
    def setUp(self):
        setUpTest()

    def tearDown(self):
        tearDownTest()

    def test_polyAnimation(self):
        """
        Test basic animation
        """
        log = logging.getLogger( "test_mayaAppTest.test_polyAnimation" )
        log.info("Started")
        cmds.file(new=True, force=True)
        cmds.polySphere(n='testSphere')
        cmds.setKeyframe('testSphere', attribute='translateX', v=5, t=['0'])
        cmds.setKeyframe('testSphere', attribute='translateX', v=10, t=['5'])
        cmds.setKeyframe('testSphere', attribute='translateX', v=15, t=['10'])
        cmds.currentTime(5)
        self.assertEqual(10, cmds.getAttr('testSphere.translateX'))
        log.info("Completed")


    
    def test_emptyBaseAnimation(self):
        """
        Delete the animLayer node and check the children of "BaseAnimation"
        """
        log = logging.getLogger( "test_mayaAppTest.test_emptyBaseAnimation" )
        log.info("Started")
        cmds.file(new=True, force=True)
        cmds.animLayer("animLayer1")
        cmds.delete("animLayer1")
        cmds.animLayer("BaseAnimation", edit=True, moveLayerAfter="BaseAnimation")
        cmds.animLayer("BaseAnimation", edit=True, moveLayerBefore="BaseAnimation")
        self.assertEqual(None, cmds.animLayer("BaseAnimation", query=True, children=True))
        log.info("Completed")