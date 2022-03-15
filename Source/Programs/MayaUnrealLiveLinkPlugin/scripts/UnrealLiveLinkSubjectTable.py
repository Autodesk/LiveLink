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

try:
  from PySide2.QtCore import *
  from PySide2.QtGui import *
  from PySide2.QtWidgets import *
  from PySide2 import __version__
except ImportError:
  from PySide.QtCore import *
  from PySide.QtGui import *
  from PySide import __version__

import UnrealLiveLinkWindow

class SubjectLineEdit(QLineEdit):
    textModified = Signal(str, str) # (before, after)

    def __init__(self, table, contents='', parent=None):
        super(SubjectLineEdit, self).__init__(contents, parent)
        self.editingFinished.connect(self.__handleEditingFinished)
        self._before = contents
        self._table = weakref.proxy(table)

    def __del__(self):
        self._table = None

    def __handleEditingFinished(self):
        before, after = self._before, self.text()
        if len(after) == 0:
            self.setText(before)
        elif before != after and self._table:
            finalName = after
            self._before = finalName
            self.textModified.emit(before, finalName)
        self._table.setFocus()

class RowHighlightDelegate(QStyledItemDelegate):
    onHoverIndexChanged = Signal(int)
    onLeaveTable = Signal()

    def __init__(self, table, parent=None):
        super(RowHighlightDelegate, self).__init__(parent)
        self.hoveredRow = -1
        self.Table = weakref.proxy(table)
        defaultColor = table.palette().color(QPalette.Window)
        self.defaultBrush = QBrush(defaultColor)
        self.hoveredBrush = QBrush(defaultColor.darker(115))

    def __del_(self):
        self.Table = None

    def _onHoverIndexChanged(self, index):
        if index != self.hoveredRow:
            self.hoveredRow = index
            self.Table.viewport().update()

    def _onLeaveTableEvent(self):
        self.hoveredRow = -1

    def paint(self, painter, option, index):
        painter.save()

        self.initStyleOption(option, index)

        isSelected = option.state & QStyle.State_Selected
        if isSelected == False:
            if index.row() == self.hoveredRow:
                painter.fillRect(option.rect, self.hoveredBrush)
            else:
                painter.fillRect(option.rect, self.defaultBrush)

        painter.restore()

class ComboBox(QComboBox):
    def __init__(self, parent):
       super(ComboBox, self).__init__(parent)
       self.icons = []
       self.hoverIcons = []

    def initIcons(self, icons, hoverIcons):
        self.icons = icons
        self.hoverIcons = hoverIcons

    def enterEvent(self, event):
        super(ComboBox, self).enterEvent(event)

        currentIndex = self.currentIndex()
        if currentIndex < 0 or currentIndex >= len(self.hoverIcons):
            return
        self.setItemIcon(currentIndex, self.hoverIcons[currentIndex])

    def leaveEvent(self, event):
        super(ComboBox, self).leaveEvent(event)

        currentIndex = self.currentIndex()
        if currentIndex < 0 or currentIndex >= len(self.icons):
            return
        self.setItemIcon(currentIndex, self.icons[currentIndex])

class UnrealLiveLinkSubjectTable(QTableWidget):
    StreamTypesPerSubjectType = {
        'Prop': 		['Root Only', 'Full Hierarchy'],
        'Character':	['Root Only', 'Full Hierarchy'],
        'Camera':		['Root Only', 'Full Hierarchy', 'Camera'],
        'Light':		['Root Only', 'Full Hierarchy', 'Light'],
    }

    IconsPerSubjectType = {
        'Prop': 		['streamTransform', 'streamAll'],
        'Character':	['streamRoot', 'streamAll'],
        'Camera':		['streamTransform', 'streamAll', 'streamCamera'],
        'Light':		['streamTransform', 'streamAll', 'streamLights'],
    }

    hoverIndexChanged = Signal(int)
    leaveTableEvent = Signal()

    def __init__(self, windowParent):
        super(UnrealLiveLinkSubjectTable, self).__init__(0, 3)

        self.windowParent = weakref.proxy(windowParent)
        self.setSelectionBehavior(QAbstractItemView.SelectRows)

        # Load the icons used in the table
        self.Icons = dict()
        self.HoverIcons = dict()
        for subject in UnrealLiveLinkSubjectTable.IconsPerSubjectType:
            iconNames = UnrealLiveLinkSubjectTable.IconsPerSubjectType[subject]
            for iconName in iconNames:
                if iconName not in self.Icons:
                    self.Icons[iconName] = self.windowParent.createIcon(iconName)
                    if self.Icons[iconName]:
                        pixmap = self.Icons[iconName].pixmap(32, 32)
                        highlightPixmap = self.windowParent.highlightPixmap(pixmap)
                        self.HoverIcons[iconName] = QIcon(highlightPixmap)

        self.setHorizontalHeaderLabels(['Type', 'Object Name', 'DAG Path'])
        self.horizontalHeader().setDefaultSectionSize(250)
        self.verticalHeader().hide()
        self.horizontalHeader().setSectionResizeMode(0, QHeaderView.Fixed)
        dpiScale = windowParent.Controller.getDpiScale(60)
        self.setColumnWidth(0, 60 + max(0, int((dpiScale - 60) * 0.5)))
        self.horizontalHeader().setSectionResizeMode(2, QHeaderView.Stretch)
        self.emptyLayout = QHBoxLayout()
        self.emptyLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))
        self.emptyLabel = QLabel('To stream a subject to Unreal:<br>1. Select an object in <i>Maya</i>.</pre><br>2. Click <i>Add Selection.</i></pre>')
        self.emptyLabel.setTextFormat(Qt.RichText)
        self.emptyLayout.addWidget(self.emptyLabel)
        self.emptyLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))
        self.setLayout(self.emptyLayout)

        self.setStyleSheet('QTableWidget { background-color: ' +
                           windowParent.palette().color(QPalette.Window).darker(125).name() +
                           '; } QTableWidget::item { padding: 3px }')

        self.setMouseTracking(True)
        self.rowDelegate = RowHighlightDelegate(self)

        self.mouseMoveEvent = self._mouseMoveEvent
        self.leaveEvent = self._leaveEvent
        self.hoverIndexChanged.connect(self.rowDelegate._onHoverIndexChanged)
        self.leaveTableEvent.connect(self.rowDelegate._onLeaveTableEvent)
        self.itemSelectionChanged.connect(lambda: self.windowParent.deleteSelectionButton.setEnabled(len(self.selectedIndexes()) > 0))

        self.subjectList = []
        self.subjectTypes = dict()
        self.cellMouseHover = [-1, -1]

    def __del__(self):
        self.windowParent = None

    def keyPressEvent(self, event):
        if event.key() == Qt.Key_Delete:
            self._removeRow()
        else:
            if self.windowParent.Controller.isDocked():
                super(UnrealLiveLinkSubjectTable, self).keyPressEvent(event)

    def _mouseMoveEvent(self, event):
        modelIndex = self.indexAt(event.pos())
        rowIndex = modelIndex.row()
        columnIndex = modelIndex.column()

        if rowIndex != self.cellMouseHover[0]:
            self.hoverIndexChanged.emit(rowIndex)

        self.cellMouseHover = [rowIndex, columnIndex]

        if rowIndex == -1 or columnIndex == -1:
            self._leaveEvent(None)

    def _leaveEvent(self, event):
        self.cellMouseHover = [-1, -1]
        self.leaveTableEvent.emit()
        self.viewport().update()

    def _addRow(self):
        alreadyInList = self.windowParent.Controller.addSelection()
        self._refreshUI()
        if alreadyInList:
            self.windowParent.logMessage('One or more objects you have selected have already been added to the table.')

    def _clearUI(self):
        self.subjectList = []
        self.subjectTypes.clear()
        self.setRowCount(0)

    def _refreshUI(self):
        addedSelections = self.windowParent.Controller.refreshSubjects()

        if len(addedSelections) == 4 and all(addedSelections):
            self.subjectList = list(addedSelections[0])

            for (SubjectName, SubjectPath, SubjectType, SubjectRole) in zip(
                 addedSelections[0], addedSelections[1], addedSelections[2], addedSelections[3]):

                self.subjectTypes[SubjectPath] = SubjectType

                # Make sure not to add back subjects we already have in the list
                subjectAlreadyInList = False
                for rowIndex in range(self.rowCount()):
                    lineEdit = self.cellWidget(rowIndex, 1)
                    pathLineEdit = self.cellWidget(rowIndex, 2)
                    if SubjectName == lineEdit.text() and SubjectPath == pathLineEdit.text():
                        subjectAlreadyInList = True
                        break
                if subjectAlreadyInList:
                    continue

                # Add new row for the subject
                rowCount = self.rowCount()
                self.insertRow(rowCount)
                self.setRowHeight(rowCount, 32)
                self.setItemDelegateForRow(rowCount, self.rowDelegate)

                # Icon dropdown to display the stream type/role
                iconComboBox = ComboBox(self)
                if self.windowParent:
                    iconNames = UnrealLiveLinkSubjectTable.IconsPerSubjectType[SubjectType]
                    normalIcons = []
                    hoverIcons = []
                    for item in UnrealLiveLinkSubjectTable.StreamTypesPerSubjectType[SubjectType]:
                        iconName = iconNames[iconComboBox.count()]
                        iconComboBox.addItem(self.Icons[iconName], '  ' + item)
                        normalIcons.append(self.Icons[iconName])
                        hoverIcons.append(self.HoverIcons[iconName])
                    selectedItem = UnrealLiveLinkSubjectTable.StreamTypesPerSubjectType[SubjectType].index(SubjectRole)
                    iconComboBox.setCurrentIndex(selectedItem)
                    iconComboBox.initIcons(normalIcons, hoverIcons)
                iconComboBox.setIconSize(QSize(24, 24))
                iconComboBox.setStyleSheet('''QComboBox { background-color: transparent; }
                                              QComboBox:item::hover { background-color: #FF0000; }
                                              QComboBox:hover { color: palette(light|text ); }''')
                iconComboBox.setMinimumWidth(60)
                iconComboBox.view().setMinimumWidth(200)
                iconComboBox.currentIndexChanged.connect(lambda x, type=SubjectType, dagPath=SubjectPath: self._changeSubjectRole(x, type, dagPath))
                self.setCellWidget(rowCount, 0, iconComboBox)

                # Subject name
                lineEdit = SubjectLineEdit(self, SubjectName)
                lineEdit.textModified.connect(lambda old, new, dagPath=SubjectPath: self._changeSubjectName(old, new, dagPath))
                self.setCellWidget(rowCount, 1, lineEdit)

                # DAG path
                label = QLabel()
                label.setText(SubjectPath)
                self.setCellWidget(rowCount, 2, label)

            # Hide the instructions when there is at least one subject in the list
            if self.rowCount() > 0:
                self.emptyLabel.hide()

    def _removeRow(self):
        if self.rowCount() > 0:
            # Get the selected rows
            rowIndices = set()
            for index in self.selectedIndexes():
                rowIndices.add(index.row())

            # Remove each subject based on its DAG path
            for rowIndex in rowIndices:
                frame = self.cellWidget(rowIndex, 2)
                dagPath = frame.text()
                del self.subjectTypes[dagPath]
                self.windowParent.Controller.removeSubject(dagPath)

            # Remove the selected rows going in reverse order to make sure that row indices stay valid
            for rowIndex in sorted(rowIndices, reverse=True):
                self.removeRow(rowIndex)

        self._updateSubjectList()

        # Show the instructions when there is no subject in the list
        if self.rowCount() == 0:
            self.emptyLabel.show()

    def _changeSubjectName(self, oldName, newName, dagPath):
        if oldName != newName:
            self.windowParent.Controller.changeSubjectName(dagPath, newName)
            self._clearUI()
            self._refreshUI()

    def _updateSubjectList(self):
        addedSelections = self.windowParent.Controller.refreshSubjects()
        if len(addedSelections) == 4 and all(addedSelections):
            self.subjectList = list(addedSelections[0])

    def _changeSubjectRole(self, roleIndex, subjectRole, dagPath):
        if subjectRole not in UnrealLiveLinkSubjectTable.StreamTypesPerSubjectType:
            return

        streamTypes = UnrealLiveLinkSubjectTable.StreamTypesPerSubjectType[subjectRole]
        if roleIndex < len(streamTypes) and self.windowParent.Controller:
            self.windowParent.Controller.changeSubjectRole(dagPath, streamTypes[roleIndex])
