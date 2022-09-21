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

#include "MStreamedEntity.h"

#ifdef STAT
#undef STAT
#endif

// Import OpenMaya headers
THIRD_PARTY_INCLUDES_START
#include <maya/MString.h>
#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
THIRD_PARTY_INCLUDES_END

// ****************************************************************************
// CLASS DECLARATION (IMStreamedEntity)
//! \brief Interface for every Subject that will be streamed.
/*!
	Every Maya subject will need to implement this interface in order to stream
	itself. Stream manger keeps track of subjects being streamed as pointers to
	this type.
*/
class IMStreamedEntity : public MStreamedEntity
{
public:
	struct LinkAssetInfo
	{
		bool bSetupOnly = false;
		MString UnrealAssetPath;
		MString UnrealAssetClass;
		MString SavedAssetPath;
		MString SavedAssetName;
		MString UnrealNativeClass; // Base C++ class when UnrealAssetClass is a blueprint
	};

	explicit IMStreamedEntity(const MDagPath& DagPath) : MStreamedEntity(DagPath) {}
	virtual ~IMStreamedEntity() {}

	// Should the subject be displayed in UI
	virtual bool ShouldDisplayInUI() const { return false; }

	virtual const MDagPath& GetDagPath() const = 0;
	virtual MString GetNameDisplayText() const = 0;
	virtual MString GetRoleDisplayText() const = 0;
	virtual const MString& GetSubjectTypeDisplayText() const = 0;
	virtual MString GetLinkedAsset() const { return MString(""); }
	virtual MString GetTargetAsset() const { return MString(""); }
	virtual MString GetClass() const { return MString(""); }
	virtual MString GetUnrealNativeClass() const { return MString(""); }
	virtual bool ValidateSubject() const = 0;
	virtual bool RebuildSubjectData(bool ForceRelink = false) = 0;
	virtual void OnStream(double StreamTime, double CurrentTime) {}
	virtual void SetStreamType(const MString& StreamType) = 0;
	virtual int  GetStreamType() const = 0;
	virtual void LinkUnrealAsset(const LinkAssetInfo& LinkInfo) {}
	virtual void UnlinkUnrealAsset() {}
	virtual void OnTimeUnitChanged() {}
};
