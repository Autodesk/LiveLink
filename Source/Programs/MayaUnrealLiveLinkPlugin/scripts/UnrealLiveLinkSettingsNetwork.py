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

from IPAddressLineEdit import IPAddressLineEdit

class UnrealLiveLinkSettingsNetwork(QWidget):
    class ListWidget(QListWidget):
        def __init__(self, button, parent=None):
            super(UnrealLiveLinkSettingsNetwork.ListWidget, self).__init__(parent)
            self.removeButton = button

        def focusOutEvent(self, event):
            super(UnrealLiveLinkSettingsNetwork.ListWidget, self).focusOutEvent(event)
            self.selectionModel().clear()
            self.removeButton.setEnabled(False)

    def __init__(self, controller, parent=None):
        super(UnrealLiveLinkSettingsNetwork, self).__init__(parent)
        self.labelStyle = 'QLabel { background-color : ' + self.palette().color(QPalette.Button).name() + '; }'
        self.setWindowTitle('Endpoints')

        mainLayout = QVBoxLayout()

        self.unicastEndpoint = '0.0.0.0:0'
        self.staticEndpoints = []
        if controller:
            endpointsDict = controller.getNetworkEndpoints()
            if endpointsDict:
                if 'unicast' in endpointsDict:
                    self.unicastEndpoint = endpointsDict['unicast']
                if 'static' in endpointsDict:
                    self.staticEndpoints = endpointsDict['static']

        vboxLayout = QVBoxLayout()
        gridLayout = QGridLayout()

        #########
        # Row 0 #
        #########
        rowNumber = 0

        # Unicast endpoint
        label = QLabel('Unicast Endpoint')
        gridLayout.addWidget(label, rowNumber, 0, 1, 1, Qt.AlignRight)

        # Unicast endpoint line edit
        self.unicastLineEdit = IPAddressLineEdit(self.unicastEndpoint)
        self.unicastLineEdit.defaultIPAddress = '0.0.0.0:0'
        self.unicastLineEdit.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self.unicastLineEdit.setPlaceholderText('X.X.X.X:X')
        self.unicastLineEdit.textModified.connect(self._unicastEndpointChanged)
        gridLayout.addWidget(self.unicastLineEdit, rowNumber, 1)

        #########
        # Row 1 #
        #########
        rowNumber += 1

        # Static endpoints
        label = QLabel('Static Endpoints')
        gridLayout.addWidget(label, rowNumber, 0, 1, 1, Qt.AlignRight)

        # Static endpoint line edit
        self.staticEndpointLineEdit = IPAddressLineEdit('')
        self.staticEndpointLineEdit.setPlaceholderText('X.X.X.X:X')
        self.staticEndpointLineEdit.textChanged.connect(lambda x, le=self.staticEndpointLineEdit: self._validateEndpoint(x, le))
        gridLayout.addWidget(self.staticEndpointLineEdit, rowNumber, 1)

        # Static endpoint add button
        self.addButton = QPushButton('+')
        self.addButton.setStyleSheet("text-align:center;");
        self.addButton.clicked.connect(self._addStaticEndpointToListWidget)
        textSize = self.addButton.fontMetrics().size(Qt.TextShowMnemonic, self.addButton.text())
        opt = QStyleOptionButton();
        opt.initFrom(self.addButton);
        opt.rect.setSize(textSize);
        size = self.addButton.style().sizeFromContents(QStyle.CT_PushButton,
                                                       opt,
                                                       textSize,
                                                       self.addButton)
        size = QSize(size.width(), size.width())
        self.addButton.setMaximumSize(size);
        self.addButton.setEnabled(False)
        gridLayout.addWidget(self.addButton, rowNumber, 2, Qt.AlignRight | Qt.AlignTop)

        #########
        # Row 2 #
        #########
        rowNumber += 1

        # Static endpoint remove button
        self.removeButton = QPushButton('-')
        self.removeButton.setStyleSheet("text-align:center;");
        self.removeButton.setMaximumSize(size)
        self.removeButton.setEnabled(False)
        self.removeButton.clicked.connect(self.removeStaticEndpointFromListWidget)

        # Static endpoint list widget
        self.staticEndpointListWidget = UnrealLiveLinkSettingsNetwork.ListWidget(self.removeButton)
        for staticEndpoint in self.staticEndpoints:
            self.staticEndpointListWidget.addItem(staticEndpoint)
        self.staticEndpointListWidget.itemClicked.connect(self._staticEndpointItemClicked)
        gridLayout.addWidget(self.staticEndpointListWidget, rowNumber, 1)
        gridLayout.addWidget(self.removeButton, rowNumber, 2, Qt.AlignRight | Qt.AlignTop)

        #########
        # Row 3 #
        #########
        rowNumber += 1

        # Horizontal spacer
        gridLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Expanding), rowNumber, 0, 1, 3)

        # Vertical spacer
        gridLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Expanding), 0, 3, rowNumber, 1)

        gridLayout.setColumnMinimumWidth(0, 150)

        label = QLabel()
        label.setTextFormat(Qt.RichText)
        label.setText('<b>Endpoints</b>')
        label.setStyleSheet(self.labelStyle)
        label.setContentsMargins(7, 2, 7, 2)
        vboxLayout.addWidget(label)

        vboxLayout.addLayout(gridLayout)
        mainLayout.addLayout(vboxLayout)

        self.setLayout(mainLayout)

    def saveContent(self, controller):
        if controller:
            # Give focus back to the parent to trigger the focusOutEvent
            self.parentWidget().setFocus()

            controller.setNetworkEndpoints({
                'unicast' : self.unicastEndpoint,
                'static' : self.staticEndpoints})

    def _validateEndpoint(self, endpoint, lineEdit):
        if lineEdit:
            self.addButton.setEnabled(lineEdit.hasAcceptableInput())
            self.removeButton.setEnabled(False)

    def _unicastEndpointChanged(self, oldIP, newIP):
        if oldIP != newIP:
            self._validateEndpoint(newIP, None)
            self.unicastEndpoint = newIP

    def _addStaticEndpointToListWidget(self, checked):
        endpoint = self.staticEndpointLineEdit.text()
        trimmed = IPAddressLineEdit.trim(endpoint)
        if len(trimmed) > 0:
            endpoint = trimmed

        if endpoint not in self.staticEndpoints:
            self.staticEndpointListWidget.addItem(endpoint)
            self.staticEndpoints.append(endpoint)
        self.staticEndpointLineEdit.clear()

    def _staticEndpointItemClicked(self, item):
        self.removeButton.setEnabled(True)

    def removeStaticEndpointFromListWidget(self, checked):
        items = self.staticEndpointListWidget.selectedItems()
        for item in items:
            self.staticEndpoints.remove(item.text())
            self.staticEndpointListWidget.takeItem(self.staticEndpointListWidget.row(item))
        if len(self.staticEndpoints) == 0:
            self.removeButton.setEnabled(False)
