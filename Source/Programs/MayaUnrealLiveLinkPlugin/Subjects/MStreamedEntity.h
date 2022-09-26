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

#include "Math/Transform.h"

#include <array>
#include <map>

THIRD_PARTY_INCLUDES_START
#include <maya/MFnAnimCurve.h>
#include <maya/MPlugArray.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MCallbackIdArray.h>
#include <maya/MDGModifier.h>
#include <maya/MNodeMessage.h>
#include <maya/MVector.h>
THIRD_PARTY_INCLUDES_END

// Maya specific class
// 
// Base structure for every type of entity
// It auto-registers/unregisters the necessary Maya callbacks to react on some event like node deletion/renaming
class MStreamedEntity
{
public:
	enum Role
	{
		Character,
		Camera,
		Light,
		Prop,
	};

	explicit MStreamedEntity(const MDagPath& DagPath);
	virtual ~MStreamedEntity()
	{
		UnregisterNodeCallbacks();
	}

	virtual MStreamedEntity::Role GetRole() const = 0;

	virtual bool IsLinked() const { return false; }

	virtual void OnStream(double StreamTime, double CurrentTime) = 0;
	void OnStreamCurrentTime();

	virtual void OnAnimCurveEdited(const MString& AnimCurveNameIn, MObject& AnimCurveObject, const MPlug& Plug, double ConversionFactor = 1.0);

	virtual void OnAnimKeyframeEdited(const MString& AnimCurveName, MObject& AnimCurveObject, const MPlug& Plug) {}
	void OnPreAnimCurvesEdited() { bTransformCurvesBaked = false; }

	virtual const MVector& GetLevelSequenceRotationOffset() const { return MVector::zero; }

	virtual bool IsScaleSupported() const { return false; }

	void UpdateAnimCurves(const MDagPath& DagPath);

	bool IsOwningBlendShape(const MString& Name) const;

	bool IsUsingHikIKEffector(const MObject& HikIKEffectorObject);

	virtual bool ShouldBakeTransform() const;

	void RegisterParentNode(MObject& ParentNode);

public:
	struct MKeyFrame
	{
		void Initialize()
		{
			Value = 0.0;
			UpdateTangentValue(0.0, MFnAnimCurve::kTangentAuto);
			TangentLocked = true;
		}

		void UpdateTangentValue(double TangentValue, MFnAnimCurve::TangentType Type, double Weight = 1.0)
		{
			TangentValueIn = { TangentValue, Weight };
			TangentValueOut = { TangentValue, Weight };
			TangentTypeIn = Type;
			TangentTypeOut = Type;
		}

		double Value;
		MFnAnimCurve::TangentType TangentTypeIn;
		std::array<double, 2> TangentValueIn;		// tan angle, Weight
		MFnAnimCurve::TangentType TangentTypeOut;
		std::array<double, 2> TangentValueOut;		// tan angle, Weight
		bool TangentLocked;
	};
	struct MAnimCurve
	{
		MKeyFrame& FindOrAddKeyFrame(double Time, bool InitIfNotFound = false);
		MKeyFrame* FindKeyFrame(double Time);

		void BakeKeyFrame(double Time, double Value, int32 Key, int32 NumKeys);

		std::map<double, MKeyFrame> KeyFrames;
	};

protected:
	std::map<std::string, MAnimCurve> AnimCurves;

	MDagPath RootDagPath;
	MPlugArray DynamicPlugs;

	static const std::array<double, MTime::Unit::kLast> MayaTimeUnitToFPS;

protected:
	void InitializeStaticData(struct FMayaLiveLinkLevelSequenceStaticData& StaticData,
							  const MString& SequenceName,
							  const MString& SequencePath,
							  const MString& ClassName,
							  const MString& LinkedAssetPath) const;
	void InitializeFrameData(struct FMayaLiveLinkAnimCurveData& CurveData, double StartTime = 0) const;

	void RebuildLevelSequenceSubject(const MString& SubjectName,
									 const MDagPath& DagPath,
									 const MString& SavedAssetName,
									 const MString& SavedAssetPath,
									 const MString& UnrealAssetName,
									 const MString& UnrealAssetPath,
									 bool ForceRelink);

	const MStringArray& GetBlendShapeNames() const { return BlendShapeNames; }

	void UpdateAnimCurveKeys(MObject& AnimCurveObject, MAnimCurve& AnimCurve, int LocationIndex = -1, int ScaleIndex = -1, double Conversion = 1.0);

	virtual void OnAttributeChanged(const MObject& Object, const MPlug& Plug, const MPlug& OtherPlug);

private:
	void RegisterNodeCallbacks(const MDagPath& DagPath, bool IsRoot = true);
	void UnregisterNodeCallbacks();
	void RegisterParentNodeRecursive(MObject& Node);

	// Callback functions
	static void AboutToDeleteCallback(MObject& Node, MDGModifier& Modifier, void* ClientData);
	static void NameChangedCallback(MObject& Node, const MString& Str, void* ClientData);
	static void AttributeChangedCallback(MNodeMessage::AttributeMessage Msg, MPlug& Plug, MPlug& OtherPlug, void* ClientData);

	void ProcessBlendShapes(const MObject& SubjectObject);
	void ProcessBlendShapeControllers(const MFnBlendShapeDeformer& BlendShape, MDagPathArray& DagPathArray);
	void ProcessHumanIKEffectors(const MObject& Node);
	void ProcessConstraints(const MFnDagNode& Node);
	void ProcessMotionPaths(const MFnDagNode& DagNode);

	FTransform ComputeUnrealTransform();
	FTransform ComputeUnrealTransform(MDGContext& TimeDGContext);

	void BakeTransformCurves(bool bRotationOnly);

	bool RegisterController(const MPlug& Plug, MDagPathArray& DagPathArray);

private:
	MCallbackIdArray CallbackIds;
	bool HIKEffectorsProcessed;
	MString HIKCharacterNodeName;
	bool bTransformCurvesBaked;
	MStringArray BlendShapeNames;
	bool bHasMotionPath;
	bool bHasConstraint;

	friend class MayaLiveLinkStreamManager;
};