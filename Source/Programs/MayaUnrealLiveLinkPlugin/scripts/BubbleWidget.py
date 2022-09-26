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
  from PySide.QtCore import *
  from PySide.QtGui import *
  from PySide import __version__

from HoverButton import HoverButton

class BubbleWidget(QFrame):
    closing = Signal()

    def __init__(self, text, parent=None):
        super(BubbleWidget, self).__init__(parent)

        # Main layout
        layout = QHBoxLayout()
        layout.setSpacing(0)
        layout.setContentsMargins(0, 0, 0, 1)

        # Get the colors from the close button
        self._closeButton = HoverButton.fromIconName('close_tag', '', False, self) # QPushButton('x')
        backgroundColor = self._closeButton.palette().color(QPalette.Button).name()

        # Create the label
        self._label = QLabel(text)
        self._label.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._label)

        vboxLayout = QVBoxLayout()
        vboxLayout.setContentsMargins(0, 1, 0, 0)

        # Size the button to have the same height as the font (make sure the size is even to avoid cropping image issues)
        capHeight = QFontMetrics(self._label.font()).capHeight()
        capHeight += capHeight % 2 # + 1
        self._closeButton.setFixedSize(capHeight, capHeight)

        # Setup the close button
        self._closeButton.setStyleSheet("""QPushButton {
                                        padding: 0px 0px 0px 0px;
                                        border: none;
                                        background-color: """ + backgroundColor + """;
                                        }
                                        QPushButton:hover {
                                            font: bold;
                                        }""")
        self._closeButton.setContentsMargins(0, 0, 0, 0)
        size = self._closeButton.sizeHint()
        self._closeButton.pressed.connect(self._onCloseButtonPressed)

        vboxLayout.addWidget(self._closeButton)
        layout.addLayout(vboxLayout)

        # Make sure the label has the same height as the button
        self._label.setMaximumHeight(size.height())

        # Setup the widget style
        self.setStyleSheet("""QFrame {
                            background-color: """ + backgroundColor + """;
                            border-style: outset;
                            border-width: 0px;
                            border-radius: 2px;
                            padding: 1px 4px 1px 1px;}""")
        self.setLayout(layout)

    def setText(self, text):
        self._label.setText(text)
        self.adjustSize()

    def text(self):
        return self._label.text()

    def _onCloseButtonPressed(self):
        self.close()
        self.closing.emit()
