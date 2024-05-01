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

import weakref

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

class TableWidget(QTableWidget):
    def __init__(self, rows, columns, parent=None):
        super(TableWidget, self).__init__(rows, columns, parent=parent)
        self.gridColor = QColor(38, 38, 38)

    def paintEvent(self, event):
        super(TableWidget, self).paintEvent(event)

        if self.horizontalHeader().count() == 0 or self.verticalHeader().count() == 0:
            return

        painter = QPainter(self.viewport())
        option = QStyleOptionViewItem()
        if usingPyside6:
            option.font = self.font()
            option.rect = self.rect()
            option.palette = self.palette()
        else:
            option.init(self)
        gridPen = QPen(self.gridColor, 0, self.gridStyle())
        painter.setPen(gridPen);

        w = self.horizontalHeader().offset()
        for i in range(self.horizontalHeader().count()):
            w += self.horizontalHeader().sectionSize(i)
            painter.drawLine(w, 0, w, self.viewport().height())
