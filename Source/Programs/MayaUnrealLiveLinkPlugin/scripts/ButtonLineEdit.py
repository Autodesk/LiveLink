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

from HoverButton import *

class ButtonLineEdit(QLineEdit):
    buttonPressed = Signal()

    def __init__(self, leftIcon, rightIcon, parent=None):
        super(ButtonLineEdit, self).__init__(parent)

        buttonSize = self.sizeHint().height()
        self.setContentsMargins(0,0,0,0)
        self.setTextMargins(0,0,0,0)

        # Left label
        self.leftLabel = None
        if leftIcon and isinstance(leftIcon, QIcon):
            self.leftLabel = QLabel('', self)
            self.leftLabel.setStyleSheet('border: 0px; padding: 0px;')
            self.leftLabel.setCursor(Qt.ArrowCursor)
            self.leftLabel.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Preferred)
            pixmap = leftIcon.pixmap(QSize(buttonSize, buttonSize))
            self.leftLabel.setPixmap(pixmap)

        # Left widget
        self._leftWidget = None

        # Right button
        self.rightButton = None
        if rightIcon and isinstance(rightIcon, QIcon):
            self.rightButton = HoverButton(rightIcon, '', False, self)
            self.rightButton.setStyleSheet('border: 0px; padding: 0px;')
            self.rightButton.setCursor(Qt.ArrowCursor)
            self.rightButton.pressed.connect(self.buttonPressed.emit)

        # Change size/padding to take the right button and left label into account
        self.setStyleSheet('QLineEdit {{padding-right: {0}px; padding-left: {1}px; }}'.format(buttonSize, buttonSize))
        self.setMinimumSize(max(self.minimumSizeHint().width(), self.rightButton.sizeHint().width() if self.rightButton else buttonSize),
                            max(self.minimumSizeHint().height(), self.rightButton.sizeHint().height() if self.rightButton else buttonSize))

    def setLeftWidget(self, widget):
        self._leftWidget = widget
        if widget:
            self._leftWidget.setParent(self)
            self._leftWidget.setCursor(Qt.ArrowCursor)
            self._leftWidget.setMaximumHeight(self.sizeHint().height() - 6)
            self._leftWidget.installEventFilter(self)

    def leftWidget(self):
        return self._leftWidget

    def resizeEvent(self, event):
        self._updateWidgetPositions()
        super(ButtonLineEdit, self).resizeEvent(event)

    def _updateWidgetPositions(self):
        if self.rightButton:
            buttonSize = self.rightButton.sizeHint()
            self.rightButton.move(self.rect().right() - buttonSize.width(),
                                  (self.rect().bottom() - buttonSize.height() + 1)/2)

        if self.leftLabel:
            self.leftLabel.move(0,
                                (self.rect().bottom() - self.leftLabel.sizeHint().height())/2)

        padLeft = self.sizeHint().height()
        if self._leftWidget:
            right = self.leftLabel.pos().x() + self.leftLabel.sizeHint().width()

            if self._leftWidget.isVisible():
                padLeft += self._leftWidget.sizeHint().width()

            self._leftWidget.move(right,
                                  (self.rect().bottom() - self._leftWidget.height() + 1)/2)

        self.setStyleSheet('QLineEdit {{padding-right: {0}px; padding-left: {1}px; }}'.format(self.sizeHint().height(), padLeft))

    def eventFilter(self, watched, event):
        if self._leftWidget is None or watched != self._leftWidget:
            return False

        eventType = event.type()
        if eventType == QEvent.Show or eventType == QEvent.Hide or eventType == QEvent.Close:
            self._updateWidgetPositions()
            return True

        return False
