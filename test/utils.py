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

import json
import math
import os
import platform

import maya.standalone
import maya.api.OpenMaya as OpenMaya
import maya.cmds as cmds

def setUpTest():
    os.environ['MAYA_UNREAL_LIVELINK_AUTOMATED_TESTS'] = '1'
    testPath =  os.path.abspath(os.path.dirname(__file__))
    if(platform.system()=='Linux'):
        binariesPlatform = 'Linux'
    else:
        binariesPlatform = 'Win64'

    MayaVersion = os.environ.get('MAYA_VERSION')
    assert MayaVersion,"MAYA_VERSION not specified"

    # Must check if MAYA_PLUG_IN_PATH is not already set, otherwise, assume executing from within source tree.
    if 'MAYA_PLUG_IN_PATH' not in os.environ:
        # Looks like MAYA_MODULE_PATH is not working when used from mayapy.
        # os.environ['MAYA_MODULE_PATH'] = os.path.join(testPath,'..','Binaries',binariesPlatform,'Maya')
        os.environ['MAYA_PLUG_IN_PATH'] = os.path.join(testPath,'..','Binaries',binariesPlatform,'Maya',MayaVersion)+os.pathsep+os.path.join(testPath,'..','Source','Programs','MayaUnrealLiveLinkPlugin')
        os.environ['MAYA_SCRIPT_PATH'] = os.path.join(testPath,'..','Source','Programs','MayaUnrealLiveLinkPlugin')

    # If running tests from within Maya, standalone.initialize is not required and will fail.
    try:
        maya.standalone.initialize(name='python')
    except:
        pass

def tearDownTest():
    pass
#    maya.standalone.uninitialize()

def loadPlugin(name):
    if not cmds.pluginInfo(name, q=1, loaded=1):
        cmds.loadPlugin(name)

def getUEVersionSuffix():
    UEVersionSuffix = os.environ.get('UNITTEST_UNREAL_VERSION')
    assert UEVersionSuffix,"UNITTEST_UNREAL_VERSION not specified"
    return UEVersionSuffix
    
def loadPlugins():
    version_suffix = getUEVersionSuffix()
    if(platform.system()=='Linux'):
        extension = ".so"
        prefix = "lib"
    else:
        extension = ".mll"
        prefix = ""

    loadPlugin(prefix + "MayaUnrealLiveLinkPlugin_" + version_suffix + extension)
    loadPlugin("MayaUnrealLiveLinkPluginUI.py")

# Read json file associated to anim data and get the scene time in seconds.
def getJsonAnimSceneTimeInSeconds(filePath, nameRootSkeleton, hasCustomAttrs=False):
    with open(filePath) as f:
        data = json.load(f)
        if data and nameRootSkeleton in data:
            rootNode = data[nameRootSkeleton]
            rootNodeLen = len(rootNode)
            # Properties is an extra field that is added when we stream custom attributes
            if rootNodeLen == (3+hasCustomAttrs) and rootNode[0]['Role'] == 'Anim' and rootNode[1]['Time'] != None:
                return rootNode[1]['Time']
        return None

# Read json file associate to the anim frame/static data.
def getJsonAnimData(filePath, nameRootSkeleton, isStaticData):
    with open(filePath) as f:
        data = json.load(f)
        if data and nameRootSkeleton in data:
            rootNode = data[nameRootSkeleton]
            rootNodeLen = len(rootNode)
            if isStaticData and rootNodeLen == 2 and rootNode[0]['Role'] == 'Anim':
                return rootNode[1]['BoneHierarchy']
            elif isStaticData is False and rootNodeLen == 3 and rootNode[0]['Role'] == 'Anim' and rootNode[1]['Time'] != None:
                return rootNode[2]['BoneTransforms']
        return None

# Read json file associate to the anim frame/static data with custom attributes.
def getJsonAnimCustomAttrData(filePath, nameRootSkeleton, isStaticData):
    with open(filePath) as f:
        data = json.load(f)
        if data and nameRootSkeleton in data:
            rootNode = data[nameRootSkeleton]
            rootNodeLen = len(rootNode)
            if isStaticData and rootNodeLen == 3 and rootNode[0]['Role'] == 'Anim':
                return rootNode[2]['Properties']
            elif isStaticData is False and rootNodeLen == 4 and rootNode[0]['Role'] == 'Anim' and rootNode[1]['Time'] != None:
                return rootNode[3]['Properties']
        return None

def getJsonData(filePath, objectName, isStaticData, roleName):
    with open(filePath) as f:
        data = json.load(f)
        if data and objectName in data:
            rootNode = data[objectName]
            rootNodeLen = len(rootNode)
            if isStaticData and rootNodeLen == 2 and rootNode[0]['Role'] == roleName:
                return rootNode[1]
            elif isStaticData is False and rootNodeLen == 3 and rootNode[0]['Role'] == roleName and rootNode[1]['Time'] != None:
                return rootNode[2]
        return None

# Read json file associate to the camera frame/static data.
def getJsonCameraData(filePath, objectName, isStaticData):
    return getJsonData(filePath, objectName, isStaticData, 'Camera')

# Read json file associate to the light frame/static data.
def getJsonLightData(filePath, objectName, isStaticData):
    return getJsonData(filePath, objectName, isStaticData, 'Light')

# Read json file associate to the prop frame/static data.
def getJsonPropData(filePath, objectName, isStaticData):
    return getJsonData(filePath, objectName, isStaticData, 'Transf')

# Return json file name for static data
def getFileNameForStaticData(objectName):
    return objectName + "_StaticData.json"

# Return json file name for frame data for a specific frame.
def getFileNameForFrameData(objectName, timeFrame):
    return objectName + "_Frame_" + str(timeFrame) + '.json'

def expandFileName(name):
    temp = cmds.internalVar( utd=True )
    if not os.path.exists(temp):
        temp = os.getcwd()
    return os.path.join(temp, name)

# Export the json files for multiple frames for the object selected.
# Will generate json files (one for the static data, and one json file for each frame indidate in the time range given in argument) 
# Will stream frame from startFrame to endFrame inclusively
def exportJson(nameObjectToSelect, startFrame=0, endFrame=-1, streamType="", setTime=True):
    filePathStaticData = expandFileName(getFileNameForStaticData(nameObjectToSelect))
    cmds.select(nameObjectToSelect, r=True)
    cmds.LiveLinkChangeSource(2)
    cmds.LiveLinkAddSelection()
    
    # If streamtype needs to be changed we do that now.
    if (streamType):
        selList = OpenMaya.MSelectionList()
        selList.add(nameObjectToSelect)
        cmds.LiveLinkChangeSubjectStreamType(selList.getDagPath(0).fullPathName(), streamType)
        
    cmds.LiveLinkExportStaticData(filePathStaticData)

    endFrame = max(startFrame, endFrame)
    for frame in range(startFrame, endFrame+1):
       if setTime:
           # set setTime as False for testing uncommitted (not keyed) edits
           cmds.currentTime(frame)
       filePathFrameData = expandFileName(getFileNameForFrameData(nameObjectToSelect, frame))
       cmds.LiveLinkExportFrameData(filePathFrameData, frame)

def selectAndAddSubjectsToLiveLink(subjects, unittest):
    objects = []

    if not isinstance(subjects, list):
        objects.append(subjects)
    else:
        objects = subjects

    objectLen = len(objects)
    cmds.select(objects, r=True)
    cmds.LiveLinkAddSelection()
    SubjectNames = cmds.LiveLinkSubjectNames()
    SubjectPaths = cmds.LiveLinkSubjectPaths()
    SubjectTypes = cmds.LiveLinkSubjectTypes()
    SubjectRoles = cmds.LiveLinkSubjectRoles()
    unittest.assertEqual(objectLen, len(SubjectNames))
    unittest.assertEqual(objectLen, len(SubjectPaths))
    unittest.assertEqual(objectLen, len(SubjectTypes))
    unittest.assertEqual(objectLen, len(SubjectRoles))

def almostEqual(list1, list2):
    if isinstance(list1, list) and isinstance(list2, list) and len(list1) == len(list2):
        return all(math.isclose(*values, abs_tol=1e-04) for values in zip(list1, list2))
    return False

def validateColorAttribute(attributeName, value, staticData, frameData, unittest, mustBePresent = True):
    unittest.assertTrue(isinstance(value, list) and len(value) == 3)

    if attributeName in staticData:
        unittest.assertTrue(attributeName in frameData)
        unittest.assertTrue(almostEqual(frameData[attributeName][:3], [x * 255 for x in value]))
    elif mustBePresent:
        unittest.assertTrue(False, msg=attributeName + " must be present in json file")

def validateVectorAttribute(attributeName, value, staticData, frameData, unittest, mustBePresent = True):
    unittest.assertTrue(isinstance(value, list) and len(value) == 3)

    if attributeName in staticData:
        unittest.assertTrue(attributeName in frameData)
        unittest.assertTrue(almostEqual(frameData[attributeName], value))
    elif mustBePresent:
        unittest.assertTrue(False, msg=attributeName + " must be present in json file")

def validateFloatAttribute(attributeName, value, staticData, frameData, unittest, mustBePresent = True):
    unittest.assertTrue(isinstance(value, float))

    if attributeName in staticData:
        unittest.assertTrue(attributeName in frameData)
        unittest.assertAlmostEqual(frameData[attributeName], value)
    elif mustBePresent:
        unittest.assertTrue(False, msg=attributeName + " must be present in json file")

def validateTransform(expectedUnrealTranslation, expectedUnrealRotation, scaleSupported, staticData, frameData, unittest, expectedUnrealScale=[0,0,0]):
    '''
    Transforms expected in Unreal coordinate system.
    
    If argument 'scaleSupported' is set to True, then optional 'expectedUnrealScale' must be provided, not needed if False.
    '''
    # Validate translation
    unittest.assertTrue('Location' in staticData, msg="No 'Location' in staticData")
    unittest.assertTrue('L' in frameData, msg="No 'L' in frameData")
    unittest.assertTrue(almostEqual(frameData['L'], expectedUnrealTranslation), msg="Expected Translation " + str(expectedUnrealTranslation) + ", got: " + str(frameData['L']) )

    # Validate rotation
    unittest.assertTrue('Rotation' in staticData, msg="No 'Rotation' in staticData")
    unittest.assertTrue('R' in frameData, msg="No 'R' in frameData")
    euler = OpenMaya.MQuaternion(frameData['R']).asEulerRotation()
    eulerDeg = [math.degrees(euler.x), math.degrees(euler.y), math.degrees(euler.z)]
    unittest.assertTrue(almostEqual(expectedUnrealRotation, eulerDeg), msg="Expected Rotation " + str(expectedUnrealRotation) + ", got " + str(eulerDeg))

    # Validate scale
    if scaleSupported:
        unittest.assertTrue('Scale' in staticData, msg="'Scale' expected in staticData, got " + str('Scale' in staticData))
        unittest.assertTrue('S' in frameData, msg="'S' expected in frameData, got " + str('S' in frameData))
        unittest.assertFalse(expectedUnrealScale==[0,0,0], msg="'scaleSupported' set to " + str(scaleSupported) + " but no 'expectedUnrealScale' provided")
        unittest.assertTrue(almostEqual(frameData['S'], expectedUnrealScale), msg="Expected Scale " + str(expectedUnrealScale) + ", got: " + str(frameData['S']) )      
    else:
        unittest.assertFalse('Scale' in staticData, msg="No 'Scale' expected in staticData, got " + str('Scale' in staticData))
        unittest.assertFalse('S' in frameData, msg="No 'S' expected in frameData, got " + str('S' in frameData))
        
def validateBoneTransform(boneIndex, expectedUnrealTranslation, expectedUnrealRotation, boneTransforms, unittest):
    boneTransformsLen = len(boneTransforms)
    unittest.assertTrue(isinstance(boneTransforms, list) and boneTransformsLen > 0)
    unittest.assertLess(boneIndex, boneTransformsLen)

    boneTransform = boneTransforms[boneIndex]

    # Validate translation
    unittest.assertTrue('L' in boneTransform, msg="No 'L' found in framedata")
    unittest.assertTrue(almostEqual(expectedUnrealTranslation, boneTransform['L']), msg="Not the expected Translation in framedata, expected " + str(expectedUnrealTranslation) + " got " + str(boneTransform['L']))

    # Validate rotation (quaternion)
    unittest.assertTrue('R' in boneTransform, msg="no 'R' in framedata")
    unittest.assertTrue(almostEqual(expectedUnrealRotation, boneTransform['R']), msg="Not the expected Quaternion in framedata, expected " + str(expectedUnrealRotation) + " got " + str(boneTransform['R']))

    # Validate scale is not present
    unittest.assertFalse('S' in boneTransform, msg="Found 'S' in framedata but should not")