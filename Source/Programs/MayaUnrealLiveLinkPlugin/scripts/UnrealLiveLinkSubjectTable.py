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
from UnrealLiveLinkAssetSelection import UnrealLiveLinkAssetSelection
from HoverButton import HoverButton
from ImageUtils import ImageUtils
from TableWidget import TableWidget

class SubjectLineEdit(QLineEdit):
    textModified = Signal(str, str) # (before, after)

    def __init__(self, table, contents='', parent=None):
        super(SubjectLineEdit, self).__init__(contents, parent)
        self.editingFinished.connect(self.__handleEditingFinished)
        self._before = contents
        self._table = weakref.proxy(table)
        self.setFrame(False)

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

class UnrealLiveLinkSubjectTable(TableWidget):
    SubjectTypeMapping = {
        'Prop': {
            'Transform': {
                'Icon' : 'streamTransform',
                'TargetAsset': 'LevelSequence',
                'LevelAsset' : True,
                'LinkAllowed' : True
                },
            'Animation': {
                'Icon' : 'streamAll',
                'TargetAsset': 'AnimSequence',
                'LevelAsset' : False,
                'LinkAllowed' : False
                }
            },
        'Character': {
            'Transform': {
                'Icon' : 'streamRoot',
                'TargetAsset': 'LevelSequence',
                'LevelAsset' : True,
                'LinkAllowed' : False
                },
            'Animation': {
                'Icon' : 'streamAll',
                'TargetAsset': 'AnimSequence',
                'LevelAsset' : False,
                'LinkAllowed' : True
                }
            },
        'Camera': {
            'Transform': {
                'Icon' : 'streamTransform',
                'TargetAsset': 'LevelSequence',
                'LevelAsset' : True,
                'LinkAllowed' : False
                },
            'Animation': {
                'Icon' : 'streamAll',
                'TargetAsset': 'AnimSequence',
                'LevelAsset' : False,
                'LinkAllowed' : False
                },
            'Camera': {
                'Icon' : 'streamCamera',
                'TargetAsset': 'LevelSequence',
                'LevelAsset' : True,
                'LinkAllowed' : True
                }
            },
        'Light': {
            'Transform': {
                'Icon' : 'streamTransform',
                'TargetAsset': 'LevelSequence',
                'LevelAsset' : True,
                'LinkAllowed' : False
                },
            'Animation': {
                'Icon' : 'streamAll',
                'TargetAsset': 'AnimSequence',
                'LevelAsset' : False,
                'LinkAllowed' : False
                },
            'Light': {
                'Icon' : 'streamLights',
                'TargetAsset': 'LevelSequence',
                'LevelAsset' : True,
                'LinkAllowed' : True
                }
            }
        }

    hoverIndexChanged = Signal(int)
    leaveTableEvent = Signal()

    def __init__(self, windowParent):
        super(UnrealLiveLinkSubjectTable, self).__init__(0, 6)

        self.windowParent = weakref.proxy(windowParent)
        self.setSelectionBehavior(QAbstractItemView.SelectRows)

        # Load the icons used in the table
        self.Icons = dict()
        self.HoverIcons = dict()
        for subject in UnrealLiveLinkSubjectTable.SubjectTypeMapping:
            role = UnrealLiveLinkSubjectTable.SubjectTypeMapping[subject]
            for streamType in role:
                iconName = role[streamType]['Icon']
                if iconName not in self.Icons:
                    self.Icons[iconName] = ImageUtils.createIcon(iconName)
                    if self.Icons[iconName]:
                        pixmap = self.Icons[iconName].pixmap(32, 32)
                        highlightPixmap = ImageUtils.highlightPixmap(pixmap)
                        self.HoverIcons[iconName] = QIcon(highlightPixmap)

        self.linkedIcon = ImageUtils.createIcon('linkConnected')
        self.linkedHoverIcon = ImageUtils.createHoverIcon(self.linkedIcon)
        self.unlinkedIcon = ImageUtils.createIcon('linkDisconnected')
        self.unlinkedHoverIcon = ImageUtils.createHoverIcon(self.unlinkedIcon)
        self.linkPendingIcon = ImageUtils.createIcon('linkConnectedWarning')
        self.linkPendingHoverIcon = ImageUtils.createHoverIcon(self.linkPendingIcon)
        self.linkPausedIcon = ImageUtils.createIcon('linkConnectedPaused')
        self.linkPausedHoverIcon = ImageUtils.createHoverIcon(self.linkPausedIcon)
        self.linkEditIcon = ImageUtils.createIcon('edit')
        self.linkEditHoverIcon = ImageUtils.createHoverIcon(self.linkEditIcon)

        self.setHorizontalHeaderLabels(['Type', 'Object Name', 'DAG Path', 'Linked asset', 'Sequence', 'Link'])
        self.horizontalHeader().setDefaultSectionSize(250)
        self.horizontalHeader().setDefaultAlignment(Qt.AlignLeft)
        self.verticalHeader().hide()
        self.horizontalHeader().setSectionResizeMode(0, QHeaderView.Fixed)
        self.setColumnWidth(0, windowParent.Controller.getDpiScale(40))
        self.horizontalHeaderItem(0).setTextAlignment(Qt.AlignCenter)
        self.horizontalHeader().setSectionResizeMode(4, QHeaderView.Stretch)
        self.horizontalHeader().setSectionResizeMode(5, QHeaderView.Fixed)
        self.setColumnWidth(5, windowParent.Controller.getDpiScale(52))
        self.horizontalHeaderItem(5).setTextAlignment(Qt.AlignCenter)
        self.emptyLayout = QHBoxLayout()
        self.emptyLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))
        self.emptyLabel = QLabel('To stream a subject to Unreal:<br>1. Select an object in <i>Maya</i>.</pre><br>2. Click <i>Add Selection.</i></pre>')
        self.emptyLabel.setTextFormat(Qt.RichText)
        self.emptyLayout.addWidget(self.emptyLabel)
        self.emptyLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))
        self.setLayout(self.emptyLayout)

        self.setStyleSheet('QTableWidget { outline: 0 } QTableWidget::item { padding: 1px } QTableWidget::item:focus { border: 0px; }')
        self.setShowGrid(False)

        self.itemSelectionChanged.connect(lambda: self.windowParent.deleteSelectionButton.setEnabled(len(self.selectedIndexes()) > 0))

        self.linkedAssets = dict()
        self.targetAssets = dict()
        self.subjectTypes = dict()
        self.assetsCurrentlyLinked = set()
        self.charactersForAnimSeqStreaming = set()

    def __del__(self):
        self.windowParent = None

    def keyPressEvent(self, event):
        if event.key() == Qt.Key_Delete:
            self._removeRow()
        else:
            if self.windowParent.Controller.isDocked():
                super(UnrealLiveLinkSubjectTable, self).keyPressEvent(event)

    def mousePressEvent(self, event):
        super(UnrealLiveLinkSubjectTable, self).mousePressEvent(event)

        modelIndex = self.indexAt(event.pos())
        columnIndex = modelIndex.column()

        # Trigger a pressed event when clicking on the Link button
        if columnIndex == self.columnCount() - 1:
            rowIndex = modelIndex.row()
            widget = self.cellWidget(rowIndex, columnIndex)
            editButtonPressed = event.pos().x() < (widget.x() + widget.width() // 2)
            self._onLinkButtonPressed(rowIndex, editButtonPressed)

    def leaveEvent(self, event):
        if event:
            super(UnrealLiveLinkSubjectTable, self).leaveEvent(event)
        self.leaveTableEvent.emit()
        self.viewport().update()

    def connectionStateChanged(self, state):
        rowCount = self.rowCount()
        for rowIndex in range(rowCount):
            linkAllowed = self._isLinkedAllowed(rowIndex)
            groupWidget = self.cellWidget(rowIndex, 5)
            linkButton = groupWidget.layout().itemAt(1).widget()
            linkButton.setEnabled(linkAllowed and (state or (self.cellWidget(rowIndex, 2).text() in self.assetsCurrentlyLinked)))
            editButton = groupWidget.layout().itemAt(0).widget()
            editButton.setEnabled(linkAllowed and state and len(self.cellWidget(rowIndex, 3).text()) > 0 and len(self.cellWidget(rowIndex, 4).text()) > 0)

    def _isLinkedAllowed(self, rowIndex):
        linkAllowed = False
        subjectPath = self.cellWidget(rowIndex, 2).text()
        if subjectPath in self.subjectTypes:
            subjectType = self.subjectTypes[subjectPath]
            if subjectType in UnrealLiveLinkSubjectTable.SubjectTypeMapping:
                subject = UnrealLiveLinkSubjectTable.SubjectTypeMapping[subjectType]
                subjectRole = self.cellWidget(rowIndex, 0).currentText().strip()
                if subjectRole in subject:
                    linkAllowed = subject[subjectRole]['LinkAllowed']
        return linkAllowed

    def _addRow(self):
        alreadyInList = self.windowParent.Controller.addSelection()
        self._refreshUI()
        if alreadyInList:
            self.windowParent.logMessage('One or more objects you have selected have already been added to the table.')

    def _clearUI(self):
        self.linkedAssets.clear()
        self.targetAssets.clear()
        self.subjectTypes.clear()
        self.setRowCount(0)

    def _refreshUI(self):
        addedSelections = self.windowParent.Controller.refreshSubjects()

        if len(addedSelections) >= 9 and all(addedSelections):
            self.linkedAssets.clear()
            self.targetAssets.clear()

            self.charactersForAnimSeqStreaming.clear()
            self.assetsCurrentlyLinked.clear()

            size8 = self.windowParent.Controller.getDpiScale(8)
            size16 = self.windowParent.Controller.getDpiScale(16)
            size24 = self.windowParent.Controller.getDpiScale(24)
            size32 = self.windowParent.Controller.getDpiScale(32)
            for (SubjectName, SubjectPath, SubjectType, SubjectRole, LinkedAsset, TargetAsset, LinkStatus, Class, UnrealNativeClass) in zip(
                 addedSelections[0], addedSelections[1], addedSelections[2], addedSelections[3], addedSelections[4], addedSelections[5], addedSelections[6], addedSelections[7], addedSelections[8]):

                self.subjectTypes[SubjectPath] = SubjectType
                linkPaused = False

                if SubjectType == 'Character' and SubjectRole == 'Animation':
                    self.charactersForAnimSeqStreaming.add(SubjectPath)
                    linkPaused = self.windowParent.isPauseAnimSeqSyncEnabled()

                isLinked = LinkStatus == '1'
                if isLinked:
                    self.assetsCurrentlyLinked.add(SubjectPath)

                # Make sure not to add back subjects we already have in the list
                subjectAlreadyInList = False
                for rowIndex in range(self.rowCount()):
                    lineEdit = self.cellWidget(rowIndex, 1)
                    pathLineEdit = self.cellWidget(rowIndex, 2)
                    if SubjectName == lineEdit.text() and SubjectPath == pathLineEdit.text():
                        subjectAlreadyInList = True
                        break
                if subjectAlreadyInList:
                    if len(LinkedAsset) > 0:
                        nameIndex = LinkedAsset.rfind('/')
                        if nameIndex >= 0:
                            self.linkedAssets[str(SubjectPath)] = (str(LinkedAsset[:nameIndex]), str(LinkedAsset[nameIndex+1:]), isLinked, Class, UnrealNativeClass)

                    if len(TargetAsset) > 0:
                        nameIndex = TargetAsset.rfind('/')
                        if nameIndex >= 0 and TargetAsset != '/':
                            self.targetAssets[str(SubjectPath)] = (str(TargetAsset[:nameIndex]), str(TargetAsset[nameIndex+1:]), isLinked)
                    continue

                # Add new row for the subject
                rowCount = self.rowCount()
                self.insertRow(rowCount)
                self.setRowHeight(rowCount, size32)

                # Icon dropdown to display the stream type/role
                iconComboBox = ComboBox(self)
                linkAllowed = False
                if self.windowParent:
                    subject = UnrealLiveLinkSubjectTable.SubjectTypeMapping[SubjectType]
                    normalIcons = []
                    hoverIcons = []
                    for item in subject:
                        role = subject[item]
                        iconName = subject[item]['Icon']
                        iconComboBox.addItem(self.Icons[iconName], '  ' + item)
                        normalIcons.append(self.Icons[iconName])
                        hoverIcons.append(self.HoverIcons[iconName])
                    selectedItem = list(subject.keys()).index(SubjectRole)
                    linkAllowed = subject[SubjectRole]['LinkAllowed']
                    iconComboBox.setCurrentIndex(selectedItem)
                    iconComboBox.initIcons(normalIcons, hoverIcons)
                iconComboBox.setIconSize(QSize(size24, size24))
                iconComboBox.setStyleSheet('''QComboBox::down-arrow {
                                                image: url(''' + ImageUtils.iconPath + '''/downarrow.png);
                                                width: ''' + str(size8) + '''px;
                                                height: ''' + str(size8) + '''px;
                                            }
                                            QComboBox::drop-down {
                                                   border: 0px;
                                                   subcontrol-origin: padding;
                                                   subcontrol-position: right bottom;
                                                   width: ''' + str(size16) + '''px;
                                                   height: ''' + str(size16) + '''px;
                                            }

                                            QComboBox {
                                                border: 0px;
                                                spacing: 0px;
                                                padding: 0px;
                                                background-color: transparent;
                                            }''')


                iconComboBox.setMinimumWidth(self.windowParent.Controller.getDpiScale(40))
                iconComboBox.view().setMinimumWidth(self.windowParent.Controller.getDpiScale(200))
                iconComboBox.currentIndexChanged.connect(lambda x, type=SubjectType, dagPath=SubjectPath: self._changeSubjectType(x, type, dagPath))
                self.setCellWidget(rowCount, 0, iconComboBox)

                # Subject name
                lineEdit = SubjectLineEdit(self, SubjectName)
                lineEdit.setStyleSheet('margin: 2px')
                lineEdit.textModified.connect(lambda old, new, dagPath=SubjectPath: self._changeSubjectName(old, new, dagPath))
                self.setCellWidget(rowCount, 1, lineEdit)

                # DAG path
                label = QLabel(self)
                label.setStyleSheet('margin: 2px')
                label.setText(SubjectPath)
                label.setSizePolicy(QSizePolicy.Ignored, QSizePolicy.Ignored)
                self.setCellWidget(rowCount, 2, label)

                # Linked asset
                linkIcon = None
                linkHoverIcon = None
                label = QLabel(self)
                label.setStyleSheet('margin: 2px')
                if len(LinkedAsset) > 0:
                    nameIndex = LinkedAsset.rfind('/')
                    if nameIndex >= 0:
                        if LinkedAsset != '/':
                            label.setToolTip(LinkedAsset)
                        self.linkedAssets[str(SubjectPath)] = (str(LinkedAsset[:nameIndex]), str(LinkedAsset[nameIndex+1:]), isLinked, Class, UnrealNativeClass)
                        LinkedAsset = LinkedAsset[nameIndex+1:]
                if isLinked:
                    linkIcon = self.linkPausedIcon if linkPaused else self.linkedIcon
                    linkHoverIcon = self.linkPausedHoverIcon if linkPaused else self.linkedHoverIcon
                else:
                    linkIcon = self.unlinkedIcon
                    linkHoverIcon = self.unlinkedHoverIcon
                label.setText(LinkedAsset)
                label.setSizePolicy(QSizePolicy.Ignored, QSizePolicy.Ignored)
                label.setEnabled(isLinked)
                self.setCellWidget(rowCount, 3, label)

                # Target asset
                label = QLabel(self)
                label.setStyleSheet('margin: 2px')
                if len(TargetAsset) > 0:
                    nameIndex = TargetAsset.rfind('/')
                    if nameIndex >= 0:
                        if TargetAsset != '/':
                            label.setToolTip(TargetAsset)
                            self.targetAssets[str(SubjectPath)] = (str(TargetAsset[:nameIndex]), str(TargetAsset[nameIndex+1:]), isLinked)
                        TargetAsset = TargetAsset[nameIndex+1:]
                label.setText(TargetAsset)
                label.setSizePolicy(QSizePolicy.Ignored, QSizePolicy.Ignored)
                label.setEnabled(isLinked)
                self.setCellWidget(rowCount, 4, label)

                # Edit/Link button group
                widget = QWidget()
                widget.setContentsMargins(0, 0, 0, 0)
                hlayout = QHBoxLayout()
                hlayout.setSpacing(0)
                hlayout.setContentsMargins(0, 0, 0, 0)
                button = HoverButton.fromIcon(self.linkEditIcon, self.linkEditHoverIcon, '', widget)
                button.ignoreMouseEvent = True
                button.setIconSize(QSize(size24, size24))
                linkAllowedAndConnected = linkAllowed and self.windowParent.ConnectedState
                button.setEnabled(linkAllowedAndConnected and (SubjectPath in self.linkedAssets) and (SubjectPath in self.targetAssets))
                hlayout.addWidget(button)
                button = HoverButton.fromIcon(linkIcon, linkHoverIcon, '', widget)
                button.ignoreMouseEvent = True
                button.setIconSize(QSize(size24, size24))
                button.setEnabled(linkAllowedAndConnected)
                hlayout.addWidget(button)
                widget.setLayout(hlayout)
                self.setCellWidget(rowCount, 5, widget)

            # Hide the instructions when there is at least one subject in the list
            if self.rowCount() > 0:
                self.emptyLabel.hide()

            self._updatePauseAnimSyncButtonState()

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
                if dagPath in self.charactersForAnimSeqStreaming:
                    self.charactersForAnimSeqStreaming.remove(dagPath)
                self.windowParent.Controller.removeSubject(dagPath)
                if dagPath in self.linkedAssets:
                    del self.linkedAssets[dagPath]
                if dagPath in self.targetAssets:
                    del self.targetAssets[dagPath]

            # Remove the selected rows going in reverse order to make sure that row indices stay valid
            for rowIndex in sorted(rowIndices, reverse=True):
                self.removeRow(rowIndex)

            self._updatePauseAnimSyncButtonState()

        # Show the instructions when there is no subject in the list
        if self.rowCount() == 0:
            self.emptyLabel.show()

    def _changeSubjectName(self, oldName, newName, dagPath):
        if oldName != newName:
            self.windowParent.Controller.changeSubjectName(dagPath, newName)
            self._clearUI()
            self._refreshUI()

    def _changeSubjectType(self, roleIndex, subjectType, dagPath):
        if subjectType not in UnrealLiveLinkSubjectTable.SubjectTypeMapping:
            return

        streamTypes = UnrealLiveLinkSubjectTable.SubjectTypeMapping[subjectType]
        if roleIndex < len(streamTypes) and self.windowParent.Controller:
            self.windowParent.Controller.changeSubjectType(dagPath, list(streamTypes.keys())[roleIndex])
            self._clearUI()
            self._refreshUI()

    def _onLinkButtonPressed(self, rowIndex, editButtonPressed):
        if rowIndex < 0 or rowIndex >= self.rowCount():
            return

        dagPath = self.cellWidget(rowIndex, 2).text()
        if not (dagPath in self.subjectTypes):
            return

        # Link button is acting as a toggle when the link information is available
        if not editButtonPressed:
            if dagPath in self.assetsCurrentlyLinked:
                self._unLinkAsset(dagPath, rowIndex)
                return
            elif (dagPath in self.linkedAssets) and (dagPath in self.targetAssets):
                linkedAssetInfo = self.linkedAssets[dagPath]
                targetAssetInfo = self.targetAssets[dagPath]
                self._linkAsset(dagPath, rowIndex,
                                linkedAssetInfo[0] + '/' + linkedAssetInfo[1],
                                linkedAssetInfo[3],
                                targetAssetInfo[0],
                                targetAssetInfo[1],
                                linkedAssetInfo[4])
                return

        type = self.subjectTypes[dagPath]
        streamType = self.cellWidget(rowIndex, 0).currentText().strip()
        if type in self.SubjectTypeMapping and \
           streamType in self.SubjectTypeMapping[type]:
            mapping = self.SubjectTypeMapping[type][streamType]

            isRootOnly = streamType == 'Transform'
            isCharacter = type == 'Character'
            isJoint = isCharacter and (not isRootOnly)
            isJointRootOnly = isCharacter and isRootOnly
            nodeTypeIn = self.windowParent.Controller.getNodeType(dagPath, isJoint, isJointRootOnly)

            # Launch the dialog outside of this function
            QTimer.singleShot(100, lambda row=rowIndex, dag=dagPath, joint=isJoint, jointRootOnly=isJointRootOnly, nodeType=nodeTypeIn :
                                    self.showLinkDialog(row, dag, joint, jointRootOnly, nodeType, mapping))

    def showLinkDialog(self, rowIndex, dagPath, isJoint, isJointRootOnly, nodeType, mapping):
            # Create the asset selection modal dialog
            assetSelection = UnrealLiveLinkAssetSelection(not isJoint, self.windowParent.Controller)
            assetSelection.initUI()

            # Select the assets from what's displayed in the table
            linkedAssetLabel = self.cellWidget(rowIndex, 3)
            linkedAssetToolTip = linkedAssetLabel.toolTip()
            targetAssetLineLabel = self.cellWidget(rowIndex, 4)
            targetAssetToolTip = targetAssetLineLabel.toolTip()
            assetSelection.setupUI(isJoint, isJointRootOnly, dagPath in self.assetsCurrentlyLinked, nodeType, mapping['TargetAsset'], mapping['LevelAsset'], linkedAssetToolTip, targetAssetToolTip, self.linkedAssets, self.targetAssets)

            # Show the dialog
            assetSelection.showWindow()

            # Verify if assets were selected
            if assetSelection.result() == QDialog.Accepted:
                if len(assetSelection.linkedAssetPath) > 0:
                    self._linkAsset(dagPath, rowIndex,
                                    assetSelection.linkedAssetPath, assetSelection.linkedAssetClass,
                                    assetSelection.targetAssetPath, assetSelection.targetAssetName,
                                    assetSelection.linkedAssetNativeClass)
                else:
                    self._unLinkAsset(dagPath, rowIndex)

    def _linkAsset(self, dagPath, rowIndex, linkedAssetPath, linkedAssetClass, targetAssetPath, targetAssetName, linkedAssetNativeClass):
        groupWidget = self.cellWidget(rowIndex, 5)
        linkButton = groupWidget.layout().itemAt(1).widget()
        linkButton.replaceIcons(self.linkPendingIcon, self.linkPendingHoverIcon)

        # Link the asset
        self.windowParent.Controller.linkAsset(dagPath,
                                               linkedAssetPath,
                                               linkedAssetClass,
                                               targetAssetPath,
                                               targetAssetName,
                                               linkedAssetNativeClass)
        self.assetsCurrentlyLinked.add(dagPath)

        self.updateLinkInfo(rowIndex,
                            linkedAssetPath,
                            targetAssetPath,
                            targetAssetName,
                            linkedAssetClass,
                            linkedAssetNativeClass)

        editButton = groupWidget.layout().itemAt(0).widget()
        editButton.setEnabled(True)

        linkedAssetLabel = self.cellWidget(rowIndex, 3)
        linkedAssetLabel.setEnabled(True)
        targetAssetLineLabel = self.cellWidget(rowIndex, 4)
        targetAssetLineLabel.setEnabled(True)

        linkPaused = False
        if self.windowParent.isPauseAnimSeqSyncEnabled() and dagPath in self.charactersForAnimSeqStreaming:
            linkPaused = True
        self._updateLinkButton(linkButton, True, linkPaused)

    def _unLinkAsset(self, dagPath, rowIndex):
        # Unlink the asset
        self.windowParent.Controller.unlinkAsset(dagPath)

        if dagPath in self.assetsCurrentlyLinked:
            self.assetsCurrentlyLinked.remove(dagPath)

        self._clearUI()
        self._refreshUI()

        linkedAssetLabel = self.cellWidget(rowIndex, 3)
        targetAssetLineLabel = self.cellWidget(rowIndex, 4)
        linkedAssetLabel.setEnabled(False)
        targetAssetLineLabel.setEnabled(False)

        groupWidget = self.cellWidget(rowIndex, 5)
        linkButton = groupWidget.layout().itemAt(1).widget()
        self._updateLinkButton(linkButton, False, False)

    def _updateLinkButton(self, linkButton, assetsLinked, linkPaused):
        if linkButton and isinstance(linkButton, QPushButton):
            if assetsLinked and not linkPaused:
                linkButton.replaceIcons(self.linkedIcon, self.linkedHoverIcon)
            elif assetsLinked and linkPaused:
                linkButton.replaceIcons(self.linkPausedIcon, self.linkPausedHoverIcon)
            else:
                linkButton.replaceIcons(self.unlinkedIcon, self.unlinkedHoverIcon)

    def updateLinkInfo(self, dagPathOrRowIndex, linkedAssetPath, targetAssetPath, targetAssetName, linkedAssetClass, linkedAssetUnrealNativeClass):
        rowIndex = -1
        dagPath = ''
        if isinstance(dagPathOrRowIndex, int):
            rowIndex = dagPathOrRowIndex
            dagPath = self.cellWidget(dagPathOrRowIndex, 2).text()
        else:
            for row in range(self.rowCount()):
                if dagPathOrRowIndex == self.cellWidget(row, 2).text():
                    rowIndex = row
                    dagPath = dagPathOrRowIndex
                    break

        if rowIndex < 0:
            return

        linkedAssetLabel = self.cellWidget(rowIndex, 3)
        linkedAssetToolTip = linkedAssetLabel.toolTip()
        targetAssetLineLabel = self.cellWidget(rowIndex, 4)

        # Update the table row with the selected assets
        linkedAssetLabel.setText(linkedAssetPath[linkedAssetPath.rfind('/')+1:])
        linkedAssetLabel.setToolTip(linkedAssetPath)

        isLinked = dagPath in self.assetsCurrentlyLinked

        if len(targetAssetPath) > 0 and \
            len(targetAssetName) > 0:
            targetAssetLineLabel.setText(targetAssetName)
            targetAssetLineLabel.setToolTip(targetAssetPath + '/' + targetAssetName)
            self.targetAssets[dagPath] = (targetAssetPath, targetAssetName, isLinked)
        else:
            targetAssetLineLabel.setText('')
            targetAssetLineLabel.setToolTip('')

        if len(linkedAssetPath) > 0:
            nameIndex = linkedAssetPath.rfind('/')
            if nameIndex >= 0:
                self.linkedAssets[dagPath] = (linkedAssetPath[:nameIndex], linkedAssetPath[nameIndex+1:], isLinked, linkedAssetClass, linkedAssetUnrealNativeClass)
            else:
                self.linkedAssets[dagPath] = ('', linkedAssetPath, isLinked, linkedAssetClass, linkedAssetUnrealNativeClass)

        groupWidget = self.cellWidget(rowIndex, 5)
        editButton = groupWidget.layout().itemAt(0).widget()
        linkAllowed = self._isLinkedAllowed(rowIndex)
        editButton.setEnabled(linkAllowed and self.windowParent.ConnectedState and (dagPath in self.linkedAssets) and (dagPath in self.targetAssets))

    def _updatePauseAnimSyncButtonState(self):
        self.windowParent.pauseAnimSyncButton.setEnabled(len(self.charactersForAnimSeqStreaming) > 0)
