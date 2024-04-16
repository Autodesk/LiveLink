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

from ImageUtils import *

class ButtonHoverWatcher(QObject):
    def __init__(self, iconName='', parent=None):
        super(ButtonHoverWatcher, self).__init__(parent)
        if iconName and isinstance(iconName, str) and len(iconName) > 0:
            self.LeaveIcon = ImageUtils.createIcon(iconName)
            self.HoverIcon = ImageUtils.createHoverIcon(self.LeaveIcon, 32)

    @classmethod
    def fromIcon(cls, icon, hoverIcon, parent=None):
        if icon and isinstance(icon, QIcon):
            watcher = ButtonHoverWatcher('', parent)
            watcher.LeaveIcon = icon
            if not hoverIcon:
                watcher.HoverIcon = ImageUtils.createHoverIcon(icon, 32)
            elif isinstance(hoverIcon, QIcon):
                watcher.HoverIcon = hoverIcon
            return watcher
        return None

    def replaceIcons(self, icon, hoverIcon):
        if (not icon) or isinstance(icon, QIcon):
            self.LeaveIcon = icon
        if (not hoverIcon) or isinstance(hoverIcon, QIcon):
            self.HoverIcon = hoverIcon

    def eventFilter(self, watched, event):
        button = watched
        if button is None:
            return False

        if event.type() == QEvent.Enter and self.HoverIcon:
            button.setIcon(self.HoverIcon)
            return True
        elif event.type() == QEvent.Leave and self.LeaveIcon:
            button.setIcon(self.LeaveIcon)
            return True
        return False

class HoverButton(QPushButton):
    def __init__(self, icon, text, applyStyleSheet, parent=None):
        super(HoverButton, self).__init__(icon, text, parent=parent)

        self.ignoreMouseEvent = False
        self.buttonHoverWatcher = ButtonHoverWatcher.fromIcon(icon, None, parent)
        if applyStyleSheet:
            self.setStyleSheet(ImageUtils.PushButtonStyleSheet)
        self.installEventFilter(self.buttonHoverWatcher)

    @classmethod
    def fromIcon(cls, icon, hoverIcon, text='', applyStyleSheet=True, parent=None):
        if icon and isinstance(icon, QIcon):
            button = HoverButton(icon, text, applyStyleSheet, parent)
            button.buttonHoverWatcher.HoverIcon = hoverIcon
            return button
        return None

    @classmethod
    def fromIconName(cls, iconName, text, applyStyleSheet, parent=None):
        if iconName and isinstance(iconName, str):
            leaveIcon = ImageUtils.createIcon(iconName)
            hoverIcon = ImageUtils.createHoverIcon(leaveIcon, 32)
            return HoverButton.fromIcon(leaveIcon, hoverIcon, text, applyStyleSheet, parent)
        return None

    def replaceIcons(self, icon, hoverIcon):
        if self.buttonHoverWatcher:
            self.buttonHoverWatcher.replaceIcons(icon, hoverIcon)
            self.setIcon(icon)

    def mousePressEvent(self, event):
        if self.ignoreMouseEvent:
            self.clicked.emit()
            event.ignore()
        else:
            super(HoverButton, self).mousePressEvent(event)