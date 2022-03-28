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
            tlc, width, height = model.loadUISettings()
            if width > 0 and height > 0:
                self._Window.setWindowRect(tlc, width, height)

        self._Window.setIconPath(iconPath)
        self._Window.initUI()

    def __del__(self):
        self._Model = None
        if self._Window:
            del self._Window
            self._Window = None

    def loadUISettings(self):
        if self._Model:
            return self._Model.loadUISettings()
        return [50, 50], 650, 450

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

    def changeSubjectRole(self, dagPath, newRole):
        if self._Model:
            self._Model.changeSubjectRole(dagPath, newRole)

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

    def getDpiScale(self, size):
        if self._Model:
            return self._Model.getDpiScale(size)
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