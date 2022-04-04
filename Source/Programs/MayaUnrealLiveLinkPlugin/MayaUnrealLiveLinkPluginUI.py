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
    Unreal5VersionSuffix = '_5_0'

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
        if MayaUnrealLiveLinkModel.getLoadedUnrealVersion() < 4:
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

    def loadUISettings(self):
        w = MayaUnrealLiveLinkModel.DefaultWidth
        h = MayaUnrealLiveLinkModel.DefaultHeight
        tlc = MayaUnrealLiveLinkModel.DefaultTopLeftCorner
        windowName = MayaUnrealLiveLinkDockableWindow.WindowName

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
    def addSelection():
        alreadyInList = cmds.LiveLinkAddSelection()
        return alreadyInList[0] if len(alreadyInList) > 0 else False

    @staticmethod
    def refreshSubjects():
        try:
            SubjectNames = cmds.LiveLinkSubjectNames()
            SubjectPaths = cmds.LiveLinkSubjectPaths()
            SubjectTypes = cmds.LiveLinkSubjectTypes()
            SubjectRoles = cmds.LiveLinkSubjectRoles()
            return [SubjectNames, SubjectPaths, SubjectTypes, SubjectRoles]
        except:
            return [[], [], [], []]

    @staticmethod
    def removeSubject(dagPath):
        cmds.LiveLinkRemoveSubject(dagPath)

    @staticmethod
    def changeSubjectName(dagPath, newName):
        cmds.LiveLinkChangeSubjectName(dagPath, newName)

    @staticmethod
    def changeSubjectRole(dagPath, newRole):
        cmds.LiveLinkChangeSubjectStreamType(dagPath, newRole)

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
    def getDpiScale(size):
        return omui.MQtUtil.dpiScale(size)

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

# Command called when the .mll/.so plugin is initialzed
class MayaUnrealLiveLinkInitialized(LiveLinkCommand):
    def __init__(self):
        LiveLinkCommand.__init__(self)

    # Invoked when the command is run.
    def doIt(self, argList):
        if MayaLiveLinkModel:
            # Update the network settings if the UI was loaded before the plugin
            MayaLiveLinkModel.initializeNetworkSettings()

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


# Grab commands declared
Commands = GetLiveLinkCommandsFromModule(dir())

AfterPluginUnloadCallbackId = None

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

# Initialize the script plug-in
def initializePlugin(mobject):
    mplugin = OpenMayaMPx.MFnPlugin(mobject)

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

    if not IsAutomatedTest:
        try:
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

# Uninitialize the script plug-in
def uninitializePlugin(mobject):
    mplugin = OpenMayaMPx.MFnPlugin(mobject)

    global AfterPluginUnloadCallbackId
    if AfterPluginUnloadCallbackId is not None:
        OpenMaya.MSceneMessage.removeCallback(AfterPluginUnloadCallbackId)
        AfterPluginUnloadCallbackId = None

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
