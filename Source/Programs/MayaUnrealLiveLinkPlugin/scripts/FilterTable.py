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

from TableWidget import TableWidget

class FilterItemDelegate(QStyledItemDelegate):
    def __init__(self, parent=None):
        super(FilterItemDelegate, self).__init__(parent)
        self.doc = QTextDocument(self)
        self._textFilter = ""
        self._columnsToFilter = []
        self._filterHighlightColor = QColor.fromRgb(255, 255, 255)

    def setColumnsToFilter(self, columns):
        if columns is None:
            return
        elif isinstance(columns, list):
            self._columnsToFilter = columns
        elif isinstance(columns, int):
            self._columnsToFilter.clear()
            self._columnsToFilter.append(columns)

    def setFilterHighlightColor(self, color):
        if color and isinstance(color, QColor):
            self._filterHighlightColor = color

    def setTextFilter(self, text):
        self._textFilter = text

    def paint(self, painter, option, index):
        painter.save()

        options = QStyleOptionViewItem(option)
        self.initStyleOption(options, index)
        self.doc.setPlainText(options.text)

        # Apply the filter highlight
        if len(self._columnsToFilter) > 0 and index.column() in self._columnsToFilter:
            self.applyFilterHighlight(option)

        options.text = ""
        style = QApplication.style() if options.widget is None else options.widget.style()
        style.drawControl(QStyle.CE_ItemViewItem, options, painter)

        ctx = QAbstractTextDocumentLayout.PaintContext()
        textStyle = None
        if option.state & QStyle.State_Selected:
            textStyle = QPalette.HighlightedText
        elif not (option.state & QStyle.State_Enabled):
            textStyle = QPalette.Window
        else:
            textStyle = QPalette.Text
        ctx.palette.setColor(QPalette.Text,
                             option.palette.color(QPalette.Active, textStyle))

        textRect = style.subElementRect(QStyle.SE_ItemViewItemText, options)

        margin = (option.rect.height() - options.fontMetrics.height()) // 2
        textRect.setTop(textRect.top() + margin)

        painter.translate(textRect.topLeft())
        painter.setClipRect(textRect.translated(-textRect.topLeft()))
        self.doc.documentLayout().draw(painter, ctx)

        painter.restore()

    def applyFilterHighlight(self, option):
        if len(self._textFilter) == 0:
            return

        cursor = QTextCursor(self.doc)
        cursor.beginEditBlock()

        format = QTextCharFormat()
        format.setForeground(self._filterHighlightColor.lighter(175) if option.state & QStyle.State_Selected else \
                             self._filterHighlightColor)

        highlightCursor = QTextCursor(self.doc)
        while not highlightCursor.isNull() and not highlightCursor.atEnd():
            highlightCursor = self.doc.find(self._textFilter, highlightCursor)
            if not highlightCursor.isNull():
                highlightCursor.mergeCharFormat(format)

        cursor.endEditBlock()

class FilterTable(TableWidget):
    def __init__(self, rows, columns, parent=None):
        super(FilterTable, self).__init__(rows, columns, parent)
        self.setFocusPolicy(Qt.NoFocus)

        self._delegate = FilterItemDelegate(self)
        label = QLabel()
        self.setFilterHighlightColor(label.palette().color(QPalette.Highlight))

        self.setItemDelegate(self._delegate)
        self.setShowGrid(False)
        self._columnsToFilter = []
        self._filterList = []

    def setColumnsToFilterForHighlight(self, columns):
        self._delegate.setColumnsToFilter(columns)

    def setColumnsToFilter(self, columns):
        if columns is None:
            return
        elif isinstance(columns, list):
            self._columnsToFilter = columns
        elif isinstance(columns, int):
            self._columnsToFilter.clear()
            self._columnsToFilter.append(columns)

    def setFilterHighlightColor(self, color):
        if color and isinstance(color, QColor):
            self._delegate.setFilterHighlightColor(color)

    def searchFilter(self, textHighlight, names):
        rowCount = self.rowCount()
        if rowCount == 0:
            return

        columnCount = self.columnCount()
        if columnCount == 0:
            return

        if len(self._delegate._columnsToFilter) == 0:
            self._delegate.setColumnsToFilter(list(range(columnCount)))

        self._delegate.setTextFilter(textHighlight)

        textLowerCase = textHighlight.lower()
        matchLen = len(textHighlight)
        namesLen = len(names)

        for index in range(rowCount):
            # Hide the row if it's not part of the filter list
            assetPath = ""
            itemCol2 = self.item(index, 2)
            if len(itemCol2.text()) > 0:
                assetPath = itemCol2.text() + '/'
            assetPath += self.item(index, 0).text()

            rowHidden = len(self._filterList) > 0 and (not (assetPath in self._filterList))
            self.setRowHidden(index, rowHidden)

            if rowHidden:
                continue

            # Match the text highlight string
            matchFound = False
            nameFound = False
            matchIndex = -1
            for column in self._delegate._columnsToFilter:
                if column < 0 or column >= columnCount:
                    continue

                item = self.item(index, column)
                itemText = item.text()
                if matchLen > 0:
                    itemTextLowerCase = itemText.lower()
                    matchIndex = itemTextLowerCase.find(textLowerCase)
                    if matchIndex >= 0:
                        matchFound = True
                        break
                else:
                    matchFound = True

            if namesLen > 0:
                for name in names:
                    for column in self._columnsToFilter:
                        if column < 0 or column >= columnCount:
                            continue

                        item = self.item(index, column)
                        itemText = item.text()
                        if item.text() == name:
                            nameFound = True
                            break
            else:
                nameFound = True

            self.setRowHidden(index, not (matchFound and nameFound))

    def updateFilterList(self, filterList):
        if filterList is None:
            return
        self._filterList = filterList

    def filterByList(self, filterList):
        rowCount = self.rowCount()
        if rowCount == 0:
            return

        columnCount = self.columnCount()
        if columnCount < 2:
            return

        self._filterList = filterList

        for index in range(rowCount):
            assetPath = ""
            itemCol2 = self.item(index, 2)
            if len(itemCol2.text()) > 0:
                assetPath = itemCol2.text() + '/'
            assetPath += self.item(index, 0).text()

            self.setRowHidden(index, not (assetPath in self._filterList))
