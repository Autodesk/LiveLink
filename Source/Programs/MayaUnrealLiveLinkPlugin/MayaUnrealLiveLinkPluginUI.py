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

import inspect
import io
import json
import os
import sys
import weakref

from maya import cmds
from maya import mel
from maya import OpenMayaUI as omui
import maya.api.OpenMaya as OpenMaya
import maya.OpenMayaMPx as OpenMayaMPx

sys.dont_write_bytecode = True

IsAutomatedTest = os.environ.get('MAYA_UNREAL_LIVELINK_AUTOMATED_TESTS')
if IsAutomatedTest is None:
    # To be able to add a script subfolder, we need to import this module to have __file__ be defined
    # We can then get the current path for this script, remove the filename and add the subfolder script name instead
    import MayaUnrealLiveLinkPluginUI
    def getBasePath():
        basePath = __file__
        basePath = basePath.replace('\\', '/')
        scriptPathIndex = basePath.rfind('/')
        if scriptPathIndex >= 0:
            basePath = basePath[:scriptPathIndex]
        return basePath
    def addPath(path):
        if (path in sys.path) is False:
            print('Adding ' + path + ' to system path')
            sys.path.append(path)
    basePath = MayaUnrealLiveLinkPluginUI.getBasePath()
    addPath(basePath)
    iconPath = basePath + '/icons'
    scriptPath = basePath + '/scripts'
    addPath(scriptPath)
    from UnrealLiveLinkController import UnrealLiveLinkController
    from UnrealLiveLinkWindow import UnrealLiveLinkWindow
    from maya.app.general.mayaMixin import MayaQWidgetDockableMixin
    try:
      from shiboken2 import wrapInstance
    except ImportError:
      from shiboken import wrapInstance
    from PySide2.QtWidgets import QWidget, QVBoxLayout
    from PySide2.QtCore import QUrl, QJsonDocument, QJsonArray, QProcess
    from PySide2.QtNetwork import QNetworkAccessManager, QNetworkRequest, QNetworkSession, QNetworkConfigurationManager, QNetworkReply
else:
    # Create dummy classes when running automated tests
    class MayaQWidgetDockableMixin():
        def __init__(self):
            pass
    class QWidget():
        def __init__(self):
            pass

MayaDockableWindow = None
MayaLiveLinkModel = None
NetworkAccessManager = None

# Base class for command (common creator method + allows for automatic
# register/unregister)
class LiveLinkCommand(OpenMayaMPx.MPxCommand):
    def __init__(self):
        OpenMayaMPx.MPxCommand.__init__(self)

    @classmethod
    def Creator(Cls):
        return OpenMayaMPx.asMPxPtr(Cls())


# Is supplied object a live link command
def IsLiveLinkCommand(InCls):
    return (
        inspect.isclass(InCls) and
        issubclass(InCls, LiveLinkCommand) and
        InCls != LiveLinkCommand)

# Given a list of strings of names return all the live link commands listed
def GetLiveLinkCommandsFromModule(ModuleItems):
    EvalItems = (eval(Item) for Item in ModuleItems)
    return [Command for Command in EvalItems if IsLiveLinkCommand(Command)]

class MayaUnrealLiveLinkDockableWindow(MayaQWidgetDockableMixin, QWidget):
    WindowName = 'MayaUnrealLiveLinkDockableWindow'
    WorkspaceControlName = WindowName + 'WorkspaceControl'

    def __init__(self, parent=None):
        super(MayaUnrealLiveLinkDockableWindow, self).__init__(parent=parent)
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        self.setContentsMargins(0, 0, 0, 0)
        self.setLayout(layout)

class MayaUnrealLiveLinkModel():
    DefaultWidth = 650
    DefaultHeight = 450
    DefaultTopLeftCorner = [50, 50]
    UpdateURL = ""
    Unreal4VersionSuffix = '_4_27'
    Unreal5VersionSuffix = '_5_1'

    NativeTypeToUnrealType = {
        'spotLight' : ['SpotLight'],
        'directionalLight' : ['DirectionalLight'],
        'pointLight' : ['PointLight'],
        'mesh' : ['StaticMeshActor'],
        'camera' : ['CameraActor', 'CineCameraActor'],
        'joint': ['Skeleton'],
        'jointRootOnly' : ['SkeletalMeshActor']
        }

    def __init__(self):
        self.Controller = None
        self.UnicastEndpoint = ''
        self.StaticEndpoints = []

        self.initializeNetworkSettings()

    def __del__(self):
        if self.Controller:
            del self.Controller
            self.Controller = None

    def initializeNetworkSettings(self):
        # Make sure the .mll/.so plugin is loaded or we will not be able to set the values to Unreal Engine
        if MayaUnrealLiveLinkModel.getLoadedUnrealVersion() < 5:
            return

        UnicastOption = cmds.optionVar(exists='unrealLLUnicastEndpoint')
        StaticOption  = cmds.optionVar(exists='unrealLLStaticEndpoints')

        if not UnicastOption:
            cmds.optionVar(stringValue=('unrealLLUnicastEndpoint', '0.0.0.0:0'))
        else:
            UnicastSavedEP = cmds.optionVar(query='unrealLLUnicastEndpoint')

            PortIndex = UnicastSavedEP.find(':')

            if PortIndex == -1:
                UnicastSavedEP += ':0'

            cmds.LiveLinkMessagingSettings(UnicastSavedEP, unicastEndpoint=True)

            self.UnicastEndpoint = UnicastSavedEP

        if StaticOption:
            StaticCastSavedEPs = cmds.optionVar(query='unrealLLStaticEndpoints')

            cmds.LiveLinkMessagingSettings(*StaticCastSavedEPs,
                                           staticEndpoints=True,
                                           addEndpoint=True)

            self.StaticEndpoints = StaticCastSavedEPs

    def createController(self, dockableWindow=None, uiWindow=None):
        if self.Controller is None:
            self.Controller = UnrealLiveLinkController(self,
                                                       MayaUnrealLiveLinkDockableWindow.WindowName,
                                                       'Unreal Live Link',
                                                       iconPath,
                                                       dockableWindow if dockableWindow else MayaDockableWindow,
                                                       uiWindow)
        self.Controller.clearUI()
        MayaUnrealLiveLinkSceneManager.loadSettings()

    def loadUISettings(self, windowName):
        w = MayaUnrealLiveLinkModel.DefaultWidth
        h = MayaUnrealLiveLinkModel.DefaultHeight
        tlc = MayaUnrealLiveLinkModel.DefaultTopLeftCorner

        if len(windowName) == 0:
            return tlc, w, h

        ## Check if the window settings already exist
        windowSettingsFound = cmds.windowPref(windowName, q=True, ex=True)

        if windowSettingsFound:
            # Read the window settings
            _w = cmds.windowPref(windowName, q=True, w=True)
            _h = cmds.windowPref(windowName, q=True, h=True)
            tlc = cmds.windowPref(windowName, q=True, tlc=True)
            if _w > 0:
                w = _w
            if _h > 0:
                h = _h
        else:
            cmds.windowPref(windowName)
            cmds.windowPref(windowName, e=True, w=w)
            cmds.windowPref(windowName, e=True, h=h)
            cmds.windowPref(windowName, e=True, tlc=tlc)
            cmds.windowPref(windowName, e=True, max=False)

        # We expect to receive [left, top] corners
        return [tlc[1], tlc[0]], w, h

    @staticmethod
    def getUIGeometry(windowName):
        x = 0
        y = 0
        width = -1
        height = -1
        if windowName and len(windowName) > 0:
            if cmds.optionVar(exists=windowName + '.x'):
                x = cmds.optionVar(q=windowName + '.x')
            if cmds.optionVar(exists=windowName + '.y'):
                y = cmds.optionVar(q=windowName + '.y')
            if cmds.optionVar(exists=windowName + '.width'):
                width = cmds.optionVar(q=windowName + '.width')
            if cmds.optionVar(exists=windowName + '.height'):
                height = cmds.optionVar(q=windowName + '.height')
        return x, y, width, height

    @staticmethod
    def saveUIGeometry(windowName, x, y, width, height):
        if windowName and len(windowName) > 0:
            cmds.optionVar(iv=(windowName + '.x', x))
            cmds.optionVar(iv=(windowName + '.y', y))
            if width > 0:
                cmds.optionVar(iv=(windowName + '.width', width))
            if height > 0:
                cmds.optionVar(iv=(windowName + '.height', height))

    @staticmethod
    def addSelection(modified=True):
        alreadyInList = cmds.LiveLinkAddSelection()
        if len(alreadyInList) > 0:
            if modified:
                cmds.file(modified=True)
            return alreadyInList[0]
        return False

    @staticmethod
    def refreshSubjects():
        try:
            SubjectNames = cmds.LiveLinkSubjectNames()
            SubjectPaths = cmds.LiveLinkSubjectPaths()
            SubjectTypes = cmds.LiveLinkSubjectTypes()
            SubjectRoles = cmds.LiveLinkSubjectRoles()
            SubjectLinkedAssets = cmds.LiveLinkSubjectLinkedAssets()
            SubjectTargetAssets = cmds.LiveLinkSubjectTargetAssets()
            SubjectLinkStatus = cmds.LiveLinkSubjectLinkStatus()
            SubjectClasses = cmds.LiveLinkSubjectClasses()
            SubjectUnrealNativeClasses = cmds.LiveLinkSubjectUnrealNativeClasses()
            return [SubjectNames, SubjectPaths, SubjectTypes, SubjectRoles, SubjectLinkedAssets, SubjectTargetAssets, SubjectLinkStatus, SubjectClasses, SubjectUnrealNativeClasses]
        except:
            return [[], [], [], [], [], [], [], [], []]

    @staticmethod
    def removeSubject(dagPath, modified=True):
        cmds.LiveLinkRemoveSubject(dagPath)
        if modified:
            cmds.file(modified=True)

    @staticmethod
    def changeSubjectName(dagPath, newName, modified=True):
        cmds.LiveLinkChangeSubjectName(dagPath, newName)
        if modified:
            cmds.file(modified=True)

    @staticmethod
    def changeSubjectType(dagPath, newType, modified=True):
        cmds.LiveLinkChangeSubjectStreamType(dagPath, newType)
        if modified:
            cmds.file(modified=True)

    def getNetworkEndpoints(self):
        # Store the current endpoint settings.
        self.UnicastEndpoint = cmds.LiveLinkMessagingSettings(
            q=True, unicastEndpoint=True)
        self.StaticEndpoints = cmds.LiveLinkMessagingSettings(
            q=True, staticEndpoints=True)
        return { 'unicast' : self.UnicastEndpoint,
                 'static' : list(self.StaticEndpoints) }
    
    def setNetworkEndpoints(self, endpointsDict):
        # Apply new endpoint settings if they differ from the current
        # settings.
        NewUnicastEndpoint = endpointsDict['unicast']
        NewStaticEndpoints = endpointsDict['static']

        if NewUnicastEndpoint != self.UnicastEndpoint:
            cmds.LiveLinkMessagingSettings(
                NewUnicastEndpoint, unicastEndpoint=True)
            self.UnicastEndpoint = NewUnicastEndpoint
            cmds.optionVar(stringValue=('unrealLLUnicastEndpoint', NewUnicastEndpoint))

        if NewStaticEndpoints != self.StaticEndpoints:
            RemovedStaticEndpoints = list(
                set(self.StaticEndpoints) - set(NewStaticEndpoints))
            AddedStaticEndpoints = list(
                set(NewStaticEndpoints) - set(self.StaticEndpoints))

            if RemovedStaticEndpoints:
                cmds.LiveLinkMessagingSettings(
                    *RemovedStaticEndpoints, staticEndpoints=True,
                    removeEndpoint=True)
            if AddedStaticEndpoints:
                cmds.LiveLinkMessagingSettings(
                    *AddedStaticEndpoints, staticEndpoints=True,
                    addEndpoint=True)
            self.StaticEndpoints = NewStaticEndpoints

            if cmds.optionVar(exists='unrealLLStaticEndpoints'):
                cmds.optionVar(clearArray='unrealLLStaticEndpoints')
            for StaticEP in NewStaticEndpoints:
                cmds.optionVar(stringValueAppend=('unrealLLStaticEndpoints', StaticEP))

    @staticmethod
    def getDpiScale(size, useConversion):
        dpiScale = omui.MQtUtil.dpiScale(size)
        return int(size + max(0, int((dpiScale - size) * 0.5))) if useConversion else dpiScale

    @staticmethod
    def isDocked():
        if cmds.workspaceControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName, q=True, exists=True) and \
            (not cmds.workspaceControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName, q=True, floating=True)):
            return True
        return False

    def getUpdateURL(self):
        return MayaUnrealLiveLinkModel.UpdateURL

    @staticmethod
    def getDocumentationURL():
        url = cmds.LiveLinkGetPluginDocumentationUrl()
        if url and isinstance(url, list) and len(url) > 0:
            return url[0]
        return ''

    @staticmethod
    def getPluginVersion():
        version = cmds.LiveLinkGetPluginVersion()
        app_version = ''
        if version and len(version) > 0:
            app_version = version[0]
        return app_version

    @staticmethod
    def getApiVersion():
        return str(cmds.about(apiVersion=True))

    @staticmethod
    def getUnrealVersion():
        version = cmds.LiveLinkGetUnrealVersion()
        app_version = ''
        if version and len(version) > 0:
            app_version = version[0]
        return app_version

    @staticmethod
    def getAboutText():
        filePath = basePath + '/../../docs/copyright.txt'
        if not os.path.exists(filePath):
            filePath = basePath + '/../../../resource/copyright.txt'
            if not os.path.exists(filePath):
                return ''
        try:
            with io.open(filePath, 'r', encoding='cp1252') as fr:
                text = fr.read()
                fr.close()
                if text and len(text) > 0:
                    return text
        except:
            print('Unable to read ' + filePath)
        return ''

    @staticmethod
    def handleVersionResponse(reply):
        er = reply.error()
        if er == QNetworkReply.NoError:
            # Get the plugin version
            app_version = MayaUnrealLiveLinkModel.getPluginVersion()

            # Get the plugin appstore id
            id = cmds.LiveLinkGetPluginAppId()
            app_id = ''
            if id and len(id) > 0:
                app_id = id[0]

            if len(app_version) == 0 or len(app_id) == 0:
                return

            # Read the network request and convert it to json for easier decoding
            bytes_string = reply.readAll()
            jsonResponse = QJsonDocument.fromJson(bytes_string)
            apps = jsonResponse.array()
            for i in range(apps.size()):
                app = apps.at(i).toObject()
                if 'app_id' in app:
                    # Check if the app id is our plugin's id
                    if app['app_id'] == app_id and 'app_version' in app:
                        version = app['app_version']
                        if not isinstance(version, str):
                            version = str(version)
                        versionSplit = version.split('.')
                        app_versionSplit = app_version.split('.')
                        minLen = min(len(versionSplit), len(app_versionSplit))

                        # Compare the versions to determine if the plugin from the appstore is newer
                        newer = False
                        for i in range(minLen):
                            try:
                                versionSplit[i] = int(versionSplit[i])
                                app_versionSplit[i] = int(app_versionSplit[i])
                                if versionSplit[i] != app_versionSplit[i]:
                                    if versionSplit[i] > app_versionSplit[i]:
                                        newer = True
                                    break
                            except:
                                pass

                        if newer or len(versionSplit) > len(app_versionSplit):
                            # If newer, get the plugin url on the appstore
                            url = cmds.LiveLinkGetPluginUpdateUrl()
                            if url and len(url) > 0:
                                MayaUnrealLiveLinkModel.UpdateURL = url[0]
                                if MayaLiveLinkModel and MayaLiveLinkModel.Controller:
                                    # Notify the UI there's an update
                                    MayaLiveLinkModel.Controller.notifyNewerUpdate()
                        break
        else:
            print("Error occured: ", er)
            print(reply.errorString())

    @staticmethod
    def NotifyPluginReloadAndQuit(currPluginName, prevPluginName):
        # Retrieve the loaded status for the current and previous plugin
        prevPluginLoaded = cmds.pluginInfo(prevPluginName, q=True, loaded=True)
        currPluginLoaded = cmds.pluginInfo(currPluginName, q=True, loaded=True)

        # Check if we need to restart Maya
        if prevPluginLoaded:
            # Notify that Maya needs to be restarted to use the current plugin
            restartMayaString = 'Restart Maya'
            cancelString = 'Cancel'
            buttonString = cmds.confirmDialog(title='Maya restart required',
                                                message='Maya needs to restart to change the Unreal\nEngine version used by the Live Link plugin.',
                                                button=[restartMayaString, cancelString],
                                                defaultButton=restartMayaString,
                                                cancelButton=cancelString,
                                                dismissString=cancelString)

            # The user accepts to quit Maya to reload the other plugin
            if buttonString == restartMayaString:
                # Get the plugin path for previous plugin and remove the plugin name
                prevPluginPath = cmds.pluginInfo(prevPluginName, q=True, path=True)
                prevPluginPath = "/".join(prevPluginPath.split("/")[:-1])

                # Current plugin must be in the same path as previous one.
                # We set a plugin path for current plugin
                currPluginPath = ""
                pluginWithExtension = ""
                if cmds.about(q=True, linux=True):
                    pluginWithExtension = currPluginName + ".so"
                    currPluginPath = os.path.join(prevPluginPath, pluginWithExtension)
                elif cmds.about(q=True, windows=True):
                    pluginWithExtension = currPluginName + ".mll"
                    currPluginPath = os.path.join(prevPluginPath, pluginWithExtension)

                # Disable autoload for the previous plugin
                if prevPluginLoaded and cmds.pluginInfo(prevPluginName, q=True, autoload=True):
                    cmds.pluginInfo(prevPluginName, e=True, autoload=False)
                cmds.pluginInfo(prevPluginName, e=True, loaded=False)

                # Enable autoload for the current plugin
                if not cmds.pluginInfo(currPluginName, q=True, autoload=True) and len(currPluginPath)>0:
                    # Autoload current plugin using full path or it will fail
                    if os.path.exists(currPluginPath):
                        cmds.pluginInfo(currPluginPath, e=True, autoload=True)
                    else:
                        # If the plugin for the other Unreal version is not found in the same path,
                        # go through the paths defined in MAYA_PLUG_IN_PATH
                        if 'MAYA_PLUG_IN_PATH' in os.environ:
                            mayaPluginPath = os.environ['MAYA_PLUG_IN_PATH']
                            mayaPluginPaths = mayaPluginPath.split(';')
                            for path in mayaPluginPaths:
                                currPluginPath = os.path.join(path, pluginWithExtension)
                                if os.path.exists(currPluginPath):
                                    cmds.pluginInfo(currPluginPath, e=True, autoload=True)
                                    break

                # Save the autoload preferences
                cmds.pluginInfo(savePluginPrefs=True)

                # Quit Maya
                cmds.LiveLinkOnQuit()
                cmds.quit()

                # Start a new process that will restart Maya using the same commandline arguments
                process = QProcess()
                if not process.startDetached(sys.argv[0], sys.argv[1:]):
                    # argv[0] is not a full path, try starting the process only using the filename
                    pathSplit = os.path.split(sys.argv[0])
                    process.startDetached(pathSplit[len(pathSplit) - 1], sys.argv[1:])
                return True
            else:
                if cmds.pluginInfo(currPluginName, q=True, loaded=True):
                    cmds.unloadPlugin(currPluginName)

            return False

    @staticmethod
    def getPluginPrefix():
        pluginName = 'MayaUnrealLiveLinkPlugin'
        if cmds.about(q=True, linux=True):
            pluginName = 'lib' + pluginName
        return pluginName

    @staticmethod
    def findLoadedUnrealVersion():
        pluginName = MayaUnrealLiveLinkModel.getPluginPrefix()

        # List the loaded plugins to find if another Live Link plugin is already loaded
        # If it's the case, we will force it not to load the next time Maya is started and
        # auto-load the other version instead
        loadedPlugins = cmds.pluginInfo(q=True, listPlugins=True)
        for plugin in loadedPlugins:
            if plugin.startswith(pluginName):
                pluginVersionSplit = plugin.split('_')
                if len(pluginVersionSplit) == 3:
                    return plugin, pluginVersionSplit
        return "", []

    @staticmethod
    def setLoadedUnrealVersion(unrealVersionNumber):
        plugin, pluginVersionSplit = MayaUnrealLiveLinkModel.findLoadedUnrealVersion()

        # Found a loaded plugin, check to see if it's different from the version we want to load
        if len(plugin) > 0 and len(pluginVersionSplit) == 3:
            ueMajorVersion = pluginVersionSplit[1]
            if ueMajorVersion.isnumeric() and int(ueMajorVersion) != unrealVersionNumber:
                pluginToAutoLoad = pluginVersionSplit[0]
                if unrealVersionNumber == 4:
                    pluginToAutoLoad += MayaUnrealLiveLinkModel.Unreal4VersionSuffix
                else:
                    pluginToAutoLoad += MayaUnrealLiveLinkModel.Unreal5VersionSuffix

                return MayaUnrealLiveLinkModel.NotifyPluginReloadAndQuit(pluginToAutoLoad, plugin)
            return True
        else:
            # With no plugin loaded, we can load the UE version selected by the user
            try:
                pluginName = MayaUnrealLiveLinkModel.getPluginPrefix()

                # Pick the right version
                if unrealVersionNumber == 4:
                    pluginName += MayaUnrealLiveLinkModel.Unreal4VersionSuffix
                else:
                    pluginName += MayaUnrealLiveLinkModel.Unreal5VersionSuffix

                # Load the plugin
                if not cmds.pluginInfo(pluginName, q=True, loaded=True):
                    cmds.loadPlugin(pluginName)

                # Set autoload to true to remember loading this version the next time Maya is started
                if not cmds.pluginInfo(pluginName, q=True, autoload=True):
                    cmds.pluginInfo(pluginName, e=True, autoload=True)
                    cmds.pluginInfo(savePluginPrefs=True)

                return True
            except:
                print('Exception while trying to load plugin ' + pluginName)
        return False

    @staticmethod
    def getLoadedUnrealVersion():
        plugin, pluginVersionSplit = MayaUnrealLiveLinkModel.findLoadedUnrealVersion()
        if len(plugin) > 0 and len(pluginVersionSplit) == 3:
            return int(pluginVersionSplit[1]) if pluginVersionSplit[1].isnumeric() else -1
        return -1

    @staticmethod
    def connectionMade():
        try:
            cmds.MayaUnrealLiveLinkConnectionMade()
        except:
            pass

    @staticmethod
    def getNodeType(dagPath, isJoint, isJointRootOnly):
        nodeType = ''
        if isJoint:
            nodeType = 'joint'
        elif isJointRootOnly:
            nodeType = 'jointRootOnly'
        else:
            shapes = cmds.listRelatives(dagPath, ad=True, f=True)
            nodePath = ""
            if len(shapes) > 0:
                nodePath = shapes[0]
            else:
                nodePath = dagPath
            nodeType = cmds.nodeType(nodePath)
        return nodeType

    @staticmethod
    def getLinkedAssets(nodeType, levelAssets):
        nativeClasses = []
        if nodeType in MayaUnrealLiveLinkModel.NativeTypeToUnrealType:
            nativeClasses = MayaUnrealLiveLinkModel.NativeTypeToUnrealType[nodeType]
        else:
            nativeClasses.append(['Actor'])
            levelAssets = True

        if levelAssets:
            assetClasses = MayaUnrealLiveLinkModel.convertListToDict(cmds.LiveLinkGetActorsByClass(nativeClasses[0]))
        else:
            assetClasses = MayaUnrealLiveLinkModel.getTargetAssets(nativeClasses[0])

        return nativeClasses, assetClasses if assetClasses else None

    @staticmethod
    def getAnimSequencesBySkeleton():
        return MayaUnrealLiveLinkModel.convertListToDict(cmds.LiveLinkGetAnimSequencesBySkeleton())

    @staticmethod
    def convertListToDict(listToConvert):
        convertedToDict = dict()

        if listToConvert is None or (not isinstance(listToConvert, list)):
            return convertedToDict

        if len(listToConvert) == 1 and listToConvert[0] == 'Timeout!':
            return None

        i = 0
        listToConvertLen = len(listToConvert)
        while i < listToConvertLen:
            key = listToConvert[i]
            startIndex = listToConvert[i + 1]
            numberElements = listToConvert[i + 2]
            i += 3
            convertedToDict[key] = []
            for j in range(i, numberElements + i):
                convertedToDict[key].append(listToConvert[j])
            i += len(convertedToDict[key])

        return convertedToDict

    @staticmethod
    def getTargetAssets(assetClass):
        return MayaUnrealLiveLinkModel.convertListToDict(cmds.LiveLinkGetAssetsByClass(assetClass, True))

    @staticmethod
    def getAssetsFromParentClass(className, parentClass):
        assets = cmds.LiveLinkGetAssetsByParentClass(className, True, ",".join(parentClass))
        if assets:
            try:
                separatorIndex = assets.index('|')
                if separatorIndex >= 0:
                    return assets[:separatorIndex], assets[separatorIndex + 1:]
            except:
                return assets, None
        return None, None

    @staticmethod
    def linkAsset(subjectPath, linkedAssetPath, linkedAssetClass, targetAssetPath, targetAssetName, linkedAssetNativeClass, setupOnly=False, modified=True):
        cmds.LiveLinkLinkUnrealAsset(subjectPath,
                                     linkedAssetPath,
                                     linkedAssetClass,
                                     targetAssetPath,
                                     targetAssetName,
                                     linkedAssetNativeClass,
                                     setupOnly)
        if modified and (not setupOnly):
            cmds.file(modified=True)

    @staticmethod
    def unlinkAsset(subjectPath):
        try:
            cmds.LiveLinkUnlinkUnrealAsset(subjectPath)
        except:
            pass

    @staticmethod
    def isPlayheadSyncEnabled():
        try:
            return cmds.LiveLinkPlayheadSync(q=True, enable=True)
        except:
            return True


    @staticmethod
    def enablePlayheadSync(state):
        try:
            cmds.LiveLinkPlayheadSync(enable=state)
        except:
            pass

    @staticmethod
    def pauseAnimSeqSync(state):
        try:
            cmds.LiveLinkPauseAnimSync(enable=state)
        except:
            pass

def checkForNewerVersion():
    # Check to see if there is an update for the plugin
    if not IsAutomatedTest:
        try:
            if len(MayaUnrealLiveLinkModel.UpdateURL) > 0:
                if MayaLiveLinkModel and MayaLiveLinkModel.Controller:
                    # Notify the UI there's an update
                    MayaLiveLinkModel.Controller.notifyNewerUpdate()
            else:
                id = cmds.LiveLinkGetPluginAppId()
                if id and len(id) > 0 and len(id[0]) > 0:
                    global NetworkAccessManager
                    NetworkAccessManager = QNetworkAccessManager()
                    NetworkAccessManager.finished[QNetworkReply].connect(MayaUnrealLiveLinkModel.handleVersionResponse)
                    url = cmds.LiveLinkGetPluginRequestUrl()
                    if url and len(url) > 0:
                        NetworkAccessManager.get(QNetworkRequest(QUrl(url[0])))
        except:
            pass

def restoreModelFromWorkspaceControl():
    if IsAutomatedTest:
        return

    if cmds.workspaceControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName, q=True, exists=True):
        deleteControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName)
        cmd = MayaUnrealLiveLinkUI()
        cmd.doIt([])

def ShowUI(restore=False):
    if IsAutomatedTest:
        return

    global MayaDockableWindow
    global MayaLiveLinkModel

    # Get the dockable window
    mixinPtr = omui.MQtUtil.findControl(MayaUnrealLiveLinkDockableWindow.WindowName)
  
    if MayaDockableWindow is None:
        # Create a custom mixin widget for the first time
        MayaDockableWindow = MayaUnrealLiveLinkDockableWindow()
        MayaDockableWindow.setObjectName(MayaUnrealLiveLinkDockableWindow.WindowName)
        MayaDockableWindow.setProperty("saveWindowPref", True )

    if MayaLiveLinkModel is None:
        MayaLiveLinkModel = MayaUnrealLiveLinkModel()
        MayaLiveLinkModel.createController()

    if restore == True:
        # When the control is restoring, the workspace control has already been created and
        # all that needs to be done is restoring its UI.
        restoredControl = None
        # Grab the created workspace control with the following.
        restoredControl = omui.MQtUtil.getCurrentParent()
        # Add custom mixin widget to the workspace control
        mixinPtr = omui.MQtUtil.findControl(MayaUnrealLiveLinkDockableWindow.WindowName)
        if (sys.version_info[0] >= 3):
            omui.MQtUtil.addWidgetToMayaLayout(int(mixinPtr), int(restoredControl))
        else:
            omui.MQtUtil.addWidgetToMayaLayout(long(mixinPtr), long(restoredControl))
        if MayaLiveLinkModel and MayaLiveLinkModel.Controller:
            MayaLiveLinkModel.Controller.refreshUI()
    else:
        # Create a workspace control for the mixin widget by passing all the needed parameters. See workspaceControl command documentation for all available flags.
        script = '''
import sys
sys.dont_write_bytecode = True
if ("{0}" in sys.path) is False:
    sys.path.append("{0}")
import MayaUnrealLiveLinkPluginUI
MayaUnrealLiveLinkPluginUI.ShowUI(restore=True)'''.format(basePath)
        MayaDockableWindow.setGeometry(0, 0, MayaUnrealLiveLinkModel.DefaultWidth, MayaUnrealLiveLinkModel.DefaultHeight)
        MayaDockableWindow.move(MayaUnrealLiveLinkModel.DefaultTopLeftCorner[0], MayaUnrealLiveLinkModel.DefaultTopLeftCorner[1])
        # Plugins argument here defines the dependency of UI on a plugin
        MayaDockableWindow.show(dockable=True, plugins='MayaUnrealLiveLinkPluginUI', uiScript=script)
        if cmds.workspaceControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName, q=True, exists=True):
            cmds.workspaceControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName, e=True, cp=True, rp='MayaUnrealLiveLinkPluginUI')

# Command to create the Live Link UI
class MayaUnrealLiveLinkUI(LiveLinkCommand):
    def __init__(self):
        LiveLinkCommand.__init__(self)

    # Invoked when the command is run.
    def doIt(self, argList):
        if IsAutomatedTest:
            return

        if cmds.workspaceControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName, q=True, exists=True) and \
           (not MayaDockableWindow):
            deleteControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName)

        if not cmds.workspaceControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName, q=True, exists=True):
            ShowUI()

        if MayaDockableWindow:
            if not cmds.workspaceControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName, q=True, visible=True):
                MayaDockableWindow.show(dockable=True)

        if MayaLiveLinkModel and MayaLiveLinkModel.Controller:
            MayaLiveLinkModel.Controller.refreshUI()
            try:
                ConnectionText, ConnectedState = cmds.LiveLinkConnectionStatus()
                MayaLiveLinkModel.Controller.updateConnectionState(ConnectionText, ConnectedState)
            except:
                pass
            onSelectionChanged(None, None)

# Command called when the .mll/.so plugin is initialzed
class MayaUnrealLiveLinkInitialized(LiveLinkCommand):
    def __init__(self):
        LiveLinkCommand.__init__(self)

    # Invoked when the command is run.
    def doIt(self, argList):
        if MayaLiveLinkModel:
            # Update the network settings if the UI was loaded before the plugin
            MayaLiveLinkModel.initializeNetworkSettings()

            if MayaLiveLinkModel.Controller:
                # Make sure the controls are enabled when the UI is initialized before the .mll/.so file
                validUnrealVersion = MayaLiveLinkModel.Controller.getLoadedUnrealVersion() != -1
                MayaLiveLinkModel.Controller.enableControls(validUnrealVersion)

        MayaUnrealLiveLinkSceneManager.loadSettings()

        checkForNewerVersion()

# Command to Refresh the subject UI
class MayaUnrealLiveLinkRefreshUI(LiveLinkCommand):
    def __init__(self):
        LiveLinkCommand.__init__(self)

    # Invoked when the command is run.
    def doIt(self, argList):
        if MayaLiveLinkModel and MayaLiveLinkModel.Controller:
            MayaLiveLinkModel.Controller.clearUI()
            MayaLiveLinkModel.Controller.refreshUI()

# Command to Refresh the connection UI
class MayaUnrealLiveLinkRefreshConnectionUI(LiveLinkCommand):
    def __init__(self):
        LiveLinkCommand.__init__(self)

    # Invoked when the command is run.
    def doIt(self, argList):
        if MayaLiveLinkModel and MayaLiveLinkModel.Controller:
            # Get current connection status
            try:
                ConnectionText, ConnectedState = cmds.LiveLinkConnectionStatus()
                MayaLiveLinkModel.Controller.updateConnectionState(ConnectionText, ConnectedState)
            except:
                pass

# Command to notify the user that Maya needs to be restarted to use a different Unreal version of the plugin
class MayaLiveLinkNotifyAndQuit(LiveLinkCommand):
    def __init__(self):
        LiveLinkCommand.__init__(self)

    # Invoked when the command is run.
    def doIt(self, argList):
        if argList and argList.length() > 1:
            currPluginName = argList.asString(0)
            prevPluginName = argList.asString(1)
            MayaUnrealLiveLinkModel.NotifyPluginReloadAndQuit(currPluginName, prevPluginName)

# Command to update the link progress bar
class MayaUnrealLiveLinkUpdateLinkProgress(LiveLinkCommand):
    def __init__(self):
        LiveLinkCommand.__init__(self)

    # Invoked when the command is run.
    def doIt(self, argList):
        if MayaDockableWindow and argList.length() > 0 and MayaLiveLinkModel and MayaLiveLinkModel.Controller:
            if cmds.workspaceControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName, q=True, visible=True):
                # set the link progress bar
                MayaLiveLinkModel.Controller.setLinkProgress(argList.asInt(0), True)

class MayaUnrealLiveLinkSceneManager():
    Name = 'MayaUnrealLiveLinkSceneManager'
    SubjectListName = 'SubjectList'
    SettingsName = 'MayaUnrealLiveLinkSettings'

    @staticmethod
    def loadSettings():
        fileOpened = cmds.file(q=True, sn=True)
        if fileOpened is None or len(fileOpened) == 0:
            return

        if MayaLiveLinkModel is None or MayaLiveLinkModel.Controller is None:
            return

        subjects = MayaUnrealLiveLinkSceneManager.__getSceneSubjectList()
        if subjects is not None and len(subjects) > 0:
            subjectList = subjects.replace('\\', '')
            try:
                # Convert the json string to dict
                subjectDict = json.loads(subjectList)
                if subjectDict and isinstance(subjectDict, dict):
                    for subject in subjectDict:
                        if cmds.objExists(subject):
                            cmds.select(subject)
                            MayaUnrealLiveLinkModel.addSelection(False)
                            if 'name' in subjectDict[subject]:
                                MayaUnrealLiveLinkModel.changeSubjectName(subject, subjectDict[subject]['name'], False)
                            if 'type' in subjectDict[subject]:
                                MayaUnrealLiveLinkModel.changeSubjectType(subject, subjectDict[subject]['type'], False)

                    MayaLiveLinkModel.Controller.refreshUI()

                    # Restore link information
                    for subject in subjectDict:
                        if 'linkedAsset' in subjectDict[subject] and 'targetSequence' in subjectDict[subject]:
                            linkedAssetFullName = subjectDict[subject]['linkedAsset']
                            targetSequenceFullName = subjectDict[subject]['targetSequence']
                            if len(linkedAssetFullName) > 0 and len(targetSequenceFullName) > 0:
                                nameIndex = targetSequenceFullName.rfind('/')
                                if nameIndex >= 0:
                                    targetPath = targetSequenceFullName[:nameIndex]
                                    targetName = targetSequenceFullName[nameIndex+1:]

                                    # Update the UI link info
                                    MayaLiveLinkModel.Controller.updateUILinkInfo(subject,
                                                                                  linkedAssetFullName,
                                                                                  targetPath,
                                                                                  targetName,
                                                                                  subjectDict[subject]['class'] if 'class' in subjectDict[subject] else '',
                                                                                  subjectDict[subject]['unrealNativeClass'] if 'unrealNativeClass' in subjectDict[subject] else '')

                                    # Setup the linked asset in the plugin without actually perform the link operation
                                    MayaLiveLinkModel.linkAsset(subject,
                                                                linkedAssetFullName,
                                                                subjectDict[subject]['class'] if 'class' in subjectDict[subject] else '',
                                                                targetPath,
                                                                targetName,
                                                                subjectDict[subject]['unrealNativeClass'] if 'unrealNativeClass' in subjectDict[subject] else '',
                                                                True,
                                                                False)
            except:
                pass

    @staticmethod
    def saveSettings():
        MayaUnrealLiveLinkSceneManager.__deleteFileInfo()

        addedSelections = MayaUnrealLiveLinkModel.refreshSubjects()
        if len(addedSelections) == 9 and all(addedSelections):
            subjectList = dict()
            for (SubjectName, SubjectPath, SubjectRole, SubjectType, LinkedAsset, TargetAsset, LinkStatus, Class, UnrealNativeClass) in zip(
                 addedSelections[0], addedSelections[1], addedSelections[2], addedSelections[3], addedSelections[4], addedSelections[5], addedSelections[6], addedSelections[7], addedSelections[8]):
                MayaUnrealLiveLinkSceneManager.__addSubject(SubjectPath, SubjectName, SubjectType, LinkedAsset, TargetAsset, Class, UnrealNativeClass, subjectList)

            try:
                jsonDumps = json.dumps(subjectList)
                cmds.scriptNode(n=MayaUnrealLiveLinkSceneManager.Name)
                cmds.select(MayaUnrealLiveLinkSceneManager.Name)
                attribName = MayaUnrealLiveLinkSceneManager.SubjectListName
                cmds.addAttr(longName=attribName, dt='string')
                cmds.setAttr(MayaUnrealLiveLinkSceneManager.Name + '.' + attribName, jsonDumps, type='string', l=True)
                cmds.select(None)
            except:
                pass

    @staticmethod
    def __getSceneSubjectList():
        if cmds.objExists(MayaUnrealLiveLinkSceneManager.Name):
            if cmds.attributeQuery(MayaUnrealLiveLinkSceneManager.SubjectListName,
                                   node=MayaUnrealLiveLinkSceneManager.Name,
                                   exists=True):
                attribName = MayaUnrealLiveLinkSceneManager.Name + '.' + MayaUnrealLiveLinkSceneManager.SubjectListName
                subjectList = cmds.getAttr(attribName)
                if subjectList and len(subjectList) > 0:
                    return subjectList
        return None

    @staticmethod
    def __addSubject(dagPath, name, type, linkedAsset, targetSequence, unrealClass, unrealNativeClass, subjectList):
        if isinstance(subjectList, dict):
            if dagPath not in subjectList:
                # Add the subject
                subjectList[dagPath] = dict()

            # Update the subject's values
            subjectList[dagPath]['name'] = name
            subjectList[dagPath]['type'] = type
            if linkedAsset and len(linkedAsset) > 0 and targetSequence and len(targetSequence) > 0:
                subjectList[dagPath]['linkedAsset'] = linkedAsset
                subjectList[dagPath]['targetSequence'] = targetSequence
                subjectList[dagPath]['class'] = unrealClass
                subjectList[dagPath]['unrealNativeClass'] = unrealNativeClass

    @staticmethod
    def __deleteFileInfo():
        if cmds.objExists(MayaUnrealLiveLinkSceneManager.Name):
            cmds.delete(MayaUnrealLiveLinkSceneManager.Name)

class MayaUnrealLiveLinkOnSceneOpen(LiveLinkCommand):
    def __init__(self):
        LiveLinkCommand.__init__(self)

    # Invoked when the command is run.
    def doIt(self, argList):
        # If a file is already opened and the plugin just got loaded, load the settings to restore subjects that were in the UI
        MayaUnrealLiveLinkSceneManager.loadSettings()

class MayaUnrealLiveLinkOnScenePreSave(LiveLinkCommand):
    def __init__(self):
        LiveLinkCommand.__init__(self)

    # Invoked when the command is run.
    def doIt(self, argList):
        MayaUnrealLiveLinkSceneManager.saveSettings()

# Grab commands declared
Commands = GetLiveLinkCommandsFromModule(dir())

AfterPluginUnloadCallbackId = None
SelectionChangedCallbackId = None

def AfterPluginUnloadCallback(stringArray, clientData):
    for stringVal in stringArray:
        if MayaLiveLinkModel and MayaLiveLinkModel.Controller and stringVal.find('MayaUnrealLiveLinkPlugin'):
            MayaLiveLinkModel.Controller.clearUI()
            MayaLiveLinkModel.Controller.refreshSubjects()
            return

def deleteControl(control):
    if cmds.workspaceControl(control, q=True, exists=True):
        cmds.workspaceControl(control,e=True, close=True)
        cmds.deleteUI(control, control=True)

    global MayaLiveLinkModel
    if MayaLiveLinkModel:
        del MayaLiveLinkModel
        MayaLiveLinkModel = None

    global MayaDockableWindow
    if MayaDockableWindow:
        del MayaDockableWindow
        MayaDockableWindow = None

def onSelectionChanged(*args, **kwargs):
    # Check if the user has selected a dag node that can be used as a subject
    selectedNodes = cmds.ls(selection=True, type='dagNode')

    # Notify the UI to allow or not the selection of this node
    if MayaLiveLinkModel and MayaLiveLinkModel.Controller:
       MayaLiveLinkModel.Controller.selectedObject(len(selectedNodes) != 0)

# Initialize the script plug-in
def initializePlugin(mobject):
    mplugin = OpenMayaMPx.MFnPlugin(mobject, 'Autodesk, Inc.', 'v2.1.0')

    print("LiveLinkUI Init:")
    for Command in Commands:
        try:
            print("\tRegistering Command '%s'" % Command.__name__)
            mplugin.registerCommand(Command.__name__, Command.Creator)

        except Exception:
            sys.stderr.write(
                "Failed to register command: %s\n" % Command.__name__)
            raise

    if not cmds.about(batch=True):
       mel.eval("eval(\"source MayaUnrealLiveLinkPluginMenu.mel;\")")
       mel.eval('callbacks -addCallback \"AddMayaUnrealLiveLinkMenuItems\" -hook \"addItemToFileMenu\" -owner \"MayaUnrealLiveLinkPluginUI\"')

    restoreModelFromWorkspaceControl()

    global AfterPluginUnloadCallbackId
    AfterPluginUnloadCallbackId = \
        OpenMaya.MSceneMessage.addStringArrayCallback(
            OpenMaya.MSceneMessage.kAfterPluginUnload,
            AfterPluginUnloadCallback)

    global SelectionChangedCallbackId
    SelectionChangedCallbackId = OpenMaya.MEventMessage.addEventCallback("SelectionChanged", onSelectionChanged)

    # If a file is already opened and the plugin just got loaded, load the settings to restore subjects that were in the UI
    MayaUnrealLiveLinkSceneManager.loadSettings()

    checkForNewerVersion()

# Uninitialize the script plug-in
def uninitializePlugin(mobject):
    mplugin = OpenMayaMPx.MFnPlugin(mobject)

    try:
        cmds.LiveLinkPluginUninitialized()
    except:
        pass

    global AfterPluginUnloadCallbackId
    if AfterPluginUnloadCallbackId is not None:
        OpenMaya.MSceneMessage.removeCallback(AfterPluginUnloadCallbackId)
        AfterPluginUnloadCallbackId = None

    global SelectionChangedCallbackId
    if SelectionChangedCallbackId:
        OpenMaya.MMessage.removeCallback(SelectionChangedCallbackId)
        SelectionChangedCallbackId = None

    if not cmds.about(batch=True):
        mel.eval('callbacks -removeCallback \"AddMayaUnrealLiveLinkMenuItems\" -hook \"addItemToFileMenu\" -owner \"MayaUnrealLiveLinkPluginUI\"');
        mel.eval("eval(\"source MayaUnrealLiveLinkPluginMenu.mel; RemoveMayaUnrealLiveLinkMenuItems;\")")
 
    for Command in Commands:
        try:
            mplugin.deregisterCommand(Command.__name__)
        except Exception:
            sys.stderr.write(
                "Failed to unregister command: %s\n" % Command.__name__)

    if cmds.workspaceControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName, q=True, ex=True):
        deleteControl(MayaUnrealLiveLinkDockableWindow.WorkspaceControlName)

    NetworkAccessManager = None
