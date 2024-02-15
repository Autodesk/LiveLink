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

import weakref
from UnrealLiveLinkWindow import UnrealLiveLinkWindow
from ImageUtils import ImageUtils

class UnrealLiveLinkController():
    def __init__(self, model, windowName, windowTitle, iconPath, parentWidget=None, window=None):
        self._Window = None
        if window:
            self._Window = window
        else:
            self._Window = UnrealLiveLinkWindow(self,
                                                windowName,
                                                windowTitle,
                                                parentWidget)

        self._Model = weakref.proxy(model)
        if model and parentWidget is None:
            tlc, width, height = model.loadUISettings(windowName)
            if width > 0 and height > 0:
                self._Window.setWindowRect(tlc, width, height)

        ImageUtils.setIconPath(iconPath)
        self._Window.initUI()

    def __del__(self):
        self._Model = None
        if self._Window:
            del self._Window
            self._Window = None

    def loadUISettings(self, windowName):
        if self._Model:
            return self._Model.loadUISettings(windowName)
        return [50, 50], 650, 450

    def getUIGeometry(self, windowName):
        if self._Model:
            return self._Model.getUIGeometry(windowName)
        return 0, 0, 650, 450

    def saveUIGeometry(self, windowName, x, y, width, height):
        if self._Model:
            self._Model.saveUIGeometry(windowName, x, y, width, height)

    def setWindowRect(self, tlc, w, h):
        if self._Window:
            self._Window.setWindowRect(tlc, w, h)

    def updateConnectionState(self, connectionText, connectedState):
        if self._Window:
            self._Window.updateConnectionState(connectionText, connectedState)

    def clearUI(self):
        if self._Window:
            self._Window.clearUI()

    def refreshUI(self):
        if self._Window:
            self._Window.refreshUI()

    def refreshSubjects(self):
        if self._Model:
            return self._Model.refreshSubjects()
        return []

    def addSelection(self):
        if self._Model:
            return self._Model.addSelection()
        return False

    def removeSubject(self, dagPath):
        if self._Model:
            self._Model.removeSubject(dagPath)

    def changeSubjectName(self, dagPath, newName):
        if self._Model:
            self._Model.changeSubjectName(dagPath, newName)

    def changeSubjectType(self, dagPath, newType):
        if self._Model:
            self._Model.changeSubjectType(dagPath, newType)

    def getNetworkEndpoints(self):
        if self._Model:
            return self._Model.getNetworkEndpoints()
        return {}

    def setNetworkEndpoints(self, endpointsDict):
        if self._Model:
            self._Model.setNetworkEndpoints(endpointsDict)

    def setWindowProperty(self, name, value):
        if self._Window:
            self._Window.setWindowProperty(name, value)

    def getDpiScale(self, size, useConversion=True):
        if self._Model:
            return self._Model.getDpiScale(size, useConversion)
        return size

    def isDocked(self):
        if self._Model:
            return self._Model.isDocked()
        return False

    def getPluginVersion(self):
        if self._Model:
            return self._Model.getPluginVersion()
        return ""

    def getApiVersion(self):
        if self._Model:
            return self._Model.getApiVersion()
        return ""

    def getUnrealVersion(self):
        if self._Model:
            return self._Model.getUnrealVersion()
        return ""

    def getAboutText(self):
        if self._Model:
            return self._Model.getAboutText()
        return ""

    def getUpdateURL(self):
        if self._Model:
            return self._Model.getUpdateURL()
        return ""

    def getDocumentationURL(self):
        if self._Model:
            return self._Model.getDocumentationURL()
        return ""

    def notifyNewerUpdate(self):
        if self._Window:
            return self._Window.notifyNewerUpdate()
        return ""

    def connectionMade(self):
        if self._Model:
            self._Model.connectionMade()

    def setLoadedUnrealVersion(self, unrealVersionNumber):
        if self._Model:
            return self._Model.setLoadedUnrealVersion(unrealVersionNumber)
        return False

    def getLoadedUnrealVersion(self):
        if self._Model:
            return self._Model.getLoadedUnrealVersion()

    def getNodeType(self, dagPath, isJoint, isJointRootOnly):
        if self._Model:
            return self._Model.getNodeType(dagPath, isJoint, isJointRootOnly)

    def getLinkedAssets(self, nodeType, levelAssets):
        if self._Model:
            return self._Model.getLinkedAssets(nodeType, levelAssets)
        return [], dict()

    def getTargetAssets(self, assetClass):
        if self._Model:
            return self._Model.getTargetAssets(assetClass)
        return dict()

    def getAssetsFromParentClass(self, className, parentClass):
        if self._Model:
            return self._Model.getAssetsFromParentClass(className, parentClass)
        return None, None

    def getAnimSequencesBySkeleton(self):
        if self._Model:
            return self._Model.getAnimSequencesBySkeleton()
        return None

    def linkAsset(self, subjectPath, linkedAssetPath, linkedAssetClass, targetAssetPath, targetAssetName, linkedAssetNativeClass):
        if self._Model:
            self._Model.linkAsset(subjectPath, linkedAssetPath, linkedAssetClass, targetAssetPath, targetAssetName, linkedAssetNativeClass)

    def unlinkAsset(self, subjectPath):
        if self._Model:
            self._Model.unlinkAsset(subjectPath)

    def setLinkProgress(self, value, visible):
        if self._Window:
            self._Window.setLinkProgress(value, visible)

    def updateUILinkInfo(self, dagPath, linkedAssetPath, targetAssetPath, targetAssetName, linkedAssetClass, linkedAssetUnrealNativeClass):
        if self._Window:
            self._Window.updateLinkInfo(dagPath, linkedAssetPath, targetAssetPath, targetAssetName, linkedAssetClass, linkedAssetUnrealNativeClass)

    def isPlayheadSyncEnabled(self):
        if self._Model:
            return self._Model.isPlayheadSyncEnabled()
        return False

    def enablePlayheadSync(self, state):
        if self._Model:
            self._Model.enablePlayheadSync(state)

    def isObjectTransformSyncEnabled(self):
        if self._Model:
            return self._Model.isObjectTransformSyncEnabled()
        return False

    def enableObjectTransformSync(self, state):
        if self._Model:
            self._Model.enableObjectTransformSync(state)
            self._Model.saveIsObjectTransformSyncOption(state)
    
    def loadObjectTransformSyncPreferences(self):
        if self._Model:
            self._Model.loadObjectTransformSyncPreferences()

    def pauseAnimSeqSync(self, state):
        if self._Model:
            self._Model.pauseAnimSeqSync(state)

    def selectedObject(self, hasSelection):
        if self._Window:
            self._Window.selectedObject(hasSelection)

    def enableControls(self, enable):
        if self._Window:
            self._Window.enableControls(enable)