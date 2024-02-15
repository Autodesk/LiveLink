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

import os

try:
  from PySide2.QtCore import *
  from PySide2.QtGui import *
  from PySide2.QtWidgets import *
  from PySide2 import __version__
  import PySide2.QtCore
except ImportError:
  from PySide.QtCore import *
  from PySide.QtGui import *
  from PySide import __version__
  import PySide.QtCore

from UnrealLiveLinkSubjectTable import *
from UnrealLiveLinkSettings import *
from UnrealLiveLinkAboutDialog import *

from HoverButton import HoverButton
from ImageUtils import ImageUtils

class UnrealLiveLinkWindow(QWidget):
    PushButtonStyleSheet = '''QPushButton { border: 1px }
                              QPushButton:hover:pressed { background-color: rgb(48, 48, 48); }'''

    def __init__(self, controller, windowName, windowTitle, parent):
        self.mainLayout = None

        super(UnrealLiveLinkWindow, self).__init__()
        if parent:
            if parent.layout():
                parent.layout().addWidget(self)
        else:
            self.setWindowFlags(Qt.Window)
            self.setObjectName(windowName if windowName else 'UnrealLiveLinkWindow')

        self._properties = dict()
        self.ConnectedState = False
        self.Controller = weakref.proxy(controller)
        self.ConnectedPicture = None
        self.DisconnectedPicture = None

        if parent and windowTitle:
            parent.setWindowTitle(windowTitle)

    def __del__(self):
        self.Controller = None

    def openHelpUrl(self):
        docUrl = self.Controller.getDocumentationURL()
        if docUrl and (isinstance(docUrl, str) or isinstance(docUrl, unicode)) and len(docUrl) > 0:
            url = QUrl(docUrl)
            if QDesktopServices.openUrl(url):
                return
        QMessageBox.warning(self, 'Open Url', 'Could not open ' + docUrl)

    def openReportProblemUrl(self):
        url = QUrl('https://www.autodesk.com/company/contact-us/product-feedback')
        if not QDesktopServices.openUrl(url):
            QMessageBox.warning(self, 'Open Url', 'Could not open ' + url.url())

    def openUnrealLiveLinkForumUrl(self):
        url = QUrl('https://forums.autodesk.com/t5/unreal-live-link-for-maya-forum/bd-p/6143')
        if not QDesktopServices.openUrl(url):
            QMessageBox.warning(self, 'Open Url', 'Could not open ' + url.url())

    def openAboutBox(self):
        pluginVersion = self.Controller.getPluginVersion()
        apiVersion = self.Controller.getApiVersion()
        unrealVersion = self.Controller.getUnrealVersion()
        About = UnrealLiveLinkAboutDialog(pluginVersion, apiVersion, unrealVersion, self)
        About.setAboutText(self.Controller.getAboutText())
        About.exec_()

    def initUI(self):
        if self.mainLayout:
            return

        self.ConnectedPicture = ImageUtils.createIcon('infoConnected')
        self.DisconnectedPicture = ImageUtils.createIcon('infoDisconnected')
        helpPicture = ImageUtils.createIcon('help')
        aboutPicture = ImageUtils.createIcon('about')
        settingsPicture = ImageUtils.createIcon('settings')

        self.setFocusPolicy(Qt.StrongFocus)

        self.mainLayout = QVBoxLayout()
        self.mainLayout.setContentsMargins(7, 0, 7, 7)
        self.mainLayout.setSpacing(3)

        # Connection Status
        ## Icon
        self.connectionButton = QPushButton(self.DisconnectedPicture, '')
        self.connectionButton.setStyleSheet("""
            border: none;
            color: palette(window-text);
            background: transparent;
            padding-top: 3px;""")
        size3 = self.Controller.getDpiScale(3)
        size20 = self.Controller.getDpiScale(20)
        size24 = self.Controller.getDpiScale(24)
        self.connectionButton.setIconSize(QSize(size20, size20))
        self.connectionButton.setFixedSize(QSize(size24, size24))
        self.connectionButton.setContentsMargins(0,0,0,0)
        
        ## Label
        self.connectionFrame = QLabel()
        self.connectionFrame.setTextFormat(Qt.RichText)
        self.connectionFrame.setText('<b>No Connection</b>')
        self.connectionFrame.setMargin(0)
        self.connectionFrame.setContentsMargins(5,3,0,0)

        # Menu bar
        menuBar = QMenuBar(self)

        # Option Menu
        self.optionsMenu = menuBar.addMenu('Options')
        self.optionsMenu.setToolTipsVisible(True)
        ## Settings action
        self.settingsAction = QAction(settingsPicture, 'Settings', self)
        self.settingsAction.triggered.connect(self.openSettings)
        # Sync time checkbox
        self.syncTimeAction = QAction('Sync Time', self)
        self.syncTimeAction.setToolTip('Enable to synchronize Maya\'s Time Slider with Unreal\'s playhead')
        self.syncTimeAction.setCheckable(True)
        self.syncTimeAction.setChecked(False)
        self.syncTimeAction.triggered.connect(self._enablePlayheadSync)
        # Sync Object Transform checkbox
        self._loadObjectTransformSyncPreferences()
        self.syncObjectTransformAction = QAction('Sync Object Transform', self)
        self.syncObjectTransformAction.setToolTip('Enable to synchronize Maya object transform with Unreal in real time')
        self.syncObjectTransformAction.setCheckable(True)
        self.syncObjectTransformAction.setChecked(self._isObjectTransformSyncEnabled())
        self.syncObjectTransformAction.triggered.connect(self._enableObjectTransformSync)
        # Add actions to Option Menu
        self.optionsMenu.addAction(self.settingsAction)
        self.optionsMenu.addAction(self.syncTimeAction)
        self.optionsMenu.addAction(self.syncObjectTransformAction)
        
        # Help Menu
        helpMenu = menuBar.addMenu('Help')
        ## Help Action
        helpAction = QAction(helpPicture, 'Help on Unreal Live Link', self)
        helpAction.triggered.connect(self.openHelpUrl)
        helpMenu.addAction(helpAction)

        ## Feedback submenu
        feedbackMenu = helpMenu.addMenu('Feedback')
        ### Report problem Action
        reportProblemAction = feedbackMenu.addAction('Report a Problem')
        reportProblemAction.triggered.connect(self.openReportProblemUrl)
        ### Unreal LiveLink forum Action
        unrealLiveLinkForumAction = feedbackMenu.addAction('Unreal Live Link Forum')
        unrealLiveLinkForumAction.triggered.connect(self.openUnrealLiveLinkForumUrl)

        ## About Action
        self.aboutAction = QAction(aboutPicture, 'About Unreal Live Link', self)
        self.aboutAction.triggered.connect(self.openAboutBox)
        helpMenu.addAction(self.aboutAction)

        # Menu bar layout
        menuBarLayout = QHBoxLayout()
        menuBarLayout.setMargin(0)
        menuBarLayout.setContentsMargins(0,0,0,0)
        menuBarLayout.setSpacing(0)

        # Populate the layout
        menuBarLayout.addWidget(menuBar, 0, Qt.AlignLeft)
        menuBarLayout.addStretch(1)
        menuBarLayout.addWidget(self.connectionButton, 0, Qt.AlignRight)
        menuBarLayout.addWidget(self.connectionFrame, 0, Qt.AlignRight)
        
        # Add menu bar layout to main layout
        self.mainLayout.addLayout(menuBarLayout)

        # Separator
        separator = QFrame()
        separator.setFrameShape(QFrame.HLine)
        separator.setStyleSheet('background-color: rgb(86, 86, 86)')
        separator.setFixedHeight(2)
        self.mainLayout.addWidget(separator)

        # Button layout
        buttonLayout = QHBoxLayout()
        buttonLayout.setContentsMargins(0, 3, 0, 3)
        self.table = UnrealLiveLinkSubjectTable(self)

        # Add selection
        self.addSelectionButton = HoverButton.fromIconName('add', 'Add Selection ', False, self)
        self.addSelectionButton.setIconSize(QSize(size24, size24))
        self.addSelectionButton.setFixedHeight(size24)
        self.addSelectionButton.setToolTip('Add selected node')
        self.addSelectionButton.clicked.connect(self.table._addRow)
        self.addSelectionButton.setEnabled(False)
        buttonLayout.addWidget(self.addSelectionButton)

        spacer = QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred)
        buttonLayout.addItem(spacer)

        # Pause Animation sequence streaming
        self.pauseIcon = ImageUtils.createIcon('pause')
        self.pauseHoverIcon = ImageUtils.createHoverIcon(self.pauseIcon)
        self.playIcon = ImageUtils.createIcon('play')
        self.playHoverIcon = ImageUtils.createHoverIcon(self.playIcon)
        self.pauseAnimSyncButton = HoverButton.fromIcon(self.pauseIcon, self.pauseHoverIcon, '', True, self)
        self.pauseAnimSyncButton.setIconSize(QSize(size24, size24))
        self.pauseAnimSyncButton.setFixedSize(QSize(size24, size24))
        self.pauseAnimSyncButton.setToolTip('Pause streaming Maya\'s timeline data to Unreal Engine\'s Animation Sequence')
        self.pauseAnimSyncButton.setEnabled(False)
        self._pauseAnimSyncButtonPauseState = True
        self.pauseAnimSyncButton.clicked.connect(self._pauseAnimSeqSync)
        buttonLayout.addWidget(self.pauseAnimSyncButton, alignment=Qt.AlignTop)

        buttonLayout.addSpacing(size3)

        separator = QFrame()
        separator.setFrameShape(QFrame.VLine)
        separator.setStyleSheet('background-color: rgb(86, 86, 86)')
        separator.setFixedWidth(2)
        buttonLayout.addWidget(separator)

        buttonLayout.addSpacing(size3)

        # Delete selection
        self.deleteSelectionButton = HoverButton.fromIconName('delete', '', True, self)
        self.deleteSelectionButton.setIconSize(QSize(size24, size24))
        self.deleteSelectionButton.setFixedSize(QSize(size24, size24))
        self.deleteSelectionButton.setToolTip('Delete selection')
        self.deleteSelectionButton.setEnabled(False)
        self.deleteSelectionButton.clicked.connect(self.table._removeRow)
        buttonLayout.addWidget(self.deleteSelectionButton, alignment=Qt.AlignTop)

        self.mainLayout.addLayout(buttonLayout)

        # Add the subject list table
        self.mainLayout.addWidget(self.table)

        # Create a status bar layout
        layout = QHBoxLayout()

        # Log label
        self._logLabel = QLabel('')
        self._logLabel.setTextFormat(Qt.RichText)
        layout.addWidget(self._logLabel)
        spacer = QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Fixed)
        layout.addItem(spacer)

        # Link progress bar
        self.linkProgressLabel = QLabel()
        self.linkProgressLabel.setText(r'Syncing')
        layout.addWidget(self.linkProgressLabel)
        self.linkProgressBar = QProgressBar()
        self.linkProgressBar.setMinimum(0)
        self.linkProgressBar.setMaximum(100)
        self.linkProgressBar.setValue(0)
        self.linkProgressBar.setMaximumWidth(250)
        layout.addWidget(self.linkProgressBar)
        self.setLinkProgress(0, False)

        # Update label
        self.updateLabel = QLabel()
        self.updateLabel.setTextInteractionFlags(Qt.TextBrowserInteraction);
        self.updateLabel.setOpenExternalLinks(True);
        self.updateLabel.setTextFormat(Qt.RichText)
        url = self.Controller.getUpdateURL()
        if url and len(url) > 0:
            self.updateLabel.setText(self._getUpdateText(url))
        else:
            self.updateLabel.hide()
        layout.addWidget(self.updateLabel)
        self.mainLayout.addLayout(layout)

        self.setLayout(self.mainLayout)

    def logMessage(self, message, timeToClear=3000):
        if len(message) > 0:
            self._logLabel.setText(message)

            # Clear the log after some time
            QTimer.singleShot(3000, lambda: self._logLabel.setText(''))

    def updateConnectionState(self, ConnectionText, ConnectedState):
        if ConnectedState != self.ConnectedState:
            self.connectionFrame.setText('<b>' + ConnectionText + '</b>')
            self.ConnectedState = ConnectedState
            connectionPicture = None
            if ConnectedState and self.ConnectedPicture:
                connectionPicture = self.ConnectedPicture
                if self.Controller:
                    self.Controller.connectionMade()
            elif ConnectedState is False and self.DisconnectedPicture:
                connectionPicture = self.DisconnectedPicture
            if connectionPicture:
                self.connectionButton.setIcon(connectionPicture)

            self.table.connectionStateChanged(self.ConnectedState)

    def clearUI(self):
        if self.table:
            self.table._clearUI()

    def refreshUI(self):
        if self.table:
            self.table._refreshUI()
        self.syncTimeAction.setChecked(self.Controller.isPlayheadSyncEnabled())
        self.syncObjectTransformAction.setChecked(self._isObjectTransformSyncEnabled())

    def showEvent(self, event):
        unrealVersion = self.Controller.getLoadedUnrealVersion()
        self.enableControls(unrealVersion != -1)
        super(UnrealLiveLinkWindow, self).showEvent(event)

    def hideEvent(self, event):
        super(UnrealLiveLinkWindow, self).hideEvent(event)

    def closeEvent(self, event):
        super(UnrealLiveLinkWindow, self).closeEvent(event)

    def paintEvent(self, event):
        super(UnrealLiveLinkWindow, self).paintEvent(event)

    def moveEvent(self, event):
        super(UnrealLiveLinkWindow, self).moveEvent(event)

    def setWindowRect(self, tlc, w, h):
        self.setGeometry(0, 0, w, h)
        self.move(tlc[0], tlc[1])

    def openSettings(self):
        Settings = UnrealLiveLinkSettings(self)
        Settings.initUI()
        Settings.showWindow()

    def setWindowProperty(self, name, value):
        self._properties[name] = value
        self.setProperty(name, value)

    def notifyNewerUpdate(self):
        url = self.Controller.getUpdateURL()
        if url and len(url) > 0:
            self.updateLabel.setText(self._getUpdateText(url))
            self.updateLabel.show()

    def setLinkProgress(self, value, visible):
        self.linkProgressLabel.setVisible(visible)
        self.linkProgressBar.setVisible(visible)
        self.linkProgressBar.setValue(value)

        if value == self.linkProgressBar.maximum():
            # Keep the progress bar around to show the user that linking is finished
            QTimer.singleShot(1000, lambda: self.setLinkProgress(0, False))

    def updateLinkInfo(self, dagPath, linkedAssetPath, targetAssetPath, targetAssetName, linkedAssetClass, linkedAssetUnrealNativeClass):
        self.table.updateLinkInfo(dagPath, linkedAssetPath, targetAssetPath, targetAssetName, linkedAssetClass, linkedAssetUnrealNativeClass)

    def selectedObject(self, hasSelection):
        self.addSelectionButton.setEnabled(self.table.isEnabled() and hasSelection)

    def isPauseAnimSeqSyncEnabled(self):
        return not self._pauseAnimSyncButtonPauseState

    def _getUpdateText(self, url):
        return '<a href="' + url + '">Update Available</a>' if url and len('URL') > 0 else ''

    def enableControls(self, enable):
        self.settingsAction.setEnabled(enable)
        self.aboutAction.setEnabled(enable)
        self.table.setEnabled(enable)

    def _enablePlayheadSync(self, state):
        self.Controller.enablePlayheadSync(self.syncTimeAction.isChecked())

    def _enableObjectTransformSync(self, state):
        self.Controller.enableObjectTransformSync(self.syncObjectTransformAction.isChecked())

    def _isObjectTransformSyncEnabled(self):
        return self.Controller.isObjectTransformSyncEnabled()
    
    def _loadObjectTransformSyncPreferences(self):
        self.Controller.loadObjectTransformSyncPreferences()

    def _pauseAnimSeqSync(self, state):
        self.Controller.pauseAnimSeqSync(self._pauseAnimSyncButtonPauseState)
        if self._pauseAnimSyncButtonPauseState:
            self.pauseAnimSyncButton.replaceIcons(self.playIcon, self.playHoverIcon)
        else:
            self.pauseAnimSyncButton.replaceIcons(self.pauseIcon, self.pauseHoverIcon)
        self._pauseAnimSyncButtonPauseState = not self._pauseAnimSyncButtonPauseState