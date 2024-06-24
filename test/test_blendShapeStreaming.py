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
import json
from utils import *

indexPropertiesStaticData = 2
indexPropertiesFrameData = 3
nameRootSkeleton = 'joint1'

# Read json file associate to the frame/static data.
# Return values of the Properties.
def getPropertiesJsonData(filePath, isStaticData):
    index = indexPropertiesFrameData
    if(isStaticData):
        index = indexPropertiesStaticData
        
    with open(filePath) as f:
        data = json.load(f)
        skeletonJoint1 = data[nameRootSkeleton]
        properties =  skeletonJoint1[index]
        return properties['Properties']

# Generate a very basic skeleton, with a skin cluster, mesh and blend shapes.
# Generate an other object with blend shapes on it. That object in created to make sure we only stream blend shapes associate
#    with the current selected character.
def createCharacter():
   # Create skeleton with joint1 as root (with 3 bones)
    cmds.joint( p=(0,0,0) )
    cmds.joint( p=(2,0,0) )
    cmds.joint( 'joint1', e=True, zso=True, oj='xyz', sao='yup' )
    cmds.joint( p=(4,0,0) )
    cmds.joint( 'joint2', e=True, zso=True, oj='xyz', sao='yup' )
    cmds.joint( p=(6,0,0) )
    cmds.joint( 'joint3', e=True, zso=True, oj='xyz', sao='yup' )
    
    # Create a cube baseObject, add subdivision and put it in position
    cmds.polyCube( n='baseObject',w=6, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
    cmds.move(3,0,0, 'baseObject', r=True)
    cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)
    
    # Add a skin cluster between the skeleton and baseObject
    cmds.skinCluster('joint1', 'joint2', 'joint3', 'baseObject',tsb=True)
    
    # Create 4 targets for blend shapes
    cmds.polyCube( n='target1A',w=2, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
    cmds.move(3,0,0, 'target1A', r=True)
    cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)
    
    cmds.polyCube( n='target1B',w=4, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
    cmds.move(3,0,0, 'target1B', r=True)
    cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)
    
    cmds.polyCube( n='target1C',w=4, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
    cmds.move(3,0,0, 'target1C', r=True)
    cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)
    
    cmds.polyCube( n='target2A',w=1, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
    cmds.move(3,0,0, 'target2A', r=True)
    cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)
    
    # Create one basObject and his target for blend shapes (we should not stream this object nor his blend shapes).
    cmds.polyCube( n='baseObjectNoStream',w=2, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
    cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)
    
    cmds.polyCube( n='targetNoStream',w=1, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
    cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)
    
    # Create blend shapes for object not to stream
    cmds.blendShape('targetNoStream', 'baseObjectNoStream', n='bsNotToStream')
    
    # Create blend shapes with 3 targets associate with our character.
    cmds.blendShape('target1A', 'target1B', 'target1C', 'baseObject' , n='bs1ToStream')
    
    # Create blend shapes with 1 target associate with our character.
    cmds.blendShape('target2A', 'baseObject', n='bs2ToStream')
    
    # Delete targets that became useless after creating the blend shapes
    cmds.delete('target1A', 'target1B', 'target1C', 'target2A', 'targetNoStream')
    
    # Set combinaiton target for target1C driven by target1A and target1B
    # Combine method to 0 is Multiplication 
    cmds.combinationShape(blendShape='bs1ToStream', combinationTargetIndex=2, driverTargetIndex=[0,1], combineMethod=0)
  
  
       
class test_blendShapeStreaming(unittest.TestCase):
    
    def setUp(self):
        setUpTest()
        cmds.file(new = True, force = True)
        self.__files = []
        loadPlugins()
        createCharacter()


    def tearDown(self):
        cmds.file(new = True, force = True)
        for f in self.__files:
            if os.path.exists(f):
                os.remove(f)
        tearDownTest()

    def test_blendShapeStreamSingleFrame(self):
        log = logging.getLogger( "test_blendShapeStreaming.test_blendShapeStreamSingleFrame" )
        log.info("Started")

        # Set weight of each blend shapes:
        # In bs1ToStream
        #      - target1A = 0.76
        #      - target1B = 0.1
        cmds.blendShape('bs1ToStream', edit=True, w=[(0, 0.76), (1, 0.1)])

        # Set the target/parent target visibility of each target.
        cmds.setAttr('bs1ToStream.tgvs[0]', True)
        cmds.setAttr('bs1ToStream.tpvs[0]', True)
        cmds.setAttr('bs1ToStream.tgvs[1]', True)
        cmds.setAttr('bs1ToStream.tpvs[1]', True)
        cmds.setAttr('bs1ToStream.tgvs[2]', True)
        cmds.setAttr('bs1ToStream.tpvs[2]', True)
        
        # In bs2ToStream
        #     - target2A = 1.0
        cmds.blendShape('bs2ToStream', edit=True, w=[(0, 1.0)])

        # Set the target/parent target visibility of each target.
        cmds.setAttr('bs2ToStream.tgvs[0]', True)
        cmds.setAttr('bs2ToStream.tpvs[0]', True)
        
        # Export Json
        exportJson(nameRootSkeleton)
        
        # Check json files were exported properly and add the file path to self.__files so we can delete them in tearDown function.
        staticDataFilePath = expandFileName(getFileNameForStaticData(nameRootSkeleton))
        frameData0FilePath = expandFileName(getFileNameForFrameData(nameRootSkeleton, 0))
       
        self.__files.append(expandFileName(staticDataFilePath))
        self.__files.append(expandFileName(frameData0FilePath))
        
        self.assertTrue(os.path.isfile(staticDataFilePath), msg="Cannot find static data json file.")
        self.assertTrue(os.path.isfile(frameData0FilePath), msg="Cannot find frame 0 data json file.")
        
        # Get data exported from json files
        names = getPropertiesJsonData(staticDataFilePath, True)
        values = getPropertiesJsonData(frameData0FilePath, False)
        
        nbNames = len(names)
        nbValues = len(values)
        
        # Assert data are streamable and will not cause errors or issues in Unreal
        self.assertEqual(nbValues, nbNames, msg="The number of properties in the StaticData and the number of properties in the FrameData are not equals. Streaming data to Unreal will fail.")
        self.assertEqual(len(set(names)), nbNames, msg="There are duplicate data in properties in StaticData. This should not happen. It might causes issues when unreal try to match the curve names with the blend shapes streamed.")
        
        # Boolean to assert the blend shapes that are suppose to be stream are streamed.
        isTarget1AStreamed = False
        isTarget1BStreamed = False
        isTarget1CStreamed = False
        isTarget2AStreamed = False
        
        #Assert value streamed
        for idx in range(min(nbNames, nbValues)):
            name = names[idx]
            value = values[idx]
            
            if(name == "target1A"):
                isTarget1AStreamed = True
                self.assertAlmostEqual(0.760, value, msg="The value of the target named target1A is not set to 0.76, but to " + str(value) + " instead. The value streamed is not the expected value.")
            elif(name == "target1B"):
                isTarget1BStreamed = True
                self.assertAlmostEqual(0.1, value, msg="The value of the target named target1B is not set to 0.0, but to " + str(value) + " instead. The value streamed is not the expected value.")
            elif(name == "target2A"):
                isTarget2AStreamed = True
                self.assertAlmostEqual(1.0, value, msg="The value of the target named target2A is not set to 1.0, but to " + str(value) + " instead. The value streamed is not the expected value.")
            elif(name == "target1C"):
                isTarget1CStreamed = True
                self.assertAlmostEqual(0.0760, value, msg="The value of the target named target1C (which is a combination target driven by target1A and target1B)is not set to 0.0760, but to " + str(value) + " instead. The value streamed is not the expected value.")
            else:
                raise RuntimeError("A target named " + name + " is streamed but should not be stream. It is not associate with the character we are currently streaming.")
        
        # Assert the blend shapes that are suppose to be stream are streamed.
        self.assertTrue(isTarget1AStreamed, msg="Target1A was not streamed. Target1A is a blend shapes associate with the character we are streaming and should be streamed.")
        self.assertTrue(isTarget1BStreamed, msg="Target1B was not streamed. Target1B is a blend shapes associate with the character we are streaming and should be streamed.")
        self.assertTrue(isTarget1CStreamed, msg="Target1C was not streamed. Target1C is a blend shapes associate with the character we are streaming and should be streamed.")
        self.assertTrue(isTarget2AStreamed, msg="Target2A was not streamed. Target2A is a blend shapes associate with the character we are streaming and should be streamed.")

        log.info("Completed")
         
            
    def test_blendShapeStreamMultipleFrames(self):
        log = logging.getLogger( "test_blendShapeStreaming.test_blendShapeStreamMultipleFrames" )
        log.info("Started")

        # SET ANIMATION BLEND SHAPE FRAME 5
        cmds.currentTime(5)
        
        # Set weight of each blend shapes:
        # In bs1ToStream
        #      - target1A = 0.76
        #      - target1B = 0.0
        cmds.blendShape('bs1ToStream', edit=True, w=[(0, 0.76), (1, 0.0)])
        cmds.setKeyframe('bs1ToStream.w[0]')
        cmds.setKeyframe('bs1ToStream.w[1]')

        # Set the target/parent target visibility of each target.
        cmds.setAttr('bs1ToStream.tgvs[0]', True)
        cmds.setAttr('bs1ToStream.tpvs[0]', True)
        cmds.setAttr('bs1ToStream.tgvs[1]', True)
        cmds.setAttr('bs1ToStream.tpvs[1]', True)
        cmds.setAttr('bs1ToStream.tgvs[2]', True)
        cmds.setAttr('bs1ToStream.tpvs[2]', True)
        
        # In bs2ToStream
        #     - target2A = 1.0
        cmds.blendShape('bs2ToStream', edit=True, w=[(0, 1.0)])
        cmds.setKeyframe('bs2ToStream.w[0]')

        # Set the target/parent target visibility of each target.
        cmds.setAttr('bs2ToStream.tgvs[0]', True)
        cmds.setAttr('bs2ToStream.tpvs[0]', True)
        
        
        # SET ANIMATION BLEND SHAPE FRAME 6
        cmds.currentTime(6)
        
        # Set weight of each blend shapes:
        # In bs1ToStream
        #      - target1A = 0.26
        #      - target1B = 0.60
        cmds.blendShape('bs1ToStream', edit=True, w=[(0, 0.26), (1, 0.60)])
        cmds.setKeyframe('bs1ToStream.w[0]')
        cmds.setKeyframe('bs1ToStream.w[1]')

        # Set the target/parent target visibility of each target.
        cmds.setAttr('bs1ToStream.tgvs[0]', True)
        cmds.setAttr('bs1ToStream.tpvs[0]', True)
        cmds.setAttr('bs1ToStream.tgvs[1]', True)
        cmds.setAttr('bs1ToStream.tpvs[1]', True)
        cmds.setAttr('bs1ToStream.tgvs[2]', True)
        cmds.setAttr('bs1ToStream.tpvs[2]', True)
        
        # In bs2ToStream
        #     - target2A = 1.0
        cmds.blendShape('bs2ToStream', edit=True, w=[(0, 1.0)])
        cmds.setKeyframe('bs2ToStream.w[0]')

        # Set the target/parent target visibility of each target.
        cmds.setAttr('bs2ToStream.tgvs[0]', True)
        cmds.setAttr('bs2ToStream.tpvs[0]', True)
        
        
        # SET ANIMATION BLEND SHAPE FRAME 7
        cmds.currentTime(7)
        
        # Set weight of each blend shapes:
        # In bs1ToStream
        #      - target1A = 0.5
        #      - target1B = 1.0
        cmds.blendShape('bs1ToStream', edit=True, w=[(0, 0.5), (1, 1.0)])
        cmds.setKeyframe('bs1ToStream.w[0]')
        cmds.setKeyframe('bs1ToStream.w[1]')

        # Set the target/parent target visibility of each target.
        cmds.setAttr('bs1ToStream.tgvs[0]', True)
        cmds.setAttr('bs1ToStream.tpvs[0]', True)
        cmds.setAttr('bs1ToStream.tgvs[1]', True)
        cmds.setAttr('bs1ToStream.tpvs[1]', True)
        cmds.setAttr('bs1ToStream.tgvs[2]', True)
        cmds.setAttr('bs1ToStream.tpvs[2]', True)
        
        # In bs2ToStream
        #     - target2A = 0.1
        cmds.blendShape('bs2ToStream', edit=True, w=[(0, 0.1)])
        cmds.setKeyframe('bs2ToStream.w[0]')

        # Set the target/parent target visibility of each target.
        cmds.setAttr('bs2ToStream.tgvs[0]', True)
        cmds.setAttr('bs2ToStream.tpvs[0]', True)
        
        
        # List of expected values for each target at frame 5 to 7
        target1AValues = [0.76, 0.26, 0.5]
        target1BValues = [0.0, 0.60, 1.0]
        target2AValues = [1.0, 1.0, 0.1]

        # Export Json
        exportJson(nameRootSkeleton, 5, 7) 
        
        # Check json files were exported properly and add the file path to self.__files so we can delete them in tearDown function.
        staticDataFilePath = expandFileName(getFileNameForStaticData(nameRootSkeleton))
        frameData5FilePath = expandFileName(getFileNameForFrameData(nameRootSkeleton, 5))
        frameData6FilePath = expandFileName(getFileNameForFrameData(nameRootSkeleton, 6))
        frameData7FilePath = expandFileName(getFileNameForFrameData(nameRootSkeleton, 7))
       
        self.__files.append(expandFileName(staticDataFilePath))
        self.__files.append(expandFileName(frameData5FilePath))
        self.__files.append(expandFileName(frameData6FilePath))
        self.__files.append(expandFileName(frameData7FilePath))
        
        self.assertTrue(os.path.isfile(staticDataFilePath), msg="Cannot find static data json file.")
        self.assertTrue(os.path.isfile(frameData5FilePath), msg="Cannot find frame 5 data json file.")
        self.assertTrue(os.path.isfile(frameData6FilePath), msg="Cannot find frame 6 data json file.")
        self.assertTrue(os.path.isfile(frameData7FilePath), msg="Cannot find frame 7 data json file.")        

        # Get static data from json file
        names = getPropertiesJsonData(staticDataFilePath, True)
        nbNames = len(names)
        
        # Assert data are streamable and will not cause errors or issues in Unreal
        self.assertEqual(len(set(names)), nbNames, msg="There are duplicate data in properties in StaticData. This should not happen. It might causes issues when Unreal try to match the curve names with the blend shapes streamed.")
        
        # Loop to test each frame from 5 to 7
        for idxFrame in range(3):
            
            frame = idxFrame + 5
            
            # Get frame data exported from json files
            values = getPropertiesJsonData(expandFileName( getFileNameForFrameData(nameRootSkeleton, frame)), False)
            
            nbValues = len(values)
            
            # Assert data are streamable and will not cause errors or issues in Unreal
            self.assertEqual(nbValues, nbNames,msg= "The number of properties in the StaticData and the number of properties in the FrameData are not equals. Streaming data to Unreal will fail.")
            
            # Boolean to assert the blend shapes that are suppose to be stream are streamed.
            isTarget1AStreamed = False
            isTarget1BStreamed = False
            isTarget1CStreamed = False
            isTarget2AStreamed = False
        
            #Assert values streamed
            for idx in range(min(nbNames, nbValues)):
                name = names[idx]
                value = values[idx]
                
                if(name == "target1A"):
                    isTarget1AStreamed = True
                    self.assertAlmostEqual(target1AValues[idxFrame], value, msg="The value of the target named target1A is not set to " + str(target1AValues[idxFrame]) + ", but to " + str(value) + " instead. The value streamed is not the expected value. Issue occure at frame " + str(frame))
                elif(name == "target1B"):
                    isTarget1BStreamed = True
                    self.assertAlmostEqual(target1BValues[idxFrame], value, msg="The value of the target named target1B is not set to " + str(target1BValues[idxFrame]) + ", but to " + str(value) + " instead. The value streamed is not the expected value. Issue occure at frame " + str(frame))
                elif(name == "target2A"):
                    isTarget2AStreamed = True
                    self.assertAlmostEqual(target2AValues[idxFrame], value, msg="The value of the target named target2A is not set to " + str(target2AValues[idxFrame]) + ", but to " + str(value) + " instead. The value streamed is not the expected value. Issue occure at frame " + str(frame))
                elif (name == "target1C"):
                    isTarget1CStreamed = True
                    expectedValue = target1AValues[idxFrame] * target1BValues[idxFrame]
                    self.assertAlmostEqual(expectedValue, value, msg="The value of the target named target1C (which is a combination target driven by target1A and target1B)is not set to " + str(value) + ", but to " + str(value) + " instead. The value streamed is not the expected value. Issue occure at frame " + str(frame))
                else:
                    raise RuntimeError("A target named " + name + " is streamed but should not be stream. It is not associate with the character we are currently streaming.")
                
            # Assert the blend shapes that are suppose to be stream are streamed.
            self.assertTrue(isTarget1AStreamed, msg="Target1A was not streamed. Target1A is a blend shapes associate with the character we are streaming and should be streamed. Issue occure at frame " + str(frame))
            self.assertTrue(isTarget1BStreamed, msg="Target1B was not streamed. Target1B is a blend shapes associate with the character we are streaming and should be streamed. Issue occure at frame " + str(frame))
            self.assertTrue(isTarget1CStreamed, msg="Target1C was not streamed. Target1C is a blend shapes associate with the character we are streaming and should be streamed. Issue occure at frame " + str(frame))
            self.assertTrue(isTarget2AStreamed, msg="Target2A was not streamed. Target2A is a blend shapes associate with the character we are streaming and should be streamed. Issue occure at frame " + str(frame))

        log.info("Completed")
                    
