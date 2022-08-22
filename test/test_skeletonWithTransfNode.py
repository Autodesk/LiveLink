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

nameRootSkeleton = 'Root'

class test_skeletonHierarchy(unittest.TestCase):
    numberOfNodes = 0

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

    def ExportJsonData(self):
        # Export Json
        exportJson(nameRootSkeleton)

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
        self.assertEqual(self.numberOfNodes, len(Nodes), msg="The number of nodes is not the same as the Nodes array size")

        staticDataFilePath, frameData0FilePath = self.ExportJsonData()

        # Get data exported from json files
        names = getJsonAnimData(staticDataFilePath, nameRootSkeleton, True)
        values = getJsonAnimData(frameData0FilePath, nameRootSkeleton, False)
        sceneTimeInSec = getJsonAnimSceneTimeInSeconds(frameData0FilePath, nameRootSkeleton)

        self.assertTrue(names is not None and values is not None, "There is no name or value in the JSON files")
        numberOfNames = len(names)
        numberOfValues = len(values)

        # Assert data are streamable and will not cause errors or issues in Unreal
        self.assertEqual(numberOfValues, numberOfNames, msg="The number of properties in the StaticData and the number of properties in the FrameData are not equals. Streaming data to Unreal will fail.")
        self.assertEqual(numberOfValues, self.numberOfNodes, msg="The number of properties in the StaticData and the number of properties in the FrameData are not equals. Streaming data to Unreal will fail.")

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

            #self.assertEqual(name['Name'], node['name'])
            #validateVectorAttribute('L', node['expectedTrans'], name, value, self)
            #self.assertTrue(almostEqual(value['R'], node['expectedRot']))
            validateBoneTransform(idx, node['expectedTrans'], node['expectedRot'], values, self)
            numberOfValidNames += 1

        # Assert the values that are supposed to be stream are streamed.
        self.assertEqual(numberOfValidNames, numberOfNames, msg="The bone parent/child hierarchy is wrong.")

        # Delete the objects
        cmds.delete(Nodes[0]['name'])
        cmds.delete('baseObject')

    def ValidatePropData(self, NodeNames):
        # Make sure we created all the required nodes
        self.assertEqual(self.numberOfNodes, len(NodeNames), msg="The number of nodes is not the same as the NodeNames array size")

        staticDataFilePath, frameData0FilePath = self.ExportJsonData()

        # Get data exported from json files
        names = getJsonPropData(staticDataFilePath, nameRootSkeleton, True)
        values = getJsonPropData(frameData0FilePath, nameRootSkeleton, False)

        self.assertTrue(names is not None and values is not None, "There is no name or value in the JSON files")
        numberOfNames = len(names)
        numberOfValues = len(values)

        # Assert data are streamable and will not cause errors or issues in Unreal
        self.assertEqual(numberOfValues, numberOfNames, msg="The number of properties in the StaticData and the number of properties in the FrameData are not equals. Streaming data to Unreal will fail.")

        # Assert value streamed
        self.assertTrue('Location' in names and 'L' in values and \
                        'Rotation' in names and 'R' in values and \
                        'Scale' in names and 'S' in values, msg="The transform data is not in the expected format");

        # Delete the objects
        cmds.delete(NodeNames)

    def test_skeletonWithTransfNode(self):
        log = logging.getLogger( "test_skeletonHierarchy.test_skeletonWithTransfNode" )
        log.info("Started")
        cmds.select( d=True )
        self.numberOfNodes = 0

        Nodes = [
        {
            'name' : nameRootSkeleton,
            'trans': [3, 0, 0],
            'rot'  : [0, 0, 0],
            'expectedTrans': [3, 0, 0],
            'expectedRot'  : [-0.707106769, 0.0, 0.0, 0.707106769]
        },
        {
            'name' : 'Reference',
            'trans': [0, 1, 0],
            'rot'  : [0, 90, 0],
            'expectedTrans': [0, -1, 0],
            'expectedRot'  : [0.0, 0.707106769, 0.0, 0.707106769]
        },
        {
            'name' : 'joint1',
            'trans': [0, 0, 0],
            'rot'  : [90, 0, 0],
            'expectedTrans': [0, 1, -3],
            'expectedRot'  : [0.0, 0.0, -0.707106769, 0.707106769]
        },
        {
            'name' : 'joint2',
            'trans': [2, 0, 0],
            'rot'  : [0, 0, 90],
            'expectedTrans': [2, 0, 0],
            'expectedRot'  : [0.0, 0.707106769, 0.0, 0.707106769]
        }]

        # Create a root transform node
        node = Nodes[self.numberOfNodes]
        cmds.createNode( 'transform', n=node['name'] )
        trans = node['trans']
        cmds.move(trans[0], trans[1], trans[2], node['name'], r=True)
        self.numberOfNodes += 1
        parentNode = node

        # Create another transform node
        node = Nodes[self.numberOfNodes]
        cmds.createNode( 'transform', n=node['name'], p=parentNode['name'] )
        trans = node['trans']
        cmds.move(trans[0], trans[1], trans[2], node['name'], r=True)
        rot = node['rot']
        cmds.rotate(rot[0], rot[1], rot[2], node['name'], r=True)
        self.numberOfNodes += 1

        # Create the skeleton joints
        parentNode = Nodes[self.numberOfNodes]
        self.numberOfNodes += 1
        node = Nodes[self.numberOfNodes]
        parentTrans = parentNode['trans']
        cmds.joint( p=(parentTrans[0],parentTrans[1],parentTrans[2]) )
        cmds.rename(parentNode['name'])
        trans = node['trans']
        cmds.joint( p=(trans[0],trans[1],trans[2]) )
        cmds.joint(parentNode['name'], e=True, zso=True, oj='xyz', sao='yup')
        cmds.joint(parentNode['name'], e=True, o=parentNode['rot'])

        # joint2 is a child of joint1
        cmds.rename(node['name'])
        cmds.joint(node['name'], e=True, zso=True, oj='xyz', sao='yup')
        cmds.joint(parentNode['name'], e=True, o=node['rot'])
        self.numberOfNodes += 1

        # Create a cube baseObject, add subdivision and put it in position
        cmds.polyCube( n='baseObject',w=6, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
        cmds.move(3,0,0, 'baseObject', r=True)
        cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)
        
        # Add a skin cluster between the skeleton and baseObject
        cmds.skinCluster(parentNode['name'], node['name'], 'baseObject',tsb=True)

        self.ValidateAnimData(Nodes)
        log.info("Completed")

    def test_JointOnly(self):
        log = logging.getLogger( "test_skeletonHierarchy.test_JointOnly" )
        log.info("Started")
        cmds.select( d=True )
        self.numberOfNodes = 0

        Nodes = [
        {
            'name' : nameRootSkeleton,
            'trans': [3, 0, 0],
            'rot'  : [0, 0, 0],
            'expectedTrans': [3, 0, 0],
            'expectedRot'  : [-0.5, 0.5, -0.5, 0.5]
        },
        {
            'name' : 'Reference',
            'trans': [0, 1, 0],
            'rot'  : [0, 90, 0],
            'expectedTrans': [3.1622777, 0, 0],
            'expectedRot'  : [-0.1601822, 0.9870874, 0, 0]
        }]

        # Create the skeleton joints
        parentNode = Nodes[self.numberOfNodes]
        self.numberOfNodes += 1
        node = Nodes[self.numberOfNodes]
        parentTrans = parentNode['trans']
        cmds.joint( p=(parentTrans[0],parentTrans[1],parentTrans[2]) )
        cmds.rename(parentNode['name'])
        trans = node['trans']
        cmds.joint( p=(trans[0],trans[1],trans[2]) )
        cmds.joint(parentNode['name'], e=True, zso=True, oj='xyz', sao='yup')
        cmds.joint(parentNode['name'], e=True, o=parentNode['rot'])

        # joint2 is a child of joint1
        cmds.rename(node['name'])
        cmds.joint(node['name'], e=True, zso=True, oj='xyz', sao='yup')
        cmds.joint(parentNode['name'], e=True, o=node['rot'])
        self.numberOfNodes += 1

        # Create a cube baseObject, add subdivision and put it in position
        cmds.polyCube( n='baseObject',w=6, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
        cmds.move(3,0,0, 'baseObject', r=True)
        cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)

        # Add a skin cluster between the skeleton and baseObject
        cmds.skinCluster(parentNode['name'], node['name'], 'baseObject',tsb=True)

        self.ValidateAnimData(Nodes)
        log.info("Completed")

    def test_TransfNodeOnly(self):
        log = logging.getLogger( "test_skeletonHierarchy.test_TransfNodeOnly" )
        log.info("Started")
        cmds.select( d=True )
        self.numberOfNodes = 0

        NodeNames = [
        nameRootSkeleton,
        'Reference',
        ]

        # Create a root transform node
        cmds.createNode( 'transform', n=NodeNames[self.numberOfNodes] )
        cmds.move(3,0,0, 'Root', r=True)
        self.numberOfNodes += 1

        # Create another transform node
        cmds.createNode( 'transform', n=NodeNames[self.numberOfNodes], p=NodeNames[self.numberOfNodes - 1] )
        cmds.move(0,1,0, 'Reference', r=True)
        self.numberOfNodes += 1

        self.ValidatePropData(NodeNames)
        log.info("Completed")
