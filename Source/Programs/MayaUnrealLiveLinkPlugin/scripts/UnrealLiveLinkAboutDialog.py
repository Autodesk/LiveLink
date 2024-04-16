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


class UnrealLiveLinkAboutDialog(QDialog):
    def __init__(self, pluginVersion, apiVersion, unrealVersion, parent=None):
        super(UnrealLiveLinkAboutDialog, self).__init__(parent=parent)

        self.mainLayout = QVBoxLayout()
        self.setMinimumSize(QSize(600, 450))

        self.setWindowFlags(Qt.Window)
        self.setObjectName("UnrealLiveLinkAboutDialog")
        self.setWindowTitle('About Unreal Live Link')

        if pluginVersion and len(pluginVersion) > 0:
            self.pluginVersionLabel = QLabel()
            self.pluginVersionLabel.setText("Plugin version: " + pluginVersion)
            self.mainLayout.addWidget(self.pluginVersionLabel)

        if apiVersion and len(apiVersion) > 0:
            self.apiVersionLabel = QLabel()
            self.apiVersionLabel.setText("API version: " + apiVersion)
            self.mainLayout.addWidget(self.apiVersionLabel)

        if unrealVersion and len(unrealVersion) > 0:
            self.unrealVersionLabel = QLabel()
            self.unrealVersionLabel.setText("Unreal version: " + unrealVersion)
            self.mainLayout.addWidget(self.unrealVersionLabel)

        self.aboutTextEdit = QTextEdit()
        self.aboutTextEdit.setReadOnly(True)
        self.mainLayout.addWidget(self.aboutTextEdit)

        layout = QHBoxLayout()
        spacer = QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred)
        layout.addItem(spacer)

        button = QPushButton()
        button.setText("OK")
        button.clicked.connect(lambda: self.accept())
        layout.addWidget(button)

        spacer = QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred)
        layout.addItem(spacer)
        self.mainLayout.addLayout(layout)

        self.setLayout(self.mainLayout)

    def setAboutText(self, text):
        if text and (isinstance(text, str) or isinstance(text, unicode)):
            self.aboutTextEdit.setText(text)
