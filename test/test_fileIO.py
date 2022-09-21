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
import os
import unittest
import logging
from utils import *

# setting Max File I/O time in seconds for all Save, New, Open and Import file commands
maxIOTime = 0.1

class test_fileIO(unittest.TestCase):
    '''
    Tests Maya File > New performance
    related to defect MAYA-124552 where New File took more than 5 secs
    ''' 
    def setUp(self):
        setUpTest()
        cmds.file(new = True, force = True)
        self.__files = []
        loadPlugins()
        
        # create test data
        cmds.sphere()
        self.__files.append(cmds.internalVar(userTmpDir=True) + 'test_fileIO_file.ma')
        cmds.file(rename=self.__files[0])

    def tearDown(self):
        cmds.file(new = True, force = True)
        for f in self.__files:
            if os.path.exists(f):
                os.remove(f)
        tearDownTest()
    
    def test_fileNewOpen(self):
        '''
        timer test for Maya File Save while Live Link Plugin is enabled.
        create a sphere to save as .ma for File Load and File Import tests
        '''
        log = logging.getLogger( "test_fileIO.test_fileNewOpen" )
        log.info("Started")
        
        # save file 
        cmds.timer(s=True, n="save")
        cmds.file(save=True, type='mayaAscii')
        fileSaveTime=(cmds.timer( e=True, n="save" ))
        
        # validate test Scene and Node are created and time it took to Save file
        self.assertLessEqual(fileSaveTime, maxIOTime, msg="Save File took " + str(fileSaveTime) + " secs but expected below " + str(maxIOTime))
        self.assertEqual(1, len(cmds.ls(g=True)), msg="Not expected Node count in current scene" )
        self.assertTrue(os.path.exists(self.__files[0]), msg="Test file not created as expected")
        
        # new file
        cmds.timer(s=True, n="new")
        cmds.file(new=True, force=True)
        fileNewTime=(cmds.timer(e=True, n="new"))
        
        # validate File New time
        self.assertLessEqual(fileNewTime, maxIOTime, msg="New File took " + str(fileNewTime) + " secs but expected below " + str(maxIOTime))
    
        # open file
        cmds.timer(s=True, n="open")
        cmds.file(self.__files[0], open=True)
        fileOpenTime=(cmds.timer(e=True, n="open"))

        # validate File Open time
        self.assertLessEqual(fileOpenTime, maxIOTime, msg="Open File took " + str(fileOpenTime) + " secs but expected below " + str(maxIOTime))       
        
        # import file
        cmds.timer(s=True, n="import")
        cmds.file(self.__files[0], i=True, type='mayaAscii')
        fileImportTime=(cmds.timer(e=True, n="import"))
        
        # validate File Import time
        self.assertLessEqual(fileImportTime, maxIOTime, msg="Load File took " + str(fileImportTime) + " secs but expected below " + str(maxIOTime))
        
        log.info("Completed")
        