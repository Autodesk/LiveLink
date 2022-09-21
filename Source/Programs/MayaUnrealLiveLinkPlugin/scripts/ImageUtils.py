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
  from PySide2 import __version__
except ImportError:
  from PySide.QtCore import *
  from PySide.QtGui import *
  from PySide import __version__

class ImageUtils:
    iconPath = ''

    PushButtonStyleSheetOpaque = '''QPushButton {
                                      border: 1px;
                                      background-color: palette(button);
                                      }'''
    PushButtonStyleSheet = '''QPushButton {
                                border: 1px;
                                background-color: transparent;
                                }
                              QPushButton:checked, QPushButton:pressed {
                                background-color: palette(highlight);
                                }'''
    @staticmethod
    def setIconPath(path):
        ImageUtils.iconPath = path

    @staticmethod
    def createIcon(iconName):
        icon = QIcon()
        availableSizes = ['1', '2', '3']
        missingIcon = True
        for size in availableSizes:
            path = os.path.join(ImageUtils.iconPath, iconName + '@' + size + 'x.png')
            if os.path.isfile(path):
                icon.addFile(path)
                missingIcon = False
        if missingIcon:
            try:
                internalIcon = QIcon(iconName)
                if internalIcon:
                    icon = internalIcon
            except:
                pass
        return icon

    @staticmethod
    def createHoverIcon(icon, size=32):
        if icon and isinstance(icon, QIcon):
                return QIcon(ImageUtils.highlightPixmap(icon.pixmap(size, size)))
        return None

    @staticmethod
    def highlightPixmap(pixmap):
        if not isinstance(pixmap, QPixmap):
            return None

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
