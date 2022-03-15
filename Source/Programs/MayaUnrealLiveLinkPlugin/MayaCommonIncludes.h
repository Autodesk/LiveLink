// MIT License

// Copyright (c) 2022 Autodesk, Inc.

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

// Boilerplate for importing OpenMaya
#if defined PLATFORM_WINDOWS
#include "Windows/WindowsPlatformCompilerPreSetup.h"
#else
#include "Unix/UnixPlatformCompilerPreSetup.h"
#endif

// Maya includes
// For Maya 2016 the SDK has to be downloaded and installed manually for these includes to work.
//#define DWORD BananaFritters
THIRD_PARTY_INCLUDES_START
#include <maya/MObject.h>
#include <maya/MGlobal.h>
#if MAYA_API_VERSION < 20190000
	#ifndef MAYA_OLD_PLUGIN
		#define MAYA_OLD_PLUGIN
	#endif // MAYA_OLD_PLUGIN
#endif // MAYA_API_VERSION
#include <maya/MFnPlugin.h>
#include <maya/MPxCommand.h> //command
#include <maya/MCommandResult.h> //command
#include <maya/MPxNode.h> //node
#include <maya/MFnNumericAttribute.h>
#include <maya/MCallbackIdArray.h>
#include <maya/MEventMessage.h>
#include <maya/MDagMessage.h>
#include <maya/MItDag.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MMatrix.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MStringArray.h>
#include <maya/MString.h>
#include <maya/MVector.h>
#include <maya/MFnTransform.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnCamera.h>
#include <maya/MFnLight.h>
#include <maya/MFnSpotLight.h>
#include <maya/MEulerRotation.h>
#include <maya/MSelectionList.h>
#include <maya/MAnimControl.h>
#include <maya/MTimerMessage.h>
#include <maya/MDGMessage.h>
#include <maya/MNodeMessage.h>
#include <maya/MSceneMessage.h>
#include <maya/M3dView.h>
#include <maya/MUiMessage.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MPlug.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MDagPathArray.h>
#include <maya/MArgList.h>
THIRD_PARTY_INCLUDES_END
//#undef DWORD