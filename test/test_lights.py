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
import maya.api.OpenMaya as OpenMaya

class test_lights(unittest.TestCase):
    def setUp(self):
        setUpTest()
        # Attributes to change
        self.translation = [3, 4, 5]
        self.expectedUnrealTranslation = [3, 5, 4]
        self.rotation = [90, 90, 90]
        if getUEVersionSuffix() == "4_27":
            self.expectedUnrealRotation = [-90.0, 0, 180]
        else:
            self.expectedUnrealRotation = [-90.0, 0, -180]
        self.scale = [0.5, 0.25, 1.5]
        self.rgb = [1, 0, 0]
        self.intensity = 0.5
        self.temperature = 6500.0
        self.innerConeAngle = 10.0
        self.outerConeAngle = 20.0
        self.attenuationRadius = 1000.0
        self.sourceLength = 0.0
        self.sourceRadius = 0.0
        self.softSourceRadius = 0.0

        cmds.file(new = True, force = True)
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
        exportJson(objectName)
        
        # Check json files were exported properly and add the file path to self.__files so we can delete them in tearDown function.
        staticDataFilePath = expandFileName(getFileNameForStaticData(objectName))
        frameData0FilePath = expandFileName(getFileNameForFrameData(objectName, 0))
       
        self.__files.append(expandFileName(staticDataFilePath))
        self.__files.append(expandFileName(frameData0FilePath))
        
        self.assertTrue(os.path.isfile(staticDataFilePath), msg="Cannot find static data json file.")
        self.assertTrue(os.path.isfile(frameData0FilePath), msg="Cannot find frame 0 data json file.")
        
        return staticDataFilePath, frameData0FilePath

    def validateLightAttributes(self, name, staticData, frameData, usingTemperature, isSpotLight):
        validateColorAttribute('LightColor', self.rgb, staticData, frameData, self)
        validateFloatAttribute('Intensity', self.intensity, staticData, frameData, self)
        if self.innerConeAngle < 0.0:
            validateFloatAttribute('InnerConeAngle', -self.innerConeAngle, staticData, frameData, self, isSpotLight)
            validateFloatAttribute('OuterConeAngle', self.outerConeAngle, staticData, frameData, self, isSpotLight)
        else:
            validateFloatAttribute('InnerConeAngle', self.innerConeAngle + self.outerConeAngle, staticData, frameData, self, isSpotLight)
            validateFloatAttribute('OuterConeAngle', self.innerConeAngle + self.outerConeAngle, staticData, frameData, self, isSpotLight)
        #validateFloatAttribute('AttenuationRadius', self.attenuationRadius, staticData, frameData, self, isSpotLight)
        #validateFloatAttribute('SourceLength', self.sourceLength, staticData, frameData, self, isSpotLight)
        #validateFloatAttribute('SourceRadius', self.sourceRadius, staticData, frameData, self, isSpotLight)
        #validateFloatAttribute('SoftSourceRadius', self.softSourceRadius, staticData, frameData, self, isSpotLight)

    def test_pointLight(self):
        log = logging.getLogger( "test_lights.test_pointLight" )
        log.info("Started")
        cmds.select(d=True)

        # Create light
        light = cmds.pointLight()
        parents = cmds.listRelatives(light, p=True)
        lightRoot = ""
        if parents and len(parents) > 0:
            lightRoot = parents[0]
        self.assertTrue(lightRoot and len(lightRoot) > 0)

        # Apply a transform
        cmds.xform(t=self.translation, ro=self.rotation, scale=self.scale)
        self.assertTrue(almostEqual(self.translation, cmds.xform(q=True, t=True)), msg="Translation not as expected")
        self.assertTrue(almostEqual(self.rotation, cmds.xform(q=True, ro=True)), msg="Rotation not as expected")
        self.assertTrue(almostEqual(self.scale, cmds.xform(q=True, r=True, scale=True)), msg="Scale not as expected")

        # Set the light attributes
        cmds.xform(t=self.translation, ro=self.rotation, scale=self.scale)
        cmds.pointLight(light, e=True, rgb=self.rgb)
        cmds.pointLight(light, e=True, i=self.intensity)
        self.assertTrue(almostEqual(self.rgb, cmds.pointLight(light, q=True, rgb=True)), msg="Color not as expected")
        self.assertAlmostEqual(self.intensity, cmds.pointLight(light, q=True, i=True), msg="Intensity not as expected")

        # Export to JSON
        staticDataFilePath, frameData0FilePath = self.ExportJsonData(lightRoot)
        staticData = getJsonLightData(staticDataFilePath, lightRoot, True)
        self.assertTrue(staticData)
        frameData = getJsonLightData(frameData0FilePath, lightRoot, False)
        self.assertTrue(frameData)

        validateTransform(self.expectedUnrealTranslation, self.expectedUnrealRotation, False, staticData, frameData, self)
        self.validateLightAttributes(light, staticData, frameData, False, False)

        cmds.delete(light)
        log.info("Completed")

    def test_directionalLight(self):
        log = logging.getLogger( "test_lights.test_directionalLight" )
        log.info("Started")
        cmds.select(d=True)

        # Create light
        light = cmds.directionalLight()
        parents = cmds.listRelatives(light, p=True)
        lightRoot = ""
        if parents and len(parents) > 0:
            lightRoot = parents[0]

        # Apply a transform
        cmds.xform(t=self.translation, ro=self.rotation, scale=self.scale)
        self.assertTrue(almostEqual(self.translation, cmds.xform(q=True, t=True)), msg="Translation not as expected")
        self.assertTrue(almostEqual(self.rotation, cmds.xform(q=True, ro=True)), msg="Rotation not as expected")
        self.assertTrue(almostEqual(self.scale, cmds.xform(q=True, r=True, scale=True)), msg="Scale not as expected")

        # Set the light attributes
        cmds.xform(t=self.translation, ro=self.rotation, scale=self.scale)
        cmds.directionalLight(light, e=True, rgb=self.rgb)
        cmds.directionalLight(light, e=True, i=self.intensity)
        self.assertTrue(almostEqual(self.rgb, cmds.directionalLight(light, q=True, rgb=True)), msg="Color not as expected")
        self.assertAlmostEqual(self.intensity, cmds.directionalLight(light, q=True, i=True), msg="Intensity not as expected")

        # Export to JSON
        staticDataFilePath, frameData0FilePath = self.ExportJsonData(lightRoot)
        staticData = getJsonLightData(staticDataFilePath, lightRoot, True)
        self.assertTrue(staticData)
        frameData = getJsonLightData(frameData0FilePath, lightRoot, False)
        self.assertTrue(frameData)

        validateTransform(self.expectedUnrealTranslation, self.expectedUnrealRotation, False, staticData, frameData, self)
        self.validateLightAttributes(light, staticData, frameData, False, False)

        cmds.delete(light)
        log.info("Completed")

    def test_spotLight(self):
        log = logging.getLogger( "test_lights.test_spotLight" )
        log.info("Started")
        cmds.select(d=True)

        # Create light
        light = cmds.spotLight()
        parents = cmds.listRelatives(light, p=True)
        lightRoot = ""
        if parents and len(parents) > 0:
            lightRoot = parents[0]

        # Apply a transform
        cmds.xform(t=self.translation, ro=self.rotation, scale=self.scale)
        self.assertTrue(almostEqual(self.translation, cmds.xform(q=True, t=True)), msg="Translation not as expected")
        self.assertTrue(almostEqual(self.rotation, cmds.xform(q=True, ro=True)), msg="Rotation not as expected")
        self.assertTrue(almostEqual(self.scale, cmds.xform(q=True, r=True, scale=True)), msg="Scale not as expected")

        # Set the light attributes
        cmds.xform(t=self.translation, ro=self.rotation, scale=self.scale)
        cmds.spotLight(light, e=True, rgb=self.rgb)
        cmds.spotLight(light, e=True, i=self.intensity)
        cmds.spotLight(light, e=True, ca=self.outerConeAngle*2)
        cmds.spotLight(light, e=True, p=self.innerConeAngle*2)
        self.assertTrue(almostEqual(self.rgb, cmds.spotLight(light, q=True, rgb=True)), msg="Color not as expected")
        self.assertAlmostEqual(self.outerConeAngle*2, cmds.spotLight(light, q=True, ca=True), msg="Cone angle not as expected")
        self.assertAlmostEqual(self.innerConeAngle*2, cmds.spotLight(light, q=True, p=True), msg="Penumbra angle not as expected")

        # Export to JSON
        staticDataFilePath, frameData0FilePath = self.ExportJsonData(lightRoot)
        staticData = getJsonLightData(staticDataFilePath, lightRoot, True)
        self.assertTrue(staticData)
        frameData = getJsonLightData(frameData0FilePath, lightRoot, False)
        self.assertTrue(frameData)

        validateTransform(self.expectedUnrealTranslation, self.expectedUnrealRotation, False, staticData, frameData, self)
        self.validateLightAttributes(light, staticData, frameData, False, True)

        cmds.delete(light)
        log.info("Completed")
