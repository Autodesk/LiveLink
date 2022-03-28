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

class ToolTipWindow(QDialog):
    def __init__(self, parent=None):
        super(ToolTipWindow, self).__init__(parent=parent, f=Qt.Dialog | Qt.FramelessWindowHint)
        layout = QVBoxLayout()
        self.textBox = QLabel()
        palette = self.palette()
        palette.setColor(QPalette.Window, palette.color(QPalette.ToolTipBase))
        palette.setColor(QPalette.WindowText, palette.color(QPalette.ToolTipText))
        self.setPalette(palette)
        self.setAutoFillBackground(True)
        layout.addWidget(self.textBox)
        self.setLayout(layout)

    def text(self):
        return self.textBox.text()

    def setText(self, text):
        if isinstance(text, str) or isinstance(text, unicode):
            self.textBox.setText(text)

class ButtonHoverWatcher(QObject):
    def __init__(self, iconName, parent=None):
        super(ButtonHoverWatcher, self).__init__(parent)
        self.LeaveIcon = UnrealLiveLinkWindow.createIcon(iconName)
        if self.LeaveIcon:
            hoverIcon = QIcon(UnrealLiveLinkWindow.highlightPixmap(self.LeaveIcon.pixmap(32, 32)))
            self.HoverIcon = hoverIcon
        else:
            self.HoverIcon = None

    def eventFilter(self, watched, event):
        button = watched
        if button is None or not button.isEnabled():
            return False

        if event.type() == QEvent.Enter and self.HoverIcon:
            button.setIcon(self.HoverIcon)
            return True
        elif event.type() == QEvent.Leave and self.LeaveIcon:
            button.setIcon(self.LeaveIcon)
            return True
        return False

class UnrealLiveLinkWindow(QWidget):
    iconPath = ''

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
        self.lastUESelectorIndex = -1

        if parent and windowTitle:
            parent.setWindowTitle(windowTitle)

    def __del__(self):
        self.Controller = None

    @staticmethod
    def setIconPath(path):
        UnrealLiveLinkWindow.iconPath = path

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

        self.ConnectedPicture = UnrealLiveLinkWindow.createIcon('infoConnected')
        self.DisconnectedPicture = UnrealLiveLinkWindow.createIcon('infoDisconnected')
        helpPicture = UnrealLiveLinkWindow.createIcon('help')
        aboutPicture = UnrealLiveLinkWindow.createIcon('about')
        settingsPicture = UnrealLiveLinkWindow.createIcon('settings')

        self.setFocusPolicy(Qt.StrongFocus)

        self.mainLayout = QVBoxLayout()
        self.mainLayout.setContentsMargins(7, 0, 7, 7)

        # Connection Status
        ## Icon
        self.connectionButton = QPushButton(self.DisconnectedPicture, '')
        self.connectionButton.setStyleSheet("""
            border: none;
            color: palette(window-text);
            background: transparent;
            padding-top: 3px;""")
        self.connectionButton.setIconSize(QSize(20, 20))
        self.connectionButton.setFixedSize(QSize(20, 20))
        self.connectionButton.setContentsMargins(0,0,0,0)
        
        ## Label
        self.connectionFrame = QLabel()
        self.connectionFrame.setTextFormat(Qt.RichText)
        self.connectionFrame.setText('<b>No Connection</b>')
        self.connectionFrame.setMargin(0)
        self.connectionFrame.setContentsMargins(5,3,0,0)

        # Menu bar
        menuBar = QMenuBar(self)

        #Edit Menu
        self.editMenu = menuBar.addMenu('Edit')
        ## Settings action
        self.settingsAction = QAction(settingsPicture, 'Settings', self)
        self.settingsAction.triggered.connect(self.openSettings)
        
        self.editMenu.addAction(self.settingsAction)
        
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
        self.table = UnrealLiveLinkSubjectTable(self)

        # Add selection
        buttonHoverWatcher = ButtonHoverWatcher('add', self)
        self.addSelectionButton = QPushButton(buttonHoverWatcher.LeaveIcon, 'Add Selection')
        self.addSelectionButton.setIconSize(QSize(20, 20))
        self.addSelectionButton.setFixedHeight(20)
        self.addSelectionButton.setStyleSheet(self.PushButtonStyleSheet)
        self.addSelectionButton.setToolTip('Add selected node')
        self.addSelectionButton.clicked.connect(self.table._addRow)
        self.addSelectionButton.installEventFilter(buttonHoverWatcher)
        buttonLayout.addWidget(self.addSelectionButton)

        spacer = QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred)
        buttonLayout.addItem(spacer)

        # Delete selection
        buttonHoverWatcher = ButtonHoverWatcher('delete', self)
        self.deleteSelectionButton = QPushButton(buttonHoverWatcher.LeaveIcon, '')
        self.deleteSelectionButton.setIconSize(QSize(20, 20))
        self.deleteSelectionButton.setFixedSize(QSize(20, 20))
        self.deleteSelectionButton.setStyleSheet(self.PushButtonStyleSheet)
        self.deleteSelectionButton.setToolTip('Delete selection')
        self.deleteSelectionButton.setEnabled(False)
        self.deleteSelectionButton.clicked.connect(self.table._removeRow)
        self.deleteSelectionButton.installEventFilter(buttonHoverWatcher)
        buttonLayout.addWidget(self.deleteSelectionButton, alignment=Qt.AlignTop)

        self.mainLayout.addLayout(buttonLayout)

        # Add the subject list table
        self.mainLayout.addWidget(self.table)

        # Create a status bar layout
        layout = QHBoxLayout()

        # UE selector
        self._ueSelector = QComboBox()
        self._ueSelector.setSizeAdjustPolicy(QComboBox.AdjustToContents)
        self._ueSelector.setToolTip('Please use this drop-down to select which plugin to load\nbased on the Unreal version you are using.')
        unrealVersion = self.Controller.getLoadedUnrealVersion()
        self._initUESelector()
        if unrealVersion == -1:
            self._ueSelector.setCurrentIndex(-1)
        else:
            self._ueSelector.setCurrentIndex(0 if unrealVersion == 4 else 1)
            self._ueSelector.setMinimumContentsLength(1)
            self._ueSelector.setMaximumWidth(self._ueSelector.sizeHint().width())
        self.lastUESelectorIndex = self._ueSelector.currentIndex()
        self._ueSelector.currentIndexChanged.connect(self._onUnrealVersionChanged)
        layout.addWidget(self._ueSelector)
        self._toolTipWindow = None

        # Log label
        self._logLabel = QLabel('')
        self._logLabel.setTextFormat(Qt.RichText)
        layout.addWidget(self._logLabel)
        spacer = QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Fixed)
        layout.addItem(spacer)

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

        if unrealVersion == -1:
            self.enableControls(False)

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

    @staticmethod
    def createIcon(iconName):
        icon = QIcon()
        availableSizes = ['1', '2', '3']
        for size in availableSizes:
            path = os.path.join(UnrealLiveLinkWindow.iconPath, iconName + '@' + size + 'x.png')
            if os.path.isfile(path):
                icon.addFile(path)
        return icon

    @staticmethod
    def highlightPixmap(pixmap):
        img = QImage(pixmap.toImage().convertToFormat(QImage.Format_ARGB32))
        imgh = img.height()
        imgw = img.width()

        for y in range (0, imgh, 1):
            for x in range (0, imgw, 1):
                pixel = img.pixel(x, y)
                highLimit = 205 # value above this limit will just max up to 255
                lowLimit = 30 # value below this limit will not be adjusted
                adjustment = 255 - highLimit
                color = QColor(pixel)
                v = color.value()
                s = color.saturation()
                h = color.hue()
                if(v > lowLimit):
                    if (v < highLimit):
                        v = v + adjustment
                    else:
                        v = 255
                v = color.setHsv(h, s, v)
                img.setPixel(x, y, qRgba(color.red(), color.green(), color.blue(), qAlpha(pixel)))
        return QPixmap(img)

    def clearUI(self):
        if self.table:
            self.table._clearUI()

    def refreshUI(self):
        if self.table:
            self.table._refreshUI()
        self._refreshUESelector()

    def showEvent(self, event):
        self._refreshUESelector()
        super(UnrealLiveLinkWindow, self).showEvent(event)

    def hideEvent(self, event):
        self._deleteUESelectorToolTip()
        super(UnrealLiveLinkWindow, self).hideEvent(event)

    def closeEvent(self, event):
        self._deleteUESelectorToolTip()
        super(UnrealLiveLinkWindow, self).closeEvent(event)

    def paintEvent(self, event):
        self._moveUESelectorToolTip()
        super(UnrealLiveLinkWindow, self).paintEvent(event)

    def moveEvent(self, event):
        self._moveUESelectorToolTip()
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
            self.updateLabel.setText('<a href="' + url + '">Update Available</a>')
            self.updateLabel.show()

    def _getUpdateText(self, url):
        return '<a href="' + url + '">Update Available</a>' if url and len('URL') > 0 else ''

    def enableControls(self, enable):
        self.addSelectionButton.setEnabled(enable)
        self.settingsAction.setEnabled(enable)
        self.aboutAction.setEnabled(enable)
        self.table.setEnabled(enable)

    def _onUnrealVersionChanged(self, index):
        if index == self.lastUESelectorIndex:
            return

        if index >= 0:
            # Resize the combo box to take only the necessary space
            self._ueSelector.setMinimumContentsLength(1)
            self._ueSelector.setMaximumWidth(self._ueSelector.sizeHint().width())
            self._ueSelector.setEditable(False)

            # Change the loaded Unreal version for the next reboot
            if self.Controller.setLoadedUnrealVersion(self._ueSelector.itemData(index)):
                self.enableControls(True)
            else:
                self._ueSelector.setCurrentIndex(self.lastUESelectorIndex)

            if self._toolTipWindow and self._toolTipWindow.isVisible():
                self._toolTipWindow.hide()
        else:
            self._ueSelector.currentIndexChanged.disconnect()
            self._initUESelector()
            self._ueSelector.setCurrentIndex(-1)
            self._ueSelector.currentIndexChanged.connect(self._onUnrealVersionChanged)
            self.enableControls(False)

        self.lastUESelectorIndex = self._ueSelector.currentIndex()

    def _initUESelector(self):
        self._ueSelector.clear()
        instructions = 'Please select the Unreal version to use'
        # If the plugin is not loaded, let the user know to set the Unreal version before using the plugin
        self._ueSelector.setMinimumContentsLength(len(instructions))
        self._ueSelector.setEditable(True)
        self._ueSelector.lineEdit().setPlaceholderText(instructions)

        versionInfo = PySide2.__version_info__
        palette = self._ueSelector.lineEdit().palette()
        if versionInfo[0] >= 5 and versionInfo[1] >= 12:
            palette.setColor(QPalette.PlaceholderText, palette.color(QPalette.WindowText))
        palette.setColor(QPalette.Base, palette.color(QPalette.Button))
        self._ueSelector.setPalette(palette)
        self._ueSelector.lineEdit().setReadOnly(True)
        self._ueSelector.setMaximumWidth(self._ueSelector.sizeHint().width())
        self._ueSelector.addItem('UE 4.27.2', 4)
        self._ueSelector.addItem('UE 5.0.0', 5)
        self._ueSelector.addItem(instructions, -1)
        self._ueSelector.removeItem(2)

    def _refreshUESelector(self):
        if self.Controller:
            unrealVersion = self.Controller.getLoadedUnrealVersion()
            if unrealVersion == -1:
                if self.lastUESelectorIndex != unrealVersion:
                    self._ueSelector.setCurrentIndex(-1)
                self._createUESelectorToolTip()
                self._toolTipWindow.show()
                self._moveUESelectorToolTip()
            else:
                self._ueSelector.setCurrentIndex(0 if unrealVersion == 4 else 1)

    def _createUESelectorToolTip(self):
        self._deleteUESelectorToolTip()
        self._toolTipWindow = ToolTipWindow(self)
        self._toolTipWindow.setText(self._ueSelector.toolTip())

    def _deleteUESelectorToolTip(self):
        if self._toolTipWindow:
            self._toolTipWindow.hide()
            del self._toolTipWindow
            self._toolTipWindow = None

    def _moveUESelectorToolTip(self):
        if self._toolTipWindow and self._toolTipWindow.isVisible():
            self._toolTipWindow.move(self._ueSelector.mapToGlobal(QPoint(self._ueSelector.width() + 5, 0)))
