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
import maya.mel as mel
import os
import unittest
import logging
from utils import *

class test_camera(unittest.TestCase):
    def setUp(self):
        setUpTest()
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
        tearDownTest()
                
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
    
    def createCameraForTest(self):
        '''
        create a new Aim and Up camera for test
        test should delete cameraNode at end of test
        '''
        # set new scene
        cmds.file(new = True, force = True)
        
        # create camera with aim and up
        cmds.select(d=True)
        cameraNode = cmds.camera()
        cameraRoot = cameraNode[0]
        cameraShape = cameraNode[1]
        mel.eval("cameraMakeNode 3 \"\";")
        selectionList = cmds.ls(selection=True)
        cameraUp = selectionList[1]
        cameraAim = selectionList[2]
        return cameraNode,cameraRoot, cameraShape, cameraUp, cameraAim       

    def exportAndGetJsonData(self, objectName):
        '''
        export subject to json file
        get staticData, frameData from json
        '''
        staticDataFilePath, frameData0FilePath = self.ExportJsonData(objectName)
        staticData = getJsonCameraData(staticDataFilePath, objectName, True)
        self.assertTrue(staticData, msg="Could not get staticData from json")
        frameData = getJsonCameraData(frameData0FilePath, objectName, False)
        self.assertTrue(frameData, msg="Could not get frameData from json")   
        return staticData, frameData   

    def test_cameraDoF(self):
        log = logging.getLogger( "test_camera.test_cameraDoF" )
        log.info("Started")
        # create camera for test
        cameraNode, cameraRoot, cameraShape, cameraUp, cameraAim = self.createCameraForTest()

        # Make sure the depth-of-field is disabled
        cmds.camera(cameraShape, e=True, dof=False)
        dofEnabled = cmds.camera(cameraShape, q=True, dof=True)
        self.assertFalse(dofEnabled, msg="Depth of Field not set False on camera")

        # Export to JSON
        staticData, frameData = self.exportAndGetJsonData(cameraRoot)

        # Focus distance shouldn't be part of the static/frame data when depth-of-field is disabled
        self.assertFalse('FocusDistance' in staticData, msg="Focus distance found in staticData but should not")
        self.assertFalse('FocusDistance' in frameData, msg="Focus distance found in frameData but should not")

        # Change the focus distance
        requestedFocusDistance = 2
        cmds.camera(cameraShape, e=True, fd=requestedFocusDistance)
        focusDist = cmds.camera(cameraShape, q=True, fd=True)
        self.assertEqual(focusDist, requestedFocusDistance, msg="Focus distance not set on camera")

        # Enable depth-of-field
        cmds.camera(cameraShape, e=True, dof=True)
        dofEnabled = cmds.camera(cameraShape, q=True, dof=True)
        self.assertTrue(dofEnabled, msg="Depth of Feild not set True on camera")

        # Re-export to JSON with the new values
        staticData, frameData = self.exportAndGetJsonData(cameraRoot)

        # Focus distance must be part of the static/frame data when depth-of-field is enabled and has expected value
        self.assertTrue('FocusDistance' in staticData, msg="Focus distance not found in staticData")
        self.assertTrue('FocusDistance' in frameData, msg="Focus distance not found in frameData")
        self.assertEqual(frameData['FocusDistance'], requestedFocusDistance, msg="Focus distance not of expected value in frameData")
    
        # delete camera, aim and up nodes
        cmds.delete(cameraNode, cameraUp, cameraAim)
        log.info("Completed")

    def test_cameraFoV(self):
        log = logging.getLogger( "test_camera.test_cameraFoV" )
        log.info("Started")
        # create camera for test
        cameraNode, cameraRoot, cameraShape, cameraUp, cameraAim = self.createCameraForTest()
        
        # set field of view (angle of view in Maya, horizontal field of view hfv in code)
        requestedFoV = 45.0
        cmds.camera(cameraShape, e=True, hfv=requestedFoV)
        self.assertAlmostEqual(cmds.camera(cameraShape, q=True, hfv=True), requestedFoV, msg="Field of View not set on camera")
        
        # Export to JSON
        staticData, frameData = self.exportAndGetJsonData(cameraRoot)
        
        # verify field of view is part of the static/frame data and has expected value
        self.assertTrue('FieldOfView' in staticData, msg="FieldOfView not found in staticData")
        self.assertTrue('FieldOfView' in frameData, msg="FieldOfView not found in frameData")
        self.assertEqual(frameData['FieldOfView'], requestedFoV, msg="FieldOfView not of expected value in frameData")
        
        # delete camera, aim and up nodes
        cmds.delete(cameraNode, cameraUp, cameraAim)
        log.info("Completed")
        
    def test_cameraAspectRatio(self):
        log = logging.getLogger( "test_camera.test_cameraAspectRatio" )
        log.info("Started")
        # create camera for test
        cameraNode, cameraRoot, cameraShape, cameraUp, cameraAim = self.createCameraForTest()
        
        # set Aspect Ration
        requestedAR = 1.33
        cmds.camera(cameraShape, e=True, ar=requestedAR)
        self.assertAlmostEqual(cmds.camera(cameraShape, q=True, ar=True), requestedAR, msg="Aspect Ratio not set on camera")
                
        # Export to JSON
        staticData, frameData = self.exportAndGetJsonData(cameraRoot)
        
        # verify Aspect Ration is part of the static/frame data and has expected value
        self.assertTrue('AspectRatio' in staticData, msg="Aspect Ratio not found in staticData")
        self.assertTrue('AspectRatio' in frameData, msg="Aspect Ratio not found in frameData")
        self.assertAlmostEqual(frameData['AspectRatio'], requestedAR, msg="Aspect Ratio not of expected value in frameData")
        
        # delete camera, aim and up nodes
        cmds.delete(cameraNode, cameraUp, cameraAim)
        log.info("Completed")
        
    def test_cameraFocalLength(self):
        log = logging.getLogger( "test_camera.test_cameraFocalLength" )
        log.info("Started")
        # create camera for test
        cameraNode, cameraRoot, cameraShape, cameraUp, cameraAim = self.createCameraForTest()
        
        # set FocalLength
        requestedFL = 33.33
        cmds.camera(cameraShape, e=True, fl=requestedFL)
        self.assertAlmostEqual(cmds.camera(cameraShape, q=True, fl=True), requestedFL, msg="Focal Length not set on camera")
        
        # Export to JSON
        staticData, frameData = self.exportAndGetJsonData(cameraRoot)
        
        # verify Focal Length is part of the static/frame data and has expected value
        self.assertTrue('FocalLength' in staticData, msg="Focal Length not found in staticData")
        self.assertTrue('FocalLength' in frameData, msg="Focal Length not found in frameData")
        self.assertAlmostEqual(frameData['FocalLength'], requestedFL, places=5, msg="Focal Length not of expected value in frameData")
        
        # delete camera, aim and up nodes
        cmds.delete(cameraNode, cameraUp, cameraAim)
        log.info("Completed")
        
    def test_cameraTranslationRotation(self):
        log = logging.getLogger( "test_camera.test_cameraTranslationRotation" )
        log.info("Started")
        # create camera for test
        cameraNode, cameraRoot, cameraShape, cameraUp, cameraAim = self.createCameraForTest()
        
        # set Camera translation
        requestedTranslationX = -2.5
        requestedTranslationY = -3
        requestedTranslationZ = 1.618
        cmds.move(requestedTranslationX, requestedTranslationY, requestedTranslationZ, cameraRoot, absolute=True )
        
        # set Up and Aim translation, which rotates the camera
        cmds.move(requestedTranslationX, -requestedTranslationY, requestedTranslationZ, cameraUp, absolute=True )
        cmds.move((-2*requestedTranslationX), 0, 0, cameraAim, absolute=True )
        
        # validate camera has been moved and rotated
        requestedRotationX = 21.3557282
        requestedRotationY = -77.8259654
        requestedRotationZ = 0
        self.assertAlmostEqual(cmds.getAttr(str(cameraRoot)+'.translateX'), requestedTranslationX, msg="Translation on X not set on camera")
        self.assertAlmostEqual(cmds.getAttr(str(cameraRoot)+'.translateY'), requestedTranslationY, msg="Translation on Y not set on camera")
        self.assertAlmostEqual(cmds.getAttr(str(cameraRoot)+'.translateZ'), requestedTranslationZ, msg="Translation on Z not set on camera")
        self.assertAlmostEqual(cmds.getAttr(str(cameraRoot)+'.rotateX'), requestedRotationX, msg="Rotation on X not set on camera")
        self.assertAlmostEqual(cmds.getAttr(str(cameraRoot)+'.rotateY'), requestedRotationY, msg="Rotation on Y not set on camera")
        self.assertAlmostEqual(cmds.getAttr(str(cameraRoot)+'.rotateZ'), requestedRotationZ, msg="Rotation on Z not set on camera")
        
        # Export to JSON
        staticData, frameData = self.exportAndGetJsonData(cameraRoot)
        
        # validate JSON transforms
        requestedTranslation = [-2.5,1.6180000305175782,-3.0]
        requestedRotation = [1.5595551e-06, -21.3557276, -12.1740377]
        validateTransform(requestedTranslation, requestedRotation, False, staticData, frameData, self)
        
        # delete camera, aim and up nodes
        cmds.delete(cameraNode, cameraUp, cameraAim)
        log.info("Completed")
        