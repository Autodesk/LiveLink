
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

nameRootSkeleton = 'joint1'

class test_customAttributes(unittest.TestCase):
    nodesWithCustomAttrs = 0

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
    

    def ExportJsonData(self):
        # Export Json
        exportJson(nameRootSkeleton, streamType="Full Hierarchy")

        # Check json files were exported properly and add the file path to self.__files so we can delete them in tearDown function.
        staticDataFilePath = expandFileName(getFileNameForStaticData(nameRootSkeleton))
        frameData0FilePath = expandFileName(getFileNameForFrameData(nameRootSkeleton, 0))

        self.__files.append(expandFileName(staticDataFilePath))
        self.__files.append(expandFileName(frameData0FilePath))

        self.assertTrue(os.path.isfile(staticDataFilePath), msg="Cannot find static data json file.")
        self.assertTrue(os.path.isfile(frameData0FilePath), msg="Cannot find frame 0 data json file.")

        return staticDataFilePath, frameData0FilePath

    def ValidateAnimData(self, Nodes):
        # Make sure we created all the required nodes
        self.assertEqual(self.nodesWithCustomAttrs, len(Nodes), msg="The number of nodes is not the same as the Nodes array size")

        staticDataFilePath, frameData0FilePath = self.ExportJsonData()

        # Get data exported from json files
        names = getJsonAnimCustomAttrData(staticDataFilePath, nameRootSkeleton, True)
        values = getJsonAnimCustomAttrData(frameData0FilePath, nameRootSkeleton, False)
        sceneTimeInSec = getJsonAnimSceneTimeInSeconds(frameData0FilePath, nameRootSkeleton, True)

        self.assertTrue(names is not None and values is not None, "There is no name or value in the JSON files")
        numberOfNames = len(names)
        numberOfValues = len(values)

        # Assert data are streamable and will not cause errors or issues in Unreal
        self.assertEqual(numberOfValues, numberOfNames, msg="The number of properties in the StaticData and the number of properties in the FrameData are not equals. Streaming data to Unreal will fail.")
        self.assertEqual(numberOfValues, self.nodesWithCustomAttrs, msg="The number of properties in the StaticData and the number of properties in the FrameData are not equals. Streaming data to Unreal will fail.")

        currentFrame = cmds.currentTime(query=True)
        # Current scene time is currentFrameNumber/frameRate
        sceneTime = currentFrame/30
        # Assert that time in seconds in correct
        self.assertAlmostEqual(sceneTime, sceneTimeInSec, places=4, msg="The Scene time being sent to Unreal is incorrect")

        # Assert value streamed
        numberOfValidNames = 0
        for idx in range(min(numberOfNames, numberOfValues)):
            name = names[idx]
            value = values[idx]
            node = Nodes[idx]

            self.assertTrue (name == node['customAttrName'])
            self.assertAlmostEqual(value, node['customAttrVal'])
            numberOfValidNames += 1

        # Assert the values that are supposed to be stream are streamed.
        self.assertEqual(numberOfValidNames, numberOfNames, msg="The bone parent/child hierarchy is wrong.")

        # Delete the objects
        cmds.delete(Nodes[0]['name'])

    def test_jointsWithCustomAttr(self):
        log = logging.getLogger( "test_customAttributes.test_jointsWithCustomAttr" )
        log.info("Started")
        cmds.select( d=True )
        self.nodesWithCustomAttrs = 0

        Nodes = [
        {
            'name' : 'joint1',
            'trans': [0, 0, 0],
            'customAttrName': 'customAttr1',
            'customAttrVal': 7.0
        },
        {
            'name' : 'joint2',
            'trans': [2, 0, 0],
            'customAttrName': 'customAttr2',
            'customAttrVal': 15.0
        }]

        # Create the skeleton joints
        parentNode = Nodes[self.nodesWithCustomAttrs]
        cmds.joint( p=parentNode['trans'] )
        cmds.joint(parentNode['name'], e=True, zso=True, oj='xyz', sao='yup')
        cmds.addAttr( longName='customAttr1', attributeType='double', defaultValue=7.0, keyable=True )
        self.nodesWithCustomAttrs += 1
    
        # joint2 is a child of joint1
        node = Nodes[self.nodesWithCustomAttrs]
        cmds.joint( p=node['trans'] )
        cmds.joint(node['name'], e=True, zso=True, oj='xyz', sao='yup')
        cmds.addAttr( longName='customAttr2', attributeType='double', defaultValue=15.0, keyable=True )
        self.nodesWithCustomAttrs += 1

        self.ValidateAnimData(Nodes)
        log.info("Completed")

    def test_propWithCustomAttr(self):
        log = logging.getLogger( "test_customAttributes.test_propWithCustomAttr" )
        log.info("Started")
        cmds.select( d=True )
        self.nodesWithCustomAttrs = 0

        Nodes = [
        {
            'name' : 'joint1',
            'trans': [0, 0, 0],
            'customAttrName': 'customAttr3',
            'customAttrVal': 25.0
        }]

        # Create the prop with joint1 name and stream with full hierarchy
        parentNode = Nodes[self.nodesWithCustomAttrs]
        cmds.polyCone(name = parentNode['name'])
        cmds.addAttr( longName='customAttr3', attributeType='double', defaultValue=25.0, keyable=True )
        self.nodesWithCustomAttrs += 1

        self.ValidateAnimData(Nodes)
        log.info("Completed")