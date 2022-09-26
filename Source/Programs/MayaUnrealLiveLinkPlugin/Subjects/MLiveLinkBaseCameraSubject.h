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

#include "IMStreamedEntity.h"

class MLiveLinkBaseCameraSubject : public IMStreamedEntity
{
public:
	enum MCameraStreamMode : uint16_t
	{
		RootOnly,
		FullHierarchy,
		Camera,
	};

	MLiveLinkBaseCameraSubject(const MString& InSubjectName, 
							   MCameraStreamMode InStreamMode = MCameraStreamMode::Camera,
							   const MDagPath& InRootPath = MDagPath());
	virtual ~MLiveLinkBaseCameraSubject();

	virtual MString GetNameDisplayText() const override;
	virtual MString GetRoleDisplayText() const override;
	virtual MStreamedEntity::Role GetRole() const override { return MStreamedEntity::Camera; }
	virtual const MString& GetSubjectTypeDisplayText() const override;

	virtual bool ValidateSubject() const override;

	virtual bool RebuildSubjectData(bool ForceRelink = false) override;

	void StreamCamera(const MDagPath& CameraPath, double StreamTime, double CurrentTime);

	virtual void SetStreamType(const MString& StreamTypeIn) override;
	virtual void SetStreamType(MCameraStreamMode StreamModeIn);
	virtual int GetStreamType() const override;

	virtual void OnAttributeChanged(const MObject& Object, const MPlug& Plug, const MPlug& OtherPlug) override;

	virtual const MVector& GetLevelSequenceRotationOffset() const override
	{
		static const MVector RotationOffset(-90.0f, 0.0f, -90.0f);
		return RotationOffset;
	}

	virtual MString GetLinkedAsset() const override { return UnrealAssetPath; }
	virtual MString GetTargetAsset() const override { return SavedAssetPath + MString("/") + SavedAssetName; }
	virtual MString GetClass() const override { return UnrealAssetClass; }
	virtual MString GetUnrealNativeClass() const override { return UnrealNativeClass; }

protected:
	void InitializeStaticData(struct FLiveLinkCameraStaticData& StaticData);

protected:
	MString  SubjectName;

	static MStringArray CameraStreamOptions;
	MCameraStreamMode StreamMode;

	MString UnrealAssetPath;
	MString UnrealAssetClass;
	MString SavedAssetPath;
	MString SavedAssetName;
	MString UnrealNativeClass;
};