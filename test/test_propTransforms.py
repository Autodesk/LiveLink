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

class test_propTransforms(unittest.TestCase):
    def setUp(self):
        cmds.file(new = True, force = True)
        # Set current time to be 30fps
        cmds.currentUnit( time='ntsc' )
        self.__files = []
        loadPlugins()

    def tearDown(self):
        cmds.file(new = True, force = True)
        for f in self.__files:
            if os.path.exists(f):
                os.remove(f)
        
    def ExportJsonData(self, objectName):
        # Export Json
        exportJson(objectName, startFrame=0, endFrame=0)
        
        # Check json files were exported properly and add the file path to self.__files so we can delete them in tearDown function.
        staticDataFilePath = expandFileName(getFileNameForStaticData(objectName))
        frameData0FilePath = expandFileName(getFileNameForFrameData(objectName, 0))
    
        self.__files.append(expandFileName(staticDataFilePath))
        self.__files.append(expandFileName(frameData0FilePath))
        
        self.assertTrue(os.path.isfile(staticDataFilePath), msg="Cannot find static data json file.")
        self.assertTrue(os.path.isfile(frameData0FilePath), msg="Cannot find frame 0 data json file.")
        
        return staticDataFilePath, frameData0FilePath
        
    def exportAndGetJsonData(self, objectName):
        '''
        export subject to json file
        get staticData, frameData from json
        '''
        staticDataFilePath, frameData0FilePath = self.ExportJsonData(objectName)
        staticData = getJsonPropData(staticDataFilePath, objectName, True)
        self.assertTrue(staticData, msg="Could not get staticData from json")
        frameData = getJsonPropData(frameData0FilePath, objectName, False)
        self.assertTrue(frameData, msg="Could not get frameData from json")   
        return staticData, frameData   
       
    def createProp(self):
        # set new scene
        cmds.file(new = True, force = True)
        
        cmds.select(d=True)
        propNode = cmds.polyCube()
        propRoot = propNode[0]
        
        return propNode, propRoot
        
    def test_propTransfroms(self):
        log = logging.getLogger( "test_propTransfroms" )
        log.info("Started")

        # create node for test
        propNode, propRoot = self.createProp()
        
        # set translation
        translationX = 2.718
        translationY = 3.14
        translationZ = 1.618
        cmds.move(translationX, translationY, translationZ, propRoot, absolute=True )
        
        # validate translation
        self.assertAlmostEqual(cmds.getAttr(str(propRoot)+'.translateX'), translationX, msg="Translation on X not set on propRoot")
        self.assertAlmostEqual(cmds.getAttr(str(propRoot)+'.translateY'), translationY, msg="Translation on Y not set on propRoot")
        self.assertAlmostEqual(cmds.getAttr(str(propRoot)+'.translateZ'), translationZ, msg="Translation on Z not set on propRoot")
        
        # set rotation
        rotationX = 21
        rotationY = 34
        rotationZ = 55
        cmds.rotate(rotationX, rotationY, rotationZ, propRoot, absolute=True )
        
        # validate rotation
        self.assertAlmostEqual(cmds.getAttr(str(propRoot)+'.rotateX'), rotationX, msg="Rotation on X not set on propRoot")
        self.assertAlmostEqual(cmds.getAttr(str(propRoot)+'.rotateY'), rotationY, msg="Rotation on Y not set on propRoot")
        self.assertAlmostEqual(cmds.getAttr(str(propRoot)+'.rotateZ'), rotationZ, msg="Rotation on Z not set on propRoot")

        # set scale
        scaleX = 1.3247
        scaleY = 0.955
        scaleZ = 1.442
        cmds.scale(scaleX, scaleY, scaleZ, propRoot, absolute=True )
        
        # validate scale
        self.assertAlmostEqual(cmds.getAttr(str(propRoot)+'.scaleX'), scaleX, msg="Scale on X not set on propRoot")
        self.assertAlmostEqual(cmds.getAttr(str(propRoot)+'.scaleY'), scaleY, msg="Scale on Y not set on propRoot")
        self.assertAlmostEqual(cmds.getAttr(str(propRoot)+'.scaleZ'), scaleZ, msg="Scale on Z not set on propRoot")
        
        # Export to JSON
        staticData, frameData = self.exportAndGetJsonData(propRoot)
        
        # validate JSON transforms
        expectedTranslation = [translationX, translationZ, translationY]
        expectedRotation = [17.611226, -42.773963, -49.623420]
        expectedScale = [scaleX, scaleZ, scaleY]
        validateTransform(expectedTranslation, expectedRotation, True, staticData, frameData, self, expectedScale)
        
        # delete test node
        cmds.delete(propNode, propRoot)
        log.info("Completed")