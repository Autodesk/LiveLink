#!/bin/bash

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

scriptDir="$(dirname "$(readlink -f "$0")")"

# arg[1] specifies the Maya version (2022|2023|2024 ...), see BuildMayaUnrealLiveLinkPlugin.xml for default value
if [ ! -z "$1" ]; then
    MayaVersion=-set:MayaVersion=$1
fi

# arg[2] specifies the build target (Debug|Development|Shipping), see BuildMayaUnrealLiveLinkPlugin.xml for default value
if [ ! -z "$2" ]; then
    BuildTarget=-set:BuildTarget=$2
fi

if [ -d "$scriptDir/Staging" ]; then
    rm -rf "$scriptDir/Staging"
fi

"${scriptDir}/../../../../../Build/BatchFiles/RunUAT.sh" BuildGraph -Script=Engine/Restricted/NotForLicensees/Source/Programs/MayaUnrealLiveLinkPlugin/BuildMayaUnrealLiveLinkPlugin.xml -Target="Stage Maya Plugin Module" ${MayaVersion} -set:MayaPlatform=Linux ${BuildTarget} -NoXGE
