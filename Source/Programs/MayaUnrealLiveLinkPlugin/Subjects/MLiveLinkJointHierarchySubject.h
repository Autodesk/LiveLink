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
#include "MStreamHierarchy.h"

#include <vector>

THIRD_PARTY_INCLUDES_START
#include <maya/MPlugArray.h>
THIRD_PARTY_INCLUDES_END

class MLiveLinkJointHierarchySubject : public IMStreamedEntity
{
public:
	enum MCharacterStreamMode : uint16_t
	{
		RootOnly,
		FullHierarchy,
	};

	MLiveLinkJointHierarchySubject(const MString& InSubjectName,
								   const MDagPath& InRootPath,
								   MCharacterStreamMode InStreamMode = MCharacterStreamMode::FullHierarchy);

	virtual ~MLiveLinkJointHierarchySubject();

	virtual bool ShouldDisplayInUI() const override;
	virtual const MDagPath& GetDagPath() const override;
	virtual MString GetNameDisplayText() const override;
	virtual MString GetRoleDisplayText() const override;
	virtual MStreamedEntity::Role GetRole() const override { return MStreamedEntity::Character; }
	virtual const MString& GetSubjectTypeDisplayText() const override;

	virtual bool ValidateSubject() const override;
	virtual bool RebuildSubjectData(bool ForceRelink = false) override;

	// Iterate through skin clusters in the scene and find skin cluster attach to the skeleton we try to stream
	// Will return the MObject skinned to the skeleton through second argument "meshes"
	void GetGeometrySkinnedToSkeleton(const std::vector<MObject>& Skeleton, std::vector<MObject>& Meshes);

	// Iterate through the blend shapes in the scene to find the blend shapes associate with the Meshes given in argument.
	// If the blend shapes is related to the Meshes given in argument, it will add the name of each weight to the CurveNames.
	// Stock the blend shapes related to the character in the variable "BlendShapeObjects".
	void AddBlendShapesWeightNameToStream(const std::vector<MObject>& Meshes);

	virtual void OnStream(double StreamTime, double CurrentTime) override;
	virtual void SetStreamType(const MString& StreamTypeIn) override;
	void SetStreamType(MCharacterStreamMode StreamModeIn);
	virtual int GetStreamType() const override;

	virtual void OnAttributeChanged(const MObject& Object, const MPlug& Plug, const MPlug& OtherPlug) override;
	virtual void OnAnimCurveEdited(const MString& AnimCurveNameIn, MObject& AnimCurveObject, const MPlug& Plug, double ConversionFactor = 1.0) override;
	virtual void OnAnimKeyframeEdited(const MString& AnimCurveName, MObject& AnimCurveObject, const MPlug& Plug) override;
	virtual void OnTimeUnitChanged() override;

	virtual MString GetLinkedAsset() const override { return UnrealAssetPath; }
	virtual bool IsLinked() const override;
	virtual MString GetTargetAsset() const override { return SavedAssetPath + MString("/") + SavedAssetName; }
	virtual MString GetClass() const override { return MString("Skeleton"); }
	virtual MString GetUnrealNativeClass() const override { return MString("Skeleton"); }

	virtual void LinkUnrealAsset(const LinkAssetInfo& LinkInfo) override;
	virtual void UnlinkUnrealAsset() override;
	virtual void SetBakeUnrealAsset(bool shouldBakeCurves) override;

	virtual bool ShouldBakeTransform() const override;

private:
	template<typename T>
	bool BuildStaticData(T& AnimationData, std::vector<MObject>& SkeletonObjects);
	template<typename T, typename F>
	void BuildFrameData(T& AnimationData,
						F& AddLambda,
						std::vector<MMatrix>& InverseScales,
						int FrameIndex);
	template<typename T, typename F>
	void BuildBlendShapeWeights(T& AnimationData, F& AddLambda, int FrameIndex);
	template<typename T, typename F>
	void BuildDynamicPlugValues(T& AnimationData, F& AddLambda, int FrameIndex);


private:
	MString SubjectName;
	std::vector<MStreamHierarchy> JointsToStream;
	MStringArray CurveNames;
	std::vector<MObject> BlendShapeObjects;

	static MStringArray CharacterStreamOptions;
	MCharacterStreamMode StreamMode;

	bool bLinked;
	MString UnrealAssetPath;
	MString SavedAssetPath;
	MString SavedAssetName;

	bool StreamFullAnimSequence;
	bool ForceLinkAsset;
	bool ShouldBakeCurves = false;
};
