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

usingPyside6 = False

try:
  from PySide2.QtCore import *
  from PySide2.QtGui import *
  from PySide2.QtWidgets import *
  from PySide2 import __version__
except ImportError:
    try:
        from PySide.QtCore import *
        from PySide.QtGui import *
        from PySide import __version__
    except ImportError:
        from PySide6.QtCore import *
        from PySide6.QtGui import *
        from PySide6.QtWidgets import *
        from PySide6 import __version__
        usingPyside6 = True

from UnrealLiveLinkSettingsNetwork import UnrealLiveLinkSettingsNetwork

class UnrealLiveLinkSettings(QDialog):
    class TreeWidgetItem(QTreeWidgetItem):
        def __init__(self, name = ''):
            super(UnrealLiveLinkSettings.TreeWidgetItem, self).__init__([name])

        def name(self):
            return self.text(0)

        def fullpath(self):
            fullpath = [self.name()]
            parent = self.parent()
            while parent:
                fullpath.append(parent.name())
                parent = parent.parent()
            fullpath.reverse()
            return '/'.join(fullpath)

    _windowName = 'UnrealLiveLinkSettings'

    def __init__(self, parent=None):
        super(UnrealLiveLinkSettings, self).__init__(parent)
        self._parentWindow = parent

    def showWindow(self):
        self.exec_()

    def initUI(self):
        self.mainLayout = QVBoxLayout()
        self.mainLayout.setContentsMargins(5, 5, 5, 5)
        self.setWindowFlags(Qt.Window)
        self.setObjectName(UnrealLiveLinkSettings._windowName)
        self.setWindowTitle('Unreal Live Link settings')

        self.labelStyle = 'QLabel { background-color : ' + self.palette().color(QPalette.Button).name() + '; }'

        # Content layout
        categoryLayout = QVBoxLayout()
        label = QLabel()
        label.setLineWidth(0)
        label.setTextFormat(Qt.RichText)
        label.setText('<b>Categories</b>')
        label.setStyleSheet(self.labelStyle)
        if usingPyside6:
            label.setContentsMargins(4, 4, 4, 4)
        else:
            label.setMargin(4)
        categoryLayout.addWidget(label)
        self.categoryTreeWidget = QTreeWidget()
        self.categoryTreeWidget.setHeaderHidden(True)
        self.categoryTreeWidget.setRootIsDecorated(False);
        self.categoryTreeWidget.setMinimumWidth(120)
        self.categoryTreeWidget.setMaximumWidth(120)
        self.categoryTreeWidget.setSortingEnabled(False)
        self.categoryTreeWidget.setItemsExpandable(False)
        self.categoryTreeWidget.itemClicked.connect(self._categoryTreeItemClicked)
        categoryLayout.addWidget(self.categoryTreeWidget)
        categoryLayout.setSpacing(0)
        categoryLayout.setContentsMargins(0,0,0,0)

        # Preference layout
        self.preferenceLayout = QVBoxLayout()
        self.preferencesLabel = QLabel('')
        self.preferencesLabel.setTextFormat(Qt.RichText)
        self.preferencesLabel.setStyleSheet(self.labelStyle)
        self.preferencesLabel.setMargin(4)
        self.preferencesLabel.setMinimumWidth(350)
        self.preferenceLayout.addWidget(self.preferencesLabel)
        self.preferencesFrame = QFrame()
        self.preferencesFrame.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.preferenceLayout.addWidget(self.preferencesFrame)
        self.preferenceLayout.setSpacing(0)
        self.preferenceLayout.setContentsMargins(0,0,0,0)

        self.contentLayout = QHBoxLayout()
        self.contentLayout.addLayout(categoryLayout)
        self.contentLayout.addLayout(self.preferenceLayout)
        self.contentLayout.setSpacing(5)
        self.contentLayout.setContentsMargins(0,0,0,3)
        self.mainLayout.addLayout(self.contentLayout)
        self.mainLayout.setSpacing(3)

        # Save/Cancel layout
        saveLayout = QHBoxLayout()
        saveButton = QPushButton()
        saveButton.setText('Save')
        saveButton.clicked.connect(self._onSaveClicked)
        saveLayout.addWidget(saveButton)

        cancelButton = QPushButton()
        cancelButton.setText('Cancel')
        cancelButton.clicked.connect(self._onCancelClicked)
        saveLayout.addWidget(cancelButton)

        self.mainLayout.addLayout(saveLayout)
        self.setLayout(self.mainLayout)

        self.closeEvent = self.closeEvent

        self._fillTreeWidget()

    def _fillTreeWidget(self):
        self.contentLayoutTable = dict()

        networkTreeWidget = UnrealLiveLinkSettings.TreeWidgetItem('Network')
        widget = UnrealLiveLinkSettingsNetwork(self._parentWindow.Controller)
        self.contentLayoutTable[networkTreeWidget.fullpath()] = widget

        self.categoryTreeWidget.addTopLevelItem(networkTreeWidget)
        topLevelItem = self.categoryTreeWidget.topLevelItem(0)
        topLevelItem.setSelected(True)
        self._categoryTreeItemClicked(topLevelItem, 0)

        self.categoryTreeWidget.expandAll()

    def _categoryTreeItemClicked(self, item, col):
        prevWidget = self.preferenceLayout.itemAt(1).widget()

        fullpath = item.fullpath()
        newWidget = None
        if fullpath in self.contentLayoutTable:
            newWidget = self.contentLayoutTable[fullpath]
        else:
            newWidget = self.preferencesFrame

        if newWidget != prevWidget:
            self.preferenceLayout.replaceWidget(prevWidget, newWidget)
            prevWidget.hide()
            newWidget.show()
            self.preferencesLabel.setText('<b>' + item.name() + ': General ' + item.name() + ' Preferences</b>')

    def _onSaveClicked(self):
        for key in self.contentLayoutTable:
            widget = self.contentLayoutTable[key]
            if isinstance(widget, QWidget):
                widget.saveContent(self._parentWindow.Controller)
        self.accept()

    def _onCancelClicked(self):
        self.reject()

    def closeEvent(self, event):
        super(UnrealLiveLinkSettings, self).closeEvent(event)
        event.accept()
