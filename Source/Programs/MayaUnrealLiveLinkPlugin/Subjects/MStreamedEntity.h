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

THIRD_PARTY_INCLUDES_START
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MCallbackIdArray.h>
#include <maya/MDGModifier.h>
#include <maya/MNodeMessage.h>
THIRD_PARTY_INCLUDES_END

// Maya specific class
// 
// Base structure for every type of entity
// It auto-registers/unregisters the necessary Maya callbacks to react on some event like node deletion/renaming
class MStreamedEntity
{
public:
	explicit MStreamedEntity(const MDagPath& DagPath);
	virtual ~MStreamedEntity()
	{
		UnregisterNodeCallbacks();
	}

private:
	void RegisterNodeCallbacks(const MDagPath& DagPath, bool IsRoot = true);
	void UnregisterNodeCallbacks();

	// Callback functions
	static void AboutToDeleteCallback(MObject& node, MDGModifier& modifier, void* clientData);
	static void NameChangedCallback(MObject &node, const MString &str, void *clientData);
	static void AttributeChangedCallback(MNodeMessage::AttributeMessage Msg, MPlug& Plug, MPlug& OtherPlug, void* ClientData);

	void ProcessBlendShapes(const MObject& SubjectObject);
	void ProcessHumanIKEffectors(const MObject& Node);

private:
	MCallbackIdArray CallbackIds;
	MDagPath RootDagPath;
	bool HIKEffectorsProcessed;
};
