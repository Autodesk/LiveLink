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

from HoverButton import *


class ListView(QListView):
    def __init__(self, parent=None):
        super(ListView, self).__init__(parent=parent)
        self.maxRows = 10
        self.minWidth = 200

    def minimumSizeHint(self):
        size = super(ListView, self).minimumSizeHint()

        rowCount = self.model().rowCount()
        if rowCount == 0:
            return size

        # Compute the width
        _width = 0
        for i in range(rowCount):
            _width = max(_width, self.sizeHintForColumn(i))
        _width += 2 * self.verticalScrollBar().sizeHint().width()
        _width = max(self.minWidth, _width)

        # Compute the height
        _height = min(rowCount, self.maxRows) * (self.sizeHintForRow(0) + self.spacing() * 2)

        return QSize(_width, _height)

    def sizeHint(self):
        size = super(ListView, self).sizeHint()

        if self.model().rowCount() == 0:
            return size

        return self.minimumSizeHint()

class FilterListPanel(QWidget):
    itemChanged = Signal(QStandardItem)

    def __init__(self, parent=None):
        super(FilterListPanel, self).__init__(parent)
        self.wndShadow = QGraphicsDropShadowEffect()
        self.wndShadow.setBlurRadius(9.0);
        self.wndShadow.setColor(QColor(0, 0, 0, 160))
        self.wndShadow.setOffset(4.0)
        self.setGraphicsEffect(self.wndShadow)

        self.setContentsMargins(5, 0, 5, 2)
        self.setAutoFillBackground(True)

        # Layout
        layout = QVBoxLayout()
        if usingPyside6:
            layout.setContentsMargins(0, 0, 0, 0)
        else:
            layout.setMargin(0)
        layout.setSpacing(2)

        # Label
        self.label = QLabel()
        self.label.setContentsMargins(0, 10, 0, 5)
        self.label.setStyleSheet('QLabel { font: bold; }')
        layout.addWidget(self.label)

        # "All" checkbox
        self._selectAllButton = QCheckBox('All')
        self._selectAllButton.setStyleSheet('QCheckBox { padding-left: 2px; }')
        self._selectAllButton.stateChanged.connect(self._onAllButtonStateChanged)
        layout.addWidget(self._selectAllButton)

        # Separator
        separator = QFrame()
        separator.setFrameShape(QFrame.HLine)
        separator.setStyleSheet('background-color: rgb(86, 86, 86)')
        separator.setFixedHeight(2)
        layout.addWidget(separator)

        # ListView
        self._model = QStandardItemModel()
        self._model.itemChanged.connect(self._onItemChanged)
        self._listView = ListView()
        self._listView.setStyleSheet('QListView { border: none; }')
        self._listView.setModel(self._model)
        self._listView.viewport().setAutoFillBackground(False)
        self._listView.setSelectionMode(QAbstractItemView.NoSelection)
        self._listView.setFocusPolicy(Qt.NoFocus)
        self._listView.setSpacing(3)
        layout.addWidget(self._listView)

        self._checkedCount = 0

        self.setLayout(layout)

    def addToList(self, text):
        if text is None or len(text) == 0:
            return

        item = QStandardItem(text)
        item.setFlags(item.flags() | Qt.ItemIsUserCheckable | Qt.ItemIsEnabled)
        item.setCheckState(Qt.Unchecked)
        item.setEditable(False)
        self._model.appendRow(item)

    def clearList(self):
        self._model.clear()
        self._selectAllButton.setChecked(False)

    def checkedCount(self):
        return self._checkedCount

    def getCheckedItems(self):
        return [self._model.item(r, 0).text() for r in range(self._model.rowCount()) if self._model.item(r, 0).checkState() == Qt.Checked]

    def leaveEvent(self, event):
        super(FilterListPanel, self).leaveEvent(event)
        self.hide()

    def _onItemChanged(self, item):
        if item.checkState() == Qt.Unchecked and self._selectAllButton.isChecked():
            self._selectAllButton.blockSignals(True)
            self._selectAllButton.setChecked(False)
            self._selectAllButton.blockSignals(False)

        if item.checkState() == Qt.Unchecked:
            self._checkedCount -= 1
        else:
            self._checkedCount += 1

        self._checkedCount = min(max(0, self._checkedCount), self._listView.model().rowCount())
        self.itemChanged.emit(item)

    def _checkAllItems(self, checkState):
        rowCount = self._model.rowCount()
        for index in range(rowCount):
            self._model.item(index, 0).setCheckState(checkState)

    def _onAllButtonStateChanged(self, state):
        self._checkAllItems(Qt.CheckState(state))
