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
nameMotionPath = 'motionPath1'

class test_motionPath(unittest.TestCase):
    '''
    Tests Joint on motion path: animation and uncommitted (not keyed) edits
    '''    
    def setUp(self):
        setUpTest()
        cmds.file(new = True, force = True)
        # Set current time to be 30fps
        cmds.currentUnit( time='ntsc' )
        cmds.playbackOptions(animationStartTime=0, animationEndTime=100)
        self.__files = []
        loadPlugins()

    def tearDown(self):
        cmds.file(new = True, force = True)
        for f in self.__files:
            if os.path.exists(f):
                os.remove(f)
        tearDownTest()

    def ExportJsonData(self, f, setTimeVal):
        # Export Json
        exportJson(nameRootSkeleton, startFrame=f, endFrame=-1, setTime=setTimeVal)

        # Check json files were exported properly and add the file path to self.__files so we can delete them in tearDown function.
        staticDataFilePath = expandFileName(getFileNameForStaticData(nameRootSkeleton))
        frameDataFilePath = expandFileName(getFileNameForFrameData(nameRootSkeleton, f))

        self.__files.append(expandFileName(staticDataFilePath))
        self.__files.append(expandFileName(frameDataFilePath))

        self.assertTrue(os.path.isfile(staticDataFilePath), msg="Cannot find static data json file.")
        self.assertTrue(os.path.isfile(frameDataFilePath), msg="Cannot find frame " + str(f) + " data json file.")

        return frameDataFilePath

    def ValidateAnimData(self, Nodes, f):
        # Make sure we created all the required nodes, there should be 6 nodes: 4 joints, a mesh and a curve
        self.assertEqual(6, len(Nodes), msg="The number of nodes is not the same as the Nodes array size")

        # export to json
        frameDataFilePath = self.ExportJsonData(f, True)
        
        # Get data exported from json files
        values = getJsonAnimData(frameDataFilePath, nameRootSkeleton, False)
        
        # construct expected Translation from frame (f) value, the node in test does not rotate so a single rotation is expected
        expectedTrans = [-10, f, f/10]
        expectedQuat = [-0.7062290906906128, 0.03522045910358429, -0.7062290906906128, 0.03522045910358429]
        validateBoneTransform(0, expectedTrans, expectedQuat, values, self)

    def validateUncommitedData(self, v, f):
        # export to json
        frameDataFilePath = self.ExportJsonData(f, False)
                
        # Get data exported from json files
        values = getJsonAnimData(frameDataFilePath, nameRootSkeleton, False)
        
        # test translation for uncommited uValue
        expectedTrans = [-10, v, v/10]
        expectedQuat = [-0.7062290906906128, 0.03522045910358429, -0.7062290906906128, 0.03522045910358429]
        validateBoneTransform(0, expectedTrans, expectedQuat, values, self)        


    def test_skeletonJointOnPath(self):
        # set maya returned floats to a rounding of
        roundingTo = 3
        
        # start logger
        log = logging.getLogger( "test_motionPath.test_skeletonJointOnPath" )
        log.info("Started")
        
        # Create skeleton with joint1 as root (with 3 bones)
        cmds.select( clear=True )
        cmds.joint( p=(0,0,0) )
        cmds.joint( p=(2,0,0) )
        cmds.joint( nameRootSkeleton, e=True, zso=True, oj='xyz', sao='yup' )
        cmds.joint( p=(4,0,0) )
        cmds.joint( 'joint2', e=True, zso=True, oj='xyz', sao='yup' )
        cmds.joint( p=(6,0,0) )
        cmds.joint( 'joint3', e=True, zso=True, oj='xyz', sao='yup' )
        
        # Create a cube as skeletalMesh, add subdivision and put it in position
        cmds.polyCube( n='skeletalMesh',w=6, h=1, d=1, sx=16, sy=4, sz=4, ax=(0,1,0), cuv=4, ch=1)
        cmds.move(3,0,0, 'skeletalMesh', r=True)
        cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=2)
        
        # Add a skin cluster between the skeleton and skeletalMesh
        cmds.skinCluster(nameRootSkeleton, 'joint2', 'joint3', 'skeletalMesh',tsb=True)

        # create a basic curve with 4 points
        # points position is used to validate Translation,
        # first point is (x, 0, 0) on frame 0 and last point is (x, 10 , 100) on frame 100,
        # so expected Y is currentFrame/10 and expected Z as currentFrame
        curveNode = cmds.curve(n='test_curve', p=[(-10, 0, 0), (-10, 3.333, 33.333), (-10, 6.667, 66.667), (-10, 10, 100)] )
        
        # create motion path: set rootJoint node on curveNode as motion path from frame 0 to 100, this creates an animation
        cmds.select( clear=True )
        cmds.select( nameRootSkeleton, curveNode)
        cmds.pathAnimation(stu=0, etu=100, f=True, wut="object", name=nameMotionPath)

        # set motion path uValue animation to linear so it's uValue follows frame value
        cmds.keyTangent(nameMotionPath, edit=True, time=(0,100), attribute='uValue', inTangentType='linear', outTangentType='linear')

        # PART 1 validate animation playback on motion path    
        for f in range(101):
            cmds.currentTime(f)
            
            txval = round((cmds.getAttr('joint1.translateX')), roundingTo)
            tyval = round((cmds.getAttr('joint1.translateY')), roundingTo)
            tzval = round((cmds.getAttr('joint1.translateZ')), roundingTo)
            
            # validate maya animation
            self.assertEqual(-10, txval, msg=("Maya animation translation on X not as expected on frame ",  str(f)))
            self.assertEqual(f/10.0, tyval, msg=("Maya animation translation on Y not as expected on frame ",  str(f)))
            self.assertEqual(f, tzval, msg=("Maya animation translation on Z not as expected on frame ",  str(f)))
            
            # validate framedata unreal animation
            self.ValidateAnimData(nameRootSkeleton, f)
       
        # PART 2 validate uncommited editing
        # set a value to the motion path uValue without keying
        # make sure autoKey is Off and go to frame 50 to test 4 uValues
        testFrame = 50
        cmds.autoKeyframe( state=False )
        cmds.currentTime(testFrame)
        uValues=[0.18, 0.353, 0.651, 1]
   
        for v in uValues:
            # set an uncommitted value
            cmds.setAttr( (nameMotionPath)+".uValue", v)
            # validate attribute changed
            self.assertEqual(v, cmds.getAttr( (nameMotionPath)+".uValue" ), msg="uValue not set as expected to: " + str(v) )
            
            # get and validate uncommitted edit in maya
            vval_y = round((v*10), roundingTo)
            vval_z = round((v*100), roundingTo)
            
            # get maya translations and validate uncommitted uValue has affected 'joint1'
            txval = round((cmds.getAttr('joint1.translateX')), roundingTo)
            tyval = round((cmds.getAttr('joint1.translateY')), roundingTo)
            tzval = round((cmds.getAttr('joint1.translateZ')), roundingTo)
            
            self.assertEqual(-10, txval, msg=("Maya animation translation on X not as expected, expected -10, got" + str(txval) ))
            self.assertEqual(vval_y, tyval, msg=("Maya animation translation on Y, expected " + str(vval_y) + " got " +str(tyval) ))
            self.assertEqual(vval_z, tzval, msg=("Maya animation translation on Z, expected " + str(vval_z) + " got " +str(tzval) ))            
            
            # validate framedata exported to unreal
            self.validateUncommitedData(vval_z, testFrame)
        
        log.info("Completed")