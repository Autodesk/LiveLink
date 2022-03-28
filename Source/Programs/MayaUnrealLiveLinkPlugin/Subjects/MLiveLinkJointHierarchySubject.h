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
#include <vector>
#include "IMStreamedEntity.h"
#include "MStreamedEntity.h"
#include "MStreamHierarchy.h"

class MLiveLinkJointHierarchySubject : public IMStreamedEntity, public MStreamedEntity
{
public:
	enum MCharacterStreamMode : uint16_t
	{
		RootOnly,
		FullHierarchy,
	};

	MLiveLinkJointHierarchySubject(const MString& InSubjectName, const MDagPath& InRootPath,
		MCharacterStreamMode InStreamMode = MCharacterStreamMode::FullHierarchy);

	~MLiveLinkJointHierarchySubject();

	virtual bool ShouldDisplayInUI() const override;
	virtual MDagPath GetDagPath() const override;
	virtual MString GetNameDisplayText() const override;
	virtual MString GetRoleDisplayText() const override;
	virtual MString GetSubjectTypeDisplayText() const override;

	virtual bool ValidateSubject() const override;
	virtual bool RebuildSubjectData() override;

	// Iterate through skin clusters in the scene and find skin cluster attach to the skeleton we try to stream
	// Will return the MObject skinned to the skeleton through second argument "meshes"
	void GetGeometrySkinnedToSkeleton(const std::vector<MObject>& Skeleton, std::vector<MObject>& Meshes);

	// Iterate through the blend shapes in the scene to find the blend shapes associate with the Meshes given in argument.
	// If the blend shapes is related to the Meshes given in argument, it will add the name of each weight to the CurveNames.
	// Stock the blend shapes related to the character in the variable "BlendShapeObjects".
	void AddBlendShapesWeightNameToStream(const std::vector<MObject>& Meshes);

	MString GetPlugAliasName(const MPlug& Plug);

	virtual void OnStream(double StreamTime, double CurrentTime) override;
	virtual void SetStreamType(const MString& StreamTypeIn) override;
	virtual int GetStreamType() const override;

private:
	MString SubjectName;
	MDagPath RootDagPath;
	std::vector<MStreamHierarchy> JointsToStream;
	MStringArray CurveNames;
	MPlugArray DynamicPlugs;
	std::vector <MObject> BlendShapeObjects;

	static MStringArray CharacterStreamOptions;
	MCharacterStreamMode StreamMode;
};
