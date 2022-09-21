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
from Worker import *
from FilterTable import *
from BubbleWidget import *
from FilterListPanel import *
from ButtonLineEdit import *

class UnrealLiveLinkAssetSelection(QDialog):
    _windowName = 'UnrealLiveLinkAssetSelection'

    def __init__(self, allowLinkedAssetCreation, controller):
        super(UnrealLiveLinkAssetSelection, self).__init__()

        self.threadpool = QThreadPool()

        self.counter = 0
        self.assetTimer = QTimer()
        self.assetTimer.setInterval(500)
        self.assetTimer.timeout.connect(self.progressTimer)
        self.targetTimer = QTimer()
        self.targetTimer.setInterval(500)
        self.targetTimer.timeout.connect(self.progressTimer)

        self.allowLinkedAssetCreation = allowLinkedAssetCreation
        self.linkedAssetPath = ''
        self.linkedAssetClass = ''
        self.linkedAssetNativeClass = ''
        self.targetAssetPath = ''
        self.targetAssetName = ''
        self._controller = controller
        self._linkPressed = False
        self.isLinked = False
        self.isJoint = False
        self.isJointRootOnly = False
        self.requestedTargetAssetClass = ''
        self.leveAssets = False
        self.previouslyLinkedAsset = ''
        self.previouslyTargetAsset = ''
        self.requestAssetTimeout = False
        self.requestTargetTimeout = False
        self.nodeType = ''
        self.doAssetUnlinkCheck = True
        self.doTargetUnlinkCheck = True

        self.animSequencesBySkeleton = dict()

        self.setObjectName(UnrealLiveLinkAssetSelection._windowName)
        self.setWindowTitle('Unreal Link Asset Selection')
        self.setWindowModality(Qt.ApplicationModal)

    def initUI(self):
        self.setMinimumWidth(500)
        self.setMinimumHeight(500)
        if self._controller:
            x, y, width, height = self._controller.getUIGeometry(UnrealLiveLinkAssetSelection._windowName)
            if width > 0 and height > 0:
                self.setGeometry(0, 0, width, height)
            self.move(x, y)

        self.mainLayout = QVBoxLayout()
        self.mainLayout.setContentsMargins(5, 5, 5, 5)

        size16 = self._controller.getDpiScale(16)

        # Determine which sort indicators to use based on the DPI scale
        scale100 = self._controller.getDpiScale(100, False)
        validSizes = [100, 150, 200]
        sizeIndex = min(range(len(validSizes)), key=lambda i: abs(validSizes[i]-scale100))
        size100 = validSizes[sizeIndex]

        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)

        # Linked asset group
        hboxLayout = QHBoxLayout()
        label = QLabel('Unreal ' + ('actors' if self.allowLinkedAssetCreation else 'skeletons'))
        label.setStyleSheet("font-weight: bold")
        hboxLayout.addWidget(label)

        # Refresh button
        self._linkedAssetRefreshButton = HoverButton.fromIconName('refresh', '', True, self)
        self._linkedAssetRefreshButton.setToolTip('Refresh table')
        self._linkedAssetRefreshButton.setIconSize(QSize(size16, size16))
        self._linkedAssetRefreshButton.setFixedSize(QSize(size16, size16))
        self._linkedAssetRefreshButton.pressed.connect(lambda: self._refreshLinkedAssets(False))
        hboxLayout.addWidget(self._linkedAssetRefreshButton)
        hboxLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))

        self._linkedAssetLineEdit = QLineEdit()
        self._linkedAssetCreateButton = None
        self._linkedAssetClassComboBox = None
        if self.allowLinkedAssetCreation:
            self._linkedAssetCreateButton = HoverButton.fromIconName('add', '', False, self)
            self._linkedAssetCreateButton.setFixedSize(QSize(self._linkedAssetLineEdit.sizeHint().height(), self._linkedAssetLineEdit.sizeHint().height()))
            self._linkedAssetClassComboBox = QComboBox()
            self._linkedAssetClassComboBox.setFixedHeight(self._linkedAssetLineEdit.sizeHint().height() + 1)
            self._linkedAssetClassComboBox.setSizeAdjustPolicy(QComboBox.AdjustToContents)

        # Linked asset table
        headerLabels = ['Name', 'Type', 'Path']
        self._linkedAssetTable = FilterTable(0, len(headerLabels))
        self._linkedAssetTable.setColumnsToFilterForHighlight([0, 2])
        self._linkedAssetTable.setColumnsToFilter([1])
        self._linkedAssetTable.setSelectionBehavior(QAbstractItemView.SelectRows)
        self._linkedAssetTable.setSelectionMode(QAbstractItemView.SingleSelection)
        self._linkedAssetTable.setHorizontalHeaderLabels(headerLabels)
        self._linkedAssetTable.horizontalHeader().setDefaultSectionSize(150)
        self._linkedAssetTable.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)
        self._linkedAssetTable.horizontalHeader().setDefaultAlignment(Qt.AlignLeft)
        self._linkedAssetTable.horizontalHeader().setSortIndicator(0, Qt.AscendingOrder)
        self._linkedAssetTable.horizontalHeader().setSortIndicatorShown(True)
        self._linkedAssetTable.horizontalHeader().sortIndicatorChanged.connect(
            lambda logicalIndex, order: self._onSortIndicatorChanged(logicalIndex, order, self._linkedAssetTable))
        self._linkedAssetTable.horizontalHeader().setStyleSheet('''QHeaderView::up-arrow
                                                                   {
                                                                        image: url(''' + ImageUtils.iconPath + '''/sort_up_''' + str(size100) + '''.png);
                                                                   }
                                                                   QHeaderView::down-arrow
                                                                   {
                                                                        image: url(''' + ImageUtils.iconPath + '''/sort_down_''' + str(size100) + '''.png);
                                                                   }''')
        self._linkedAssetTable.verticalHeader().hide()
        self._linkedAssetTable.setWordWrap(False)
        self._linkedAssetTable.setShowGrid(False)
        self._linkedAssetTable.setStyleSheet('QTableWidget::item { padding-right : 10px; }')
        self._linkedAssetTable.itemSelectionChanged.connect(self._onLinkedAssetSelectionChanged)
        self._linkedAssetTable.horizontalHeader().setStretchLastSection(True)

        self.retrieveAssetLayout = QHBoxLayout()
        self.retrieveAssetLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))
        self.retrieveAssetLabel = QLabel('Retrieving {0} from Unreal Editor'.format('actors' if self.allowLinkedAssetCreation else 'skeletons'))
        self.retrieveAssetLayout.addWidget(self.retrieveAssetLabel)
        self.retrieveAssetProgressLabel = QLabel('   ')
        self.retrieveAssetLayout.addWidget(self.retrieveAssetProgressLabel)
        self.retrieveAssetLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))
        self._linkedAssetTable.setLayout(self.retrieveAssetLayout)

        self.searchIcon = ImageUtils.createIcon('search')
        self.clearIcon = ImageUtils.createIcon('clear')

        # Linked asset filter line edit
        hboxLayout2 = QHBoxLayout()
        hboxLayout2.setSpacing(2)
        hboxLayout2.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))
        self._linkedAssetFilterLineEdit = ButtonLineEdit(self.searchIcon, self.clearIcon)
        self._linkedAssetFilterLineEdit.setPlaceholderText('Search assets')
        self._linkedAssetFilterLineEdit.rightButton.setVisible(False)
        self._linkedAssetFilterLineEdit.setMinimumSize(self._linkedAssetFilterLineEdit.sizeHint())
        self._linkedAssetFilterLineEdit.setMinimumWidth(self._linkedAssetFilterLineEdit.minimumWidth() * 1.5)
        self._linkedAssetFilterLineEdit.setMaximumSize(self._linkedAssetFilterLineEdit.minimumSize())
        self._linkedAssetFilterLineEdit.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Preferred)
        self._linkedAssetFilterLineEdit.rightButton.pressed.connect(lambda: self._onFilterLineEditButtonPressed(self._linkedAssetFilterLineEdit))
        self._linkedAssetFilterLineEdit.textChanged.connect(lambda x: self._onFilterTextChanged(x,
                                                                                                self._linkedAssetTable,
                                                                                                self._linkedAssetFilterLineEdit))

        hboxLayout2.addWidget(self._linkedAssetFilterLineEdit)
        self._linkedAssetClassFilterButton = None
        if self.allowLinkedAssetCreation:
            bubbleWidget = BubbleWidget('Test', self._linkedAssetFilterLineEdit)
            bubbleWidget.hide()
            bubbleWidget.closing.connect(self._onLinkedAssetClassFilterClosing)
            self._linkedAssetFilterLineEdit.setLeftWidget(bubbleWidget)

            self._linkedAssetClassFilterButton = HoverButton.fromIconName('filter', '', True, self)
            self._linkedAssetClassFilterButton.setToolTip('Filter by type')
            self._linkedAssetClassFilterButton.setStyleSheet('QPushButton { border: 0px; padding: 0px; }')
            self._linkedAssetClassFilterButton.pressed.connect(self._onLinkedAssetClassFilterButtonPressed)
            hboxLayout2.addWidget(self._linkedAssetClassFilterButton)

            self._filterListPanel = FilterListPanel(self)
            self._filterListPanel.label.setText('Filter by type:')
            self._filterListPanel.itemChanged.connect(self._onLinkedAssetClassFilterItemChanged)
            self._filterListPanel.hide()
        hboxLayout.addLayout(hboxLayout2)
        layout.addLayout(hboxLayout)

        hboxLayout = QHBoxLayout()
        hboxLayout.setSpacing(3)

        # Linked asset Create button
        if self.allowLinkedAssetCreation:
            self._linkedAssetLineEdit.textEdited.connect(self._onLinkedAssetLineEditTextEdited)
            hboxLayout.addWidget(self._linkedAssetLineEdit)
            hboxLayout.addWidget(self._linkedAssetClassComboBox)
            self._linkedAssetClassComboBox.setEnabled(False)
            self._linkedAssetCreateButton.setToolTip('Create asset')
            self._linkedAssetCreateButton.setEnabled(False)
            self._linkedAssetCreateButton.pressed.connect(self._onLinkedAssetCreatePressed)
            hboxLayout.addWidget(self._linkedAssetCreateButton)
        else:
            hboxLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))

        layout.addLayout(hboxLayout)
        layout.addWidget(self._linkedAssetTable)
        self.mainLayout.addLayout(layout)

        # Separator
        separator = QFrame()
        separator.setFrameShape(QFrame.HLine)
        separator.setStyleSheet('background-color: rgb(86, 86, 86)')
        separator.setFixedHeight(2)
        self.mainLayout.addWidget(separator)

        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)

        # Target asset group
        hboxLayout = QHBoxLayout()
        label = QLabel('Target ' + ('level' if self.allowLinkedAssetCreation else 'anim') + ' sequences')
        label.setStyleSheet("font-weight: bold")
        hboxLayout.addWidget(label)

        # Refresh button
        self._targetRefreshButton = HoverButton.fromIconName('refresh', '', True, self)
        self._targetRefreshButton.setToolTip('Refresh table')
        self._targetRefreshButton.setIconSize(QSize(size16, size16))
        self._targetRefreshButton.setFixedSize(QSize(size16, size16))
        self._targetRefreshButton.pressed.connect(lambda: self._refreshTargetAssets(False))
        hboxLayout.addWidget(self._targetRefreshButton)
        hboxLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))

        # Target asset table
        headerLabels = ['Name', 'Type', 'Path']
        self._targetAssetTable = FilterTable(0, len(headerLabels))
        self._targetAssetTable.setColumnsToFilterForHighlight([0, 2])
        self._targetAssetTable.setSelectionBehavior(QAbstractItemView.SelectRows)
        self._targetAssetTable.setSelectionMode(QAbstractItemView.SingleSelection)
        self._targetAssetTable.setHorizontalHeaderLabels(headerLabels)
        self._targetAssetTable.horizontalHeader().setDefaultSectionSize(150)
        self._targetAssetTable.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)
        self._targetAssetTable.horizontalHeader().setDefaultAlignment(Qt.AlignLeft)
        self._targetAssetTable.horizontalHeader().setSortIndicator(0, Qt.AscendingOrder)
        self._targetAssetTable.horizontalHeader().setSortIndicatorShown(True)
        self._targetAssetTable.horizontalHeader().sortIndicatorChanged.connect(
            lambda logicalIndex, order: self._onSortIndicatorChanged(logicalIndex, order, self._targetAssetTable))
        self._targetAssetTable.horizontalHeader().setStyleSheet('''QHeaderView::up-arrow
                                                                   {
                                                                        image: url(''' + ImageUtils.iconPath + '''/sort_up_''' + str(size100) + '''.png);
                                                                   }
                                                                   QHeaderView::down-arrow
                                                                   {
                                                                        image: url(''' + ImageUtils.iconPath + '''/sort_down_''' + str(size100) + '''.png);
                                                                   }''')
        self._targetAssetTable.verticalHeader().hide()
        self._targetAssetTable.setWordWrap(False)
        self._targetAssetTable.setShowGrid(False)
        self._targetAssetTable.setStyleSheet('QTableWidget:item { padding-right : 10px }')
        self._targetAssetTable.itemSelectionChanged.connect(self._onAssetSelectionChanged)
        self._targetAssetTable.horizontalHeader().setStretchLastSection(True)

        self.retrieveTargetLayout = QHBoxLayout()
        self.retrieveTargetLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))
        self.retrieveTargetLabel = QLabel('Retrieving sequences from Unreal Editor')
        self.retrieveTargetLayout.addWidget(self.retrieveTargetLabel)
        self.retrieveTargetProgressLabel = QLabel('   ')
        self.retrieveTargetLayout.addWidget(self.retrieveTargetProgressLabel)
        self.retrieveTargetLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))
        self._targetAssetTable.setLayout(self.retrieveTargetLayout)

        # Target asset filter line edit
        self._targetAssetFilterLineEdit = ButtonLineEdit(self.searchIcon, self.clearIcon)
        self._targetAssetFilterLineEdit.setPlaceholderText('Search sequences')
        self._targetAssetFilterLineEdit.rightButton.setVisible(False)
        self._targetAssetFilterLineEdit.setMinimumSize(self._targetAssetFilterLineEdit.sizeHint())
        self._targetAssetFilterLineEdit.setMinimumWidth(self._targetAssetFilterLineEdit.minimumWidth() * 1.5)
        self._targetAssetFilterLineEdit.setMaximumSize(self._targetAssetFilterLineEdit.minimumSize())
        self._targetAssetFilterLineEdit.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Preferred)
        self._targetAssetFilterLineEdit.rightButton.pressed.connect(lambda: self._onFilterLineEditButtonPressed(self._targetAssetFilterLineEdit))
        self._targetAssetFilterLineEdit.textChanged.connect(lambda x: self._onFilterTextChanged(x,
                                                                                               self._targetAssetTable,
                                                                                               self._targetAssetFilterLineEdit))
        hboxLayout.addWidget(self._targetAssetFilterLineEdit)
        layout.addLayout(hboxLayout)

        # Target asset Create/Link button group
        hboxLayout = QHBoxLayout()
        hboxLayout.setSpacing(3)
        self._targetAssetLineEdit = QLineEdit()
        self._targetAssetLineEdit.textEdited.connect(self._onTargetAssetLineEditTextEdited)
        hboxLayout.addWidget(self._targetAssetLineEdit)
        self._targetCreateButton = HoverButton.fromIconName('add', '', False, self)
        self._targetCreateButton.setToolTip('Create sequence')
        self._targetCreateButton.setEnabled(False)
        self._targetCreateButton.setFixedSize(self._targetAssetLineEdit.sizeHint().height(), self._targetAssetLineEdit.sizeHint().height())
        self._targetCreateButton.pressed.connect(self._onTargetCreatePressed)
        hboxLayout.addWidget(self._targetCreateButton)
        self._linkButton = QPushButton()
        self._linkButton.setText('Link')
        self._linkButton.setEnabled(False)
        self._linkButton.pressed.connect(self._onLinkPressed)
        layout.addLayout(hboxLayout)
        layout.addWidget(self._targetAssetTable)
        hboxLayout = QHBoxLayout()
        hboxLayout.addItem(QSpacerItem(0, 0, QSizePolicy.Expanding, QSizePolicy.Preferred))
        hboxLayout.addWidget(self._linkButton)
        layout.addLayout(hboxLayout)
        self.mainLayout.addLayout(layout)

        self.setLayout(self.mainLayout)

        self.assetTimer.start()
        self.targetTimer.start()

        self._linkedAssets = None
        self._targetAssets = None

    def setupUI(self, isJoint, isJointRootOnly, isLinked, nodeType, targetAssetClass, levelAssets, linkedAsset, targetAsset, alreadyLinkedAssets, alreadyLinkedTargets):
        self.isLinked = isLinked
        self.isJoint = isJoint
        self.isJointRootOnly = isJointRootOnly
        self.requestedTargetAssetClass = targetAssetClass
        self.levelAssets = levelAssets
        self.previouslyLinkedAsset = linkedAsset if linkedAsset else ''
        self.previouslyTargetAsset = targetAsset if targetAsset else ''
        self.nodeType = nodeType
        self._linkedAssets = alreadyLinkedAssets
        self._targetAssets = alreadyLinkedTargets

    def showWindow(self):
        self.show()
        self._refreshLinkedAssets(True)
        self.exec_()

    def _getLinkedAssets(self, progressCallback):
        unrealNativeClasses, assetClasses = self._controller.getLinkedAssets(self.nodeType, self.levelAssets)

        uniqueClasses = None
        nativeAssetClasses = None
        if self._linkedAssetClassComboBox and unrealNativeClasses and len(unrealNativeClasses) > 0:
            # Get blueprint classes if there is any
            uniqueClasses, nativeAssetClasses = self._controller.getAssetsFromParentClass('Blueprint', unrealNativeClasses)
        elif not self.allowLinkedAssetCreation:
            self.animSequencesBySkeleton = self._controller.getAnimSequencesBySkeleton()

        return uniqueClasses, assetClasses, nativeAssetClasses

    def _getTargetAssets(self, progressCallback):
        return self._controller.getTargetAssets(self.requestedTargetAssetClass)

    def _onGetLinkedAssetsResult(self, result):
        # If result is invalid, inform the user that a timeout occurred
        if result is None:
            self.requestAssetTimeout = True
            return

        uniqueClasses, assetClasses, nativeAssetClasses = result

        # If result is invalid, inform the user that a timeout occurred
        if uniqueClasses and len(uniqueClasses) == 1 and 'Timeout!' in uniqueClasses:
            self.requestAssetTimeout = True
            return

        if self._linkedAssetClassComboBox:
            self._linkedAssetClassComboBox.clear()

            # Get blueprint classes if there is any
            if uniqueClasses and len(uniqueClasses) > 0 and nativeAssetClasses and len(nativeAssetClasses) > 0 and len(uniqueClasses) == len(nativeAssetClasses):
                for i in range(len(uniqueClasses)):
                    unrealClass = uniqueClasses[i]
                    nativeAssetClass = nativeAssetClasses[i]
                    nameIndex = unrealClass.rfind('/')
                    if nameIndex >= 0:
                        self._linkedAssetClassComboBox.addItem(unrealClass[nameIndex+1:], (unrealClass, nativeAssetClass))
                    else:
                        self._linkedAssetClassComboBox.addItem(unrealClass, (unrealClass, nativeAssetClass))
                self._linkedAssetClassComboBox.model().sort(0)

            self._filterListPanel.clearList()
            for index in range(self._linkedAssetClassComboBox.count()):
                self._filterListPanel.addToList(self._linkedAssetClassComboBox.itemText(index))
            self._filterListPanel.adjustSize()

        self._linkedAssetTable.horizontalHeader().setStretchLastSection(False)
        self._linkedAssetTable.setSortingEnabled(False)

        isAssetExist = False
        if assetClasses:
            linkedAssets = self._linkedAssets if not self.isJoint else None
            previouslyLinkedAsset = self.previouslyLinkedAsset if not self.isJoint else None
            for assetClass in assetClasses:
                assets = assetClasses[assetClass]
                for asset in assets:
                    assetClass = assetClass[:len(assetClass)-2] if assetClass.endswith('_C') else assetClass
                    UnrealLiveLinkAssetSelection._createNewItem(asset, assetClass, self._linkedAssetTable, linkedAssets, previouslyLinkedAsset)
                    if (not isAssetExist) and previouslyLinkedAsset:
                        assetFullPath = asset if asset.rfind('/') >= 0 else ('/' + asset)
                        isAssetExist = assetFullPath == self.previouslyLinkedAsset

        if (not isAssetExist) and (not self.isJoint) and len(self.previouslyLinkedAsset) > 0 and self.previouslyLinkedAsset != '/':
            for i in self._linkedAssets:
                if (self._linkedAssets[i][0] + '/' + self._linkedAssets[i][1]) == self.previouslyLinkedAsset:
                    UnrealLiveLinkAssetSelection._createNewItem(self.previouslyLinkedAsset, self._linkedAssets[i][3], self._linkedAssetTable, self._linkedAssets, self.previouslyLinkedAsset)
                    break

        self._linkedAssetTable.resizeRowsToContents()
        self._linkedAssetTable.resizeColumnsToContents()
        self._linkedAssetTable.horizontalHeader().setStretchLastSection(True)

        self._selectAsset(self.previouslyLinkedAsset, self._linkedAssetTable)
        self._updateAssetNames(self.doAssetUnlinkCheck)
        self._validateLink()
        self._linkedAssetTable.setSortingEnabled(True)

    def _onGetTargetAssetsResult(self, targetAssetClasses, refreshTriggeredByLinkAssetRefresh):
        if targetAssetClasses is None:
            self.requestTargetTimeout = True
            return

        self._targetAssetTable.horizontalHeader().setStretchLastSection(False)
        self._targetAssetTable.setSortingEnabled(False)
        isAssetExist = False
        targetAssets = self._targetAssets if self.isJoint else None
        previouslyTargetAsset = self.previouslyTargetAsset if self.isJoint else None
        if targetAssetClasses:
            for assetClass in targetAssetClasses:
                assets = targetAssetClasses[assetClass]
                for asset in assets:
                    # Filter asset that are only in the Game folder to avoid overwriting Engine assets 
                    if asset.startswith('/Game/'):
                        UnrealLiveLinkAssetSelection._createNewItem(asset, self.requestedTargetAssetClass, self._targetAssetTable, targetAssets, previouslyTargetAsset)
                        if (not isAssetExist) and self.previouslyTargetAsset:
                            nameIndex = asset.rfind('.')
                            assetFullPath = str(asset) if nameIndex < 0 else asset[:nameIndex]
                            assetFullPath = assetFullPath if assetFullPath.rfind('/') >= 0 else ('/' + assetFullPath)
                            isAssetExist = assetFullPath == self.previouslyTargetAsset

        # Create the missing sequence
        missingSequenceCreated = False
        if (not isAssetExist) and len(self.previouslyTargetAsset) > 0 and self.previouslyTargetAsset != '/':
            for i in self._targetAssets:
                if (self._targetAssets[i][0] + '/' + self._targetAssets[i][1]) == self.previouslyTargetAsset:
                    UnrealLiveLinkAssetSelection._createNewItem(self.previouslyTargetAsset, self.requestedTargetAssetClass, self._targetAssetTable, targetAssets, previouslyTargetAsset)
                    missingSequenceCreated = True
                    break

        self._targetAssetTable.resizeRowsToContents()
        self._targetAssetTable.resizeColumnsToContents()
        self._targetAssetTable.horizontalHeader().setStretchLastSection(True)

        self._selectAsset(self.previouslyTargetAsset, self._targetAssetTable)
        self._updateAssetNames(self.doTargetUnlinkCheck)
        self._validateLink()
        self._targetAssetTable.setSortingEnabled(True)

        # Create the missing AnimSequence for the selected skeleton
        if not self.allowLinkedAssetCreation:
            # Retrieve the AnimSequences when a refresh is not triggered by the linked asset refresh
            if not refreshTriggeredByLinkAssetRefresh:
                self.animSequencesBySkeleton = self._controller.getAnimSequencesBySkeleton()

            # Update the AnimSequence dictionary to add the missing sequence
            if missingSequenceCreated and (self.animSequencesBySkeleton is not None) and \
               len(self.previouslyLinkedAsset) > 0:
                if not (self.previouslyLinkedAsset in self.animSequencesBySkeleton):
                     self.animSequencesBySkeleton[self.previouslyLinkedAsset] = []
                animSequences = self.animSequencesBySkeleton[self.previouslyLinkedAsset]

                if not (self.previouslyTargetAsset in animSequences):
                    animSequences.append(self.previouslyTargetAsset)

    def _onGetLinkedAssetsComplete(self, shouldRefreshTargetAssets):
        if self.requestAssetTimeout:
            self.retrieveAssetLabel.setText('Request timeout. Please press the Refresh button to retry.')
            self.retrieveAssetLabel.show()
            self.retrieveAssetProgressLabel.setText('   ')
            self.retrieveAssetProgressLabel.hide()
            self._linkedAssetTable.setRowCount(0)
            self._updateAssetNames(True)
            self._validateLink()
        else:
            self.retrieveAssetLabel.hide()
            self.retrieveAssetProgressLabel.hide()

        if self.allowLinkedAssetCreation:
            self._onLinkedAssetLineEditTextEdited(self._linkedAssetLineEdit.text())

        # Start refreshing the target assets
        # We can't do both in parallel because it's not thread-safe with the Live Link message system
        if shouldRefreshTargetAssets:
            self._refreshTargetAssets(True)
        elif (not self.retrieveTargetLabel.isVisible()) or (self.retrieveTargetLabel.isVisible() and self.requestTargetTimeout):
            self._linkedAssetRefreshButton.setEnabled(True)
            self._targetRefreshButton.setEnabled(True)
            if self._linkedAssetClassFilterButton:
                self._linkedAssetClassFilterButton.setEnabled(True)
            else:
                # Reset the target asset filter in case additional skeletons/AnimSequences were created since the last refresh
                self._refreshTargetAssetTableWithSelectedLinkedAsset()

    def _onGetTargetAssetsComplete(self):
        if self.requestTargetTimeout:
            self.retrieveTargetLabel.setText('Request timeout. Please press the Refresh button to retry.')
            self.retrieveTargetLabel.show()
            self.retrieveTargetProgressLabel.setText('   ')
            self.retrieveTargetProgressLabel.hide()
            self._targetAssetTable.setRowCount(0)
            self._updateAssetNames(True)
            self._validateLink()
        else:
            self.retrieveTargetLabel.hide()
            self.retrieveTargetProgressLabel.hide()

        if (not self.retrieveAssetLabel.isVisible()) or (self.retrieveAssetLabel.isVisible() and self.requestAssetTimeout):
            self._linkedAssetRefreshButton.setEnabled(True)
            self._targetRefreshButton.setEnabled(True)
            if self._linkedAssetClassFilterButton:
                self._linkedAssetClassFilterButton.setEnabled(True)
            else:
                # Reset the target asset filter in case additional skeletons/AnimSequences were created since the last refresh
                self._refreshTargetAssetTableWithSelectedLinkedAsset()

        self._targetAssetFilterLineEdit.clear()
        self._onTargetAssetLineEditTextEdited(self._targetAssetLineEdit.text())

    @staticmethod
    def _splitPath(assetFullPath):
        if assetFullPath and len(assetFullPath) > 0:
            pathEndIndex = assetFullPath.rfind('/')
            if pathEndIndex >=0:
                return assetFullPath[:pathEndIndex], assetFullPath[pathEndIndex+1:]
        return '', ''

    def _selectAsset(self, assetFullPath, table):
        assetPath, assetName = UnrealLiveLinkAssetSelection._splitPath(assetFullPath)
        if len(assetName) > 0:
            rowCount = table.rowCount()
            for i in range(rowCount):
                if table.item(i, 0).text() == assetName and \
                   table.item(i, 2).text() == assetPath:
                    table.selectRow(i)
                    table.scrollToItem(table.item(i, 0), QAbstractItemView.PositionAtTop)
                    return True
        return False

    def _onSortIndicatorChanged(self, logicalIndex, order, table):
        if table:
            table.sortByColumn(logicalIndex, order)

    def closeEvent(self, event):
        self.assetTimer.stop()
        self.assetTimer = None
        self.targetTimer.stop()
        self.targetTimer = None

        self.saveUISettings()

        if self._linkPressed:
            self.setResult(1)
            event.accept()
        else:
            self.setResult(0)
            event.ignore()
            super(UnrealLiveLinkAssetSelection, self).closeEvent(event)

    def saveUISettings(self):
        if self._controller:
            self._controller.saveUIGeometry(UnrealLiveLinkAssetSelection._windowName,
                                            self.pos().x(),
                                            self.pos().y(),
                                            self.width(),
                                            self.height())

    def _onLinkedAssetSelectionChanged(self):
        items = self._linkedAssetTable.selectedItems()
        if len(items) >= 3:
            assetPath = ""
            if len(items[2].text()) > 0:
                assetPath = items[2].text() + '/'
            assetPath += items[0].text()

            if self._linkedAssetCreateButton:
                self._linkedAssetClassComboBox.setEnabled(False)
                self._linkedAssetCreateButton.setEnabled(False)

                className = items[1].text()
                for index in range(self._linkedAssetClassComboBox.count()):
                    itemComboBoxName = self._linkedAssetClassComboBox.itemText(index)
                    if className == itemComboBoxName:
                        self._linkedAssetClassComboBox.setCurrentIndex(index)
                        break
            else:
                self._targetAssetTable.clearSelection()
                self._targetAssetTable.updateFilterList(self.animSequencesBySkeleton[assetPath] if assetPath in self.animSequencesBySkeleton else [])
                self._onFilterTextChanged(self._targetAssetFilterLineEdit.text(), self._targetAssetTable, self._targetAssetFilterLineEdit)
                self._resizeColumms(self._targetAssetTable)

            self._linkedAssetLineEdit.setText(assetPath)
        self._validateLink()

    def _onAssetSelectionChanged(self):
        if self._targetCreateButton:
            self._targetCreateButton.setEnabled(False)

        items = self._targetAssetTable.selectedItems()
        if len(items) >= 3:
            assetPath = items[2].text() + '/' + items[0].text()
            self._targetAssetLineEdit.setText(assetPath)
        self._validateLink()

    def _validateLink(self):
        selectedLinkAssets = self._linkedAssetTable.selectedItems()
        selectedTargetAssets = self._targetAssetTable.selectedItems()
        isValid = len(selectedLinkAssets) >= 3 and (not self._linkedAssetTable.isRowHidden(selectedLinkAssets[0].row())) and \
                  len(selectedTargetAssets) >= 3 and (not self._targetAssetTable.isRowHidden(selectedTargetAssets[0].row()))

        if isValid:
            self._linkButton.setEnabled(True)
            unlinkState = False
            if self.allowLinkedAssetCreation:
                classIndex = self._linkedAssetClassComboBox.currentIndex()

            if self.linkedAssetPath == (selectedLinkAssets[2].text() + '/' + selectedLinkAssets[0].text()) and \
               self.targetAssetName == selectedTargetAssets[0].text() and \
               self.targetAssetPath == selectedTargetAssets[2].text():
                if self.allowLinkedAssetCreation:
                    classIndex = self._linkedAssetClassComboBox.currentIndex()
                    if self.linkedAssetClass == self._linkedAssetClassComboBox.itemData(classIndex)[0]:
                        unlinkState = True
                else:
                    unlinkState = True

            self._linkButton.setText('Unlink' if unlinkState and self.isLinked else 'Link')
        else:
            # If an asset or sequence is missing, make sure we can still unlink
            if self.isLinked and len(self.previouslyLinkedAsset) > 0 and len(self.previouslyTargetAsset) > 0 and (len(selectedLinkAssets) == 0 or len(selectedTargetAssets) == 0):
                self._linkButton.setText('Unlink')
                self._linkButton.setEnabled(True)
            else:
                self._linkButton.setEnabled(False)

    @staticmethod
    def _createNewItem(path, assetClass, table, linkedAssets = None, previouslyLinkedAsset = None):
        rowIndex = table.rowCount()
        table.insertRow(rowIndex)

        assetNameIndex = path.rfind('.')
        assetPath = ""
        item = None
        if assetNameIndex >= 0:
            item = QTableWidgetItem(path[assetNameIndex + 1:])
            assetPathIndex = path.rfind('/')
            if assetPathIndex >= 0:
                assetPath = path[:assetPathIndex]
            else:
                assetPath = path[:assetNameIndex]
        else:
            actorPathIndex = path.rfind('/')
            if actorPathIndex >= 0:
                item = QTableWidgetItem(path[actorPathIndex+1:])
                assetPath = path[:actorPathIndex]
            else:
                item = QTableWidgetItem(path)
                assetPath = ""

        assetClassShortName = str(assetClass)
        assetClassIndex = assetClassShortName.rfind('.')
        if assetClassIndex >= 0:
            assetClassShortName = assetClassShortName[assetClassIndex + 1:]

        disableFlags = Qt.ItemIsEditable

        # Make sure to not be able to select an already linked asset
        # It's only the cases for actors, since you can use the same skeleton to update multiple AnimSequences
        toolTipText = ''
        if linkedAssets and (previouslyLinkedAsset is not None):
            for key in linkedAssets:
                if linkedAssets[key][2] and assetPath == linkedAssets[key][0] and item.text() == linkedAssets[key][1] and \
                   (linkedAssets[key][0] + '/' + linkedAssets[key][1]) != previouslyLinkedAsset:
                    disableFlags = disableFlags | Qt.ItemIsEnabled
                    toolTipText = 'Already linked by ' + key
                    break

        toolTipTextLen = len(toolTipText)
        disableFlags = ~disableFlags
        item.setFlags(item.flags() & disableFlags)
        if toolTipTextLen > 0:
            item.setToolTip(toolTipText)
        table.setItem(rowIndex, 0, item)
        item = QTableWidgetItem(assetClassShortName)
        item.setFlags(item.flags() & disableFlags)
        if toolTipTextLen > 0:
            item.setToolTip(toolTipText)
        table.setItem(rowIndex, 1, item)
        item = QTableWidgetItem(assetPath)
        item.setFlags(item.flags() & disableFlags)
        if toolTipTextLen > 0:
            item.setToolTip(toolTipText)
        table.setItem(rowIndex, 2, item)
        return item

    def _onLinkedAssetCreatePressed(self):
        className = self._linkedAssetClassComboBox.currentText()
        assetName = self._validatePath(self._linkedAssetLineEdit.text())
        if len(assetName) == 0:
            return

        self._linkedAssetTable.setSortingEnabled(False)
        item = UnrealLiveLinkAssetSelection._createNewItem(assetName, className, self._linkedAssetTable)
        self._linkedAssetTable.resizeRowsToContents()
        self._linkedAssetTable.resizeColumnToContents(0)
        if item:
            self._linkedAssetTable.selectRow(item.row())
        self._linkedAssetCreateButton.setEnabled(False)
        self._linkedAssetClassComboBox.setEnabled(False)
        self._linkedAssetTable.setSortingEnabled(True)

    def _onTargetCreatePressed(self):
        assetClass = ''
        selectedItems = self._targetAssetTable.selectedItems()
        if (self._targetAssetTable.rowCount() == 0 or len(selectedItems) == 0) and len(self.requestedTargetAssetClass) > 0:
            assetClass = self.requestedTargetAssetClass
        elif len(selectedItems) == 3:
            assetClass = selectedItems[1].text()

        if len(assetClass) > 0:
            path = self._validatePath(self._targetAssetLineEdit.text())
            if len(path) == 0:
                return

            # Append the "/Game/" prefix if the user didn't type it
            if not path.startswith('/Game/'):
                if path[0] == '/':
                    path = '/Game' + path
                else:
                    path = '/Game/' + path
            assetNameIndex = path.rfind('/')
            if assetNameIndex >= 0:
                self._targetAssetTable.setSortingEnabled(False)
                item = UnrealLiveLinkAssetSelection._createNewItem(path + '.' + path[assetNameIndex+1:], assetClass, self._targetAssetTable)
                self._targetAssetTable.resizeRowsToContents()
                self._targetAssetTable.resizeColumnToContents(0)
                if item:
                    self._targetAssetTable.selectRow(item.row())
                self._targetCreateButton.setEnabled(False)
                self._targetAssetTable.setSortingEnabled(True)

    def _onLinkPressed(self):
        self.saveUISettings()

        if self._updateAssetNames(True):
            self._linkPressed = True

        self.close()

    def _updateAssetNames(self, doUnlinkCheck):
        if doUnlinkCheck and self._linkButton.text() == 'Unlink':
            self.linkedAssetClass = ''
            self.linkedAssetPath = ''
            self.targetAssetName = ''
            self.targetAssetPath = ''
            return True

        selectedLinkAssets = self._linkedAssetTable.selectedItems()
        selectedTargetAssets = self._targetAssetTable.selectedItems()
        if len(selectedLinkAssets) >= 3:
            self.linkedAssetPath = selectedLinkAssets[2].text() + '/' + selectedLinkAssets[0].text()
            if self.allowLinkedAssetCreation:
                classIndex = self._linkedAssetClassComboBox.currentIndex()
                self.linkedAssetClass, self.linkedAssetNativeClass = self._linkedAssetClassComboBox.itemData(classIndex)

            if len(selectedTargetAssets) >= 3:
                self.targetAssetName = selectedTargetAssets[0].text()
                self.targetAssetPath = selectedTargetAssets[2].text()
            return True
        return False

    def _onLinkedAssetLineEditTextEdited(self, text):
        enableButton = self._linkedAssetClassComboBox.count() > 0 and len(text) > 0
        if enableButton:
            rowCount = self._linkedAssetTable.rowCount()
            for row in range(rowCount):
                name = self._linkedAssetTable.item(row, 0).text()
                fullname = self._linkedAssetTable.item(row, 2).text()
                if len(fullname) > 0:
                    fullname += '/' + name
                else:
                    fullname = name

                if fullname == text:
                    enableButton = False
                    break
        self._linkedAssetClassComboBox.setEnabled(enableButton)
        self._linkedAssetCreateButton.setEnabled(enableButton)

    def _onTargetAssetLineEditTextEdited(self, text):
        enableButton = len(text) > 0
        if enableButton:
            if text.startswith('/Engine/'):
                enableButton = False
            else:
                if not text.startswith('/Game/'):
                    text = self._validatePath('/Game/' + text)
                rowCount = self._targetAssetTable.rowCount()
                for row in range(rowCount):
                    name = self._targetAssetTable.item(row, 0).text()
                    path = self._targetAssetTable.item(row, 2).text()
                    fullname = path + '/' + name
                    if fullname == text:
                        enableButton = False
                        break
        self._targetCreateButton.setEnabled(enableButton)

    def _refreshLinkedAssets(self, shouldRefreshTargetAssets):
        self.requestAssetTimeout = False
        self._linkedAssetRefreshButton.setEnabled(False)
        self._targetRefreshButton.setEnabled(False)
        if self._linkedAssetClassFilterButton:
            self._linkedAssetClassFilterButton.setEnabled(False)
        if self.allowLinkedAssetCreation and self._linkedAssetClassComboBox:
            self._linkedAssetClassComboBox.clear()
        self.retrieveAssetLabel.setText('Retrieving {0} from Unreal Editor'.format('actors' if self.allowLinkedAssetCreation else 'skeletons'))
        self.retrieveAssetLabel.show()
        self.doAssetUnlinkCheck = self._targetAssetTable.rowCount() == 0
        self._linkedAssetTable.setRowCount(0)
        self.retrieveAssetProgressLabel.setText('   ')
        self.retrieveAssetProgressLabel.show()
        if not self.assetTimer.isActive():
            self.assetTimer.start()

        # Start a thread to query the linked assets without blocking the UI
        worker = Worker(self._getLinkedAssets)
        worker.signals.result.connect(self._onGetLinkedAssetsResult)
        worker.signals.finished.connect(lambda: self._onGetLinkedAssetsComplete(shouldRefreshTargetAssets))
        self.threadpool.start(worker)

    def _refreshTargetAssets(self, refreshTriggeredByLinkAssetRefresh):
        self.requestTargetTimeout = False
        self._linkedAssetRefreshButton.setEnabled(False)
        self._targetRefreshButton.setEnabled(False)
        if self._linkedAssetClassFilterButton:
            self._linkedAssetClassFilterButton.setEnabled(False)
        self.retrieveTargetLabel.setText('Retrieving sequences from Unreal Editor')
        self.retrieveTargetLabel.show()
        self.doTargetUnlinkCheck = self._targetAssetTable.rowCount() == 0
        self._targetAssetTable.setRowCount(0)
        self.retrieveTargetProgressLabel.setText('   ')
        self.retrieveTargetProgressLabel.show()
        if not self.targetTimer.isActive():
            self.targetTimer.start()

        # Start a thread to query the target assets without blocking the UI
        worker = Worker(self._getTargetAssets)
        worker.signals.result.connect(lambda x: self._onGetTargetAssetsResult(x, refreshTriggeredByLinkAssetRefresh))
        worker.signals.finished.connect(self._onGetTargetAssetsComplete)
        self.threadpool.start(worker)

    def progressTimer(self):
        # Create a progress string that shows feedback to the user
        self.counter = (self.counter + 1) % 4
        progressText = (('.') * self.counter) + ((' ') * (3 - self.counter))

        if self.retrieveAssetProgressLabel.isVisible():
            self.retrieveAssetProgressLabel.setText(progressText)
        else:
            self.assetTimer.stop()

        if self.retrieveTargetProgressLabel.isVisible():
            self.retrieveTargetProgressLabel.setText(progressText)
        else:
            self.targetTimer.stop()

    def _validatePath(self, pathIn):
        path = os.path.normpath(pathIn)
        path = path.replace('\\', '/')
        pathLen = len(path)
        if path.endswith('/'):
            if pathLen == 1:
                return ''

            path = path[:len(path)-1]
        return path

    def _onFilterTextChanged(self, text, table, lineEdit):
        if table:
            itemList = []
            if self.allowLinkedAssetCreation and self._filterListPanel and table == self._linkedAssetTable:
                itemList = self._filterListPanel.getCheckedItems()

            table.searchFilter(text, itemList)
            table.viewport().update()
            self._validateLink()

        if lineEdit and lineEdit.rightButton:
            lineEdit.rightButton.setVisible(len(text) > 0)

        # Restore default column size when clearing the text
        if len(text) == 0:
            self._resizeColumms(self._targetAssetTable)

    def _onFilterLineEditButtonPressed(self, lineEdit):
        if lineEdit:
            lineEdit.clear()

    def _onLinkedAssetClassFilterButtonPressed(self):
        if self._linkedAssetClassComboBox is None:
            return

        if self._filterListPanel.isVisible():
            self._filterListPanel.hide()
            return

        self._filterListPanel.move(self._linkedAssetClassFilterButton.pos() - QPoint(self._filterListPanel.size().width() - self._linkedAssetClassFilterButton.size().width(), -self._linkedAssetClassFilterButton.size().height() - 2))
        self._filterListPanel.show()
        self._filterListPanel.raise_()
        self._filterListPanel.setFocus()

    def _onLinkedAssetClassFilterItemChanged(self, item):
        if item is None:
            return

        if item.checkState() == Qt.Checked:
            itemListLen = self._filterListPanel.checkedCount()
            self._linkedAssetFilterLineEdit.leftWidget().setText(UnrealLiveLinkAssetSelection._formatFilterText(itemListLen))
            self._linkedAssetFilterLineEdit._updateWidgetPositions()

            # Need to update the table since the filter changed
            self._linkedAssetTable.searchFilter(self._linkedAssetFilterLineEdit.text(),
                                                self._filterListPanel.getCheckedItems())
            self._validateLink()

            if itemListLen > 0 and (not self._linkedAssetFilterLineEdit.leftWidget().isVisible()):
                self._linkedAssetFilterLineEdit.leftWidget().show()

        elif item.checkState() == Qt.Unchecked:
            itemListLen = self._filterListPanel.checkedCount()
            self._linkedAssetFilterLineEdit.leftWidget().setText(UnrealLiveLinkAssetSelection._formatFilterText(itemListLen))
            self._linkedAssetFilterLineEdit._updateWidgetPositions()

            # Need to update the table since the filter changed
            self._linkedAssetTable.searchFilter(self._linkedAssetFilterLineEdit.text(),
                                                self._filterListPanel.getCheckedItems())
            self._validateLink()

            if itemListLen == 0 and self._linkedAssetFilterLineEdit.leftWidget().isVisible():
                self._linkedAssetFilterLineEdit.leftWidget().hide()

        self._resizeColumms(self._linkedAssetTable)

        self._filterListPanel.raise_()

    def _onLinkedAssetClassFilterClosing(self):
        self._filterListPanel._checkAllItems(Qt.Unchecked)

    def _resizeColumms(self, table):
        table.horizontalHeader().setStretchLastSection(False)
        table.resizeRowsToContents()
        table.resizeColumnsToContents()
        table.horizontalHeader().setStretchLastSection(True)

    @staticmethod
    def _formatFilterText(count):
        return (str(count) if count < 10 else '9+') + ' Filter' + ('s' if count > 1 else '')

    def _refreshTargetAssetTableWithSelectedLinkedAsset(self):
        assetPath = ''
        items = self._linkedAssetTable.selectedItems()
        if len(items) >= 3:
            assetPath = ""
            if len(items[2].text()) > 0:
                assetPath = items[2].text() + '/'
            assetPath += items[0].text()

            self._targetAssetTable.updateFilterList(self.animSequencesBySkeleton[assetPath] if assetPath in self.animSequencesBySkeleton else [])
        else:
            self._targetAssetTable.updateFilterList([])
            self._targetAssetTable.clearSelection()

        self._onFilterTextChanged(self._targetAssetFilterLineEdit.text(), self._targetAssetTable, self._targetAssetFilterLineEdit)

        self._linkedAssetFilterLineEdit.clear()
