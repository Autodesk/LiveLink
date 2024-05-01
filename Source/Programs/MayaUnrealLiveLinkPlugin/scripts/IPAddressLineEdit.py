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

class IPAddressLineEdit(QLineEdit):
    parent = None
    regexp = None
    if usingPyside6:
        parent = QRegularExpressionValidator
        regexp = QRegularExpression
    else:
        parent = QRegExpValidator
        regexp = QRegExp

    class IPAddressRegExpValidator(parent):
        validationChanged = Signal(QValidator.State)

        def validate(self, input, pos):
            state, input, pos = super(IPAddressLineEdit.IPAddressRegExpValidator, self).validate(input, pos)
            self.validationChanged.emit(state)
            return state, input, pos

    textModified = Signal(str, str) # (before, after)
    validationError = Signal()

    _ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])"   # Part of the regular expression
    _portRange = "([0-9][0-9]{0,3}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])"
    _ipRegex = regexp("^" + _ipRange + "\\." + _ipRange + "\\." + _ipRange + "\\." + _ipRange + ":" + _portRange + "$|0.0.0.0:0")

    def __init__(self, contents='', parent=None):
        super(IPAddressLineEdit, self).__init__(contents, parent)
        self.editingFinished.connect(self.__handleEditingFinished)
        self._before = contents
        self._validationState = self.hasAcceptableInput()
        self._skipValidation = False
        self.defaultIPAddress = ''

        # Install a validator to make the IP address is always well formed
        ipValidator = IPAddressLineEdit.IPAddressRegExpValidator(IPAddressLineEdit._ipRegex, self)
        ipValidator.validationChanged.connect(self.handleValidationChange)
        self.setValidator(ipValidator)

    def focusOutEvent(self, event):
        super(IPAddressLineEdit, self).focusOutEvent(event)
        textLength = len(self.text())

        # validate the IP address
        if textLength == 0:
            if len(self.defaultIPAddress) > 0:
                # Replace an empty line edit by the default address
                oldString = str(self.text())
                self.setText(self.defaultIPAddress)
                self.textModified.emit(oldString, self.defaultIPAddress)
        elif self.hasAcceptableInput() is False:
            # Stay on the line edit until the user finishes typing the address
            self.setFocus()
            self.validationError.emit()

    @staticmethod
    def trim(address):
        # Remove any leading 0
        after = address.replace(':', '.')
        trimmedIP = after.split('.')
        trimmedIP = [x.lstrip('0') for x in trimmedIP]
        trimmedIPSize = len(trimmedIP)
        if trimmedIPSize == 5:
            for i in range(trimmedIPSize):
                if len(trimmedIP[i]) == 0:
                       trimmedIP[i] = '0'
            after = ".".join(trimmedIP[:4]) + ':' + trimmedIP[4]
            return after
        return ""

    def __handleEditingFinished(self):
        before, after = self._before, self.text()
        if before != after:
            trimmed = IPAddressLineEdit.trim(after)
            if len(trimmed) > 0:
                after = trimmed
            self.setText(after)
            self._before = after
            self.textModified.emit(before, after)

    def handleValidationChange(self, state):
        validState = False
        if state == QValidator.Invalid:
            colour = 'red'
        elif state == QValidator.Intermediate:
            colour = 'gold'
        elif state == QValidator.Acceptable:
            colour = 'lime'
            validState = True
        if validState or len(self.text()) == 0:
            # Clear the border if valid or empty
            QTimer.singleShot(1000, lambda: self.setStyleSheet(''))

        # Add a colored border to let the user know that the IP address is valid or not
        self.setStyleSheet('border: 2px solid %s' % colour)
        if validState != self._validationState:
            self_validationState = validState
