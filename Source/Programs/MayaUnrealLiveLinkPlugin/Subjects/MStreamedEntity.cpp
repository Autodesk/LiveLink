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

#include "MStreamedEntity.h"
#include "../MayaLiveLinkStreamManager.h"
#include "../MayaUnrealLiveLinkUtils.h"

#include <cmath>

THIRD_PARTY_INCLUDES_START
#include <maya/MAnimUtil.h>
#include <maya/MDistance.h>
#include <maya/MDGContextGuard.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MItCurveCV.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MFnMesh.h>
THIRD_PARTY_INCLUDES_END

const std::array<double, MTime::Unit::kLast> MStreamedEntity::MayaTimeUnitToFPS =
{
	0.0,		// kInvalid
	0.0,		// kHours
	0.0,		// kMinutes
	0.0,		// kSeconds
	0.0f,		// kMilliseconds
	15.0,		// k15FPS
	24.0,		// k24FPS
	25.0,		// k25FPS
	30.0,		// k30FPS
	48.0,		// k48FPS
	50.0,		// k50FPS
	60.0,		// k60FPS
	2.0,		// k2FPS,
	3.0,		// k3FPS,
	4.0,		// k4FPS,
	5.0,		// k5FPS,
	6.0,		// k6FPS,
	8.0,		// k8FPS,
	10.0,		// k10FPS
	12.0,		// k12FPS
	16.0,		// k16FPS
	20.0,		// k20FPS
	40.0,		// k40FPS,
	75.0,		// k75FPS,
	80.0,		// k80FPS,
	100.0,		// k100FPS,
	120.0,		// k120FPS,
	125.0,		// k125FPS,
	150.0,		// k150FPS,
	200.0,		// k200FPS,
	240.0,		// k240FPS,
	250.0,		// k250FPS,
	300.0,		// k300FPS,
	375.0,		// k375FPS,
	400.0,		// k400FPS,
	500.0,		// k500FPS,
	600.0,		// k600FPS,
	750.0,		// k750FPS,
	1200.0,		// k1200FPS
	1500.0,		// k1500FPS
	2000.0,		// k2000FPS
	3000.0,		// k3000FPS
	6000.0,		// k6000FPS
	23.976,		// k23_976FPS
	29.97,		// k29_97FPS
	29.97,		// k29_97DF,
	47.952,		// k47_952FPS,
	59.94,		// k59_94FPS,
	44100.0,	// k44100FPS
	48000.0,	// k48000FPS
	90.0,		// k90FPS
	0.0,		// kUserDef, user defined units (not implemented yet)
};

static const std::array<std::string, 3> LocationNames = { "LocationX", "LocationY", "LocationZ" };
static const std::array<std::string, 3> RotationNames = { "RotationX", "RotationY", "RotationZ" };
static const std::array<std::string, 3> ScaleNames = { "ScaleX", "ScaleY", "ScaleZ" };

namespace
{
	double SAFE_TANGENT(double x)
	{
		static const double SAFE_TANGENT_THRESHOLD = 6000.0;
		return std::min(std::max(x, -SAFE_TANGENT_THRESHOLD), SAFE_TANGENT_THRESHOLD);
	}
}


MStreamedEntity::MKeyFrame& MStreamedEntity::MAnimCurve::FindOrAddKeyFrame(double Time, bool InitIfNotFound)
{
	// Find the keyframe time
	auto KeyFrameIter = KeyFrames.find(Time);
	if (KeyFrameIter == KeyFrames.end())
	{
		MKeyFrame Frame;
		if (InitIfNotFound)
		{
			Frame.Initialize();
		}
		KeyFrameIter = KeyFrames.emplace(Time, Frame).first;
	}

	return KeyFrameIter->second;
}

MStreamedEntity::MKeyFrame* MStreamedEntity::MAnimCurve::FindKeyFrame(double Time)
{
	// Find the keyframe time
	auto KeyFrameIter = KeyFrames.find(Time);
	if (KeyFrameIter == KeyFrames.end())
	{
		return nullptr;
	}

	return &KeyFrameIter->second;
}

MStreamedEntity::MStreamedEntity(const MDagPath& DagPath)
: HIKEffectorsProcessed(false)
, bTransformCurvesBaked(false)
, bHasMotionPath(false)
, bHasConstraint(false)
{
	if (DagPath.isValid())
	{
		RootDagPath = DagPath;
		RegisterNodeCallbacks(DagPath);
	}
}

void MStreamedEntity::UpdateAnimCurves(const MDagPath& DagPath)
{
	if (!IsLinked())
	{
		AnimCurves.clear();
		return;
	}

	// Wait a bit after rebuilding the subject data before sending the curve data to Unreal.
	// Otherwise, Unreal will ignore it.
	using namespace std::chrono_literals;
	std::this_thread::sleep_for(100ms);

	// Find the animated plugs from this subject
	MSelectionList list;
	list.add(DagPath);

	// Also add the shape node to get its anim curves
	auto ShapeDagPath = DagPath;
	auto Status = ShapeDagPath.extendToShape();
	if (Status == MStatus::kSuccess && !(DagPath == ShapeDagPath))
	{
		list.add(ShapeDagPath);
	}

	MPlugArray animatedPlugs;
	MAnimUtil::findAnimatedPlugs(list, animatedPlugs);

	MObjectArray ObjectArray;
	unsigned int numPlugs = animatedPlugs.length();
	for (unsigned int i = 0; i < numPlugs; i++)
	{
		MPlug plug = animatedPlugs[i];
		MObjectArray animation;

		// Find the animation curve(s) that animate this plug
		if (!MAnimUtil::findAnimation(plug, animation))
		{
			continue;
		}

		unsigned int numAnimCurves = animation.length();
		for (unsigned int Curve = 0; Curve < numAnimCurves; ++Curve)
		{
			ObjectArray.append(animation[Curve]);
		}
	}

	// Notify that we want to send these anim curves
	if (ObjectArray.length() != 0)
	{
		::OnAnimCurveEdited(ObjectArray, nullptr);
	}

	if (ShouldBakeTransform())
	{
		BakeTransformCurves(false);
		OnStreamCurrentTime();
	}
}

void MStreamedEntity::InitializeStaticData(FMayaLiveLinkLevelSequenceStaticData& StaticData,
										   const MString& SequenceName,
										   const MString& SequencePath,
										   const MString& ClassName,
										   const MString& LinkedAssetPath) const
{
	StaticData.SequenceName = SequenceName.asChar();
	StaticData.SequencePath = SequencePath.asChar();
	// The UnrealAssetPath contains the class name with its path if it's a blueprint class
	StaticData.ClassName = ClassName.asChar();
	StaticData.LinkedAssetPath = LinkedAssetPath.asChar();

	auto TimeUnit = MTime::uiUnit();
	StaticData.FrameRate = MayaUnrealLiveLinkUtils::GetMayaFrameRateAsUnrealFrameRate();
	StaticData.StartFrame = static_cast<int32>(MAnimControl::minTime().as(TimeUnit));
	StaticData.EndFrame = static_cast<int32>(MAnimControl::maxTime().as(TimeUnit));
}

void MStreamedEntity::InitializeFrameData(FMayaLiveLinkAnimCurveData& CurveData, double StartTime) const
{
	CurveData.MetaData.SceneTime = MayaUnrealLiveLinkUtils::GetMayaFrameTimeAsUnrealTime();

	for (auto& CurvePair : AnimCurves)
	{
		FMayaLiveLinkCurve AnimCurve;
		auto& KeyFrames = AnimCurve.KeyFrames;
		for (auto& KeyFramePair : CurvePair.second.KeyFrames)
		{
			FMayaLiveLinkKeyFrame KeyFrame;
			const double Time = KeyFramePair.first - StartTime;
			const MKeyFrame& MayaKeyFrame = KeyFramePair.second;
			KeyFrame.Value = MayaKeyFrame.Value;
			KeyFrame.TangentAngleIn = MayaKeyFrame.TangentValueIn[0];
			KeyFrame.TangentAngleOut = MayaKeyFrame.TangentValueOut[0];
			KeyFrame.TangentWeightIn = MayaKeyFrame.TangentValueIn[1];
			KeyFrame.TangentWeightOut = MayaKeyFrame.TangentValueOut[1];

			// InterpMode
			switch (MayaKeyFrame.TangentTypeOut)
			{
				case MFnAnimCurve::kTangentLinear:
					KeyFrame.InterpMode = LLIM_Linear;
					break;
				case MFnAnimCurve::kTangentStep:
					KeyFrame.InterpMode = LLIM_Constant;
					break;
				default:
					KeyFrame.InterpMode = LLIM_Cubic;
					break;
			}

			// TangentMode
			if (FMath::IsNearlyEqual(MayaKeyFrame.TangentValueIn[0], MayaKeyFrame.TangentValueOut[0], 1.e-4) &&
				MayaKeyFrame.TangentLocked)
			{
				KeyFrame.TangentMode = LLTM_User;
			}
			else
			{
				KeyFrame.TangentMode = LLTM_Break;
			}

			// TangentWeightMode
			if (!FMath::IsNearlyEqual(MayaKeyFrame.TangentValueIn[1], MayaKeyFrame.TangentValueOut[1], 1.e-4))
			{
				KeyFrame.TangentWeightMode = LLTWM_WeightedBoth;
				KeyFrame.TangentWeightIn = Time > 0.0 ? MayaKeyFrame.TangentValueIn[1] : 0.0;
				KeyFrame.TangentWeightOut = MayaKeyFrame.TangentValueOut[1];
			}
			else
			{
				KeyFrame.TangentWeightMode = LLTWM_WeightedNone;
				KeyFrame.TangentWeightIn = 1.0;
				KeyFrame.TangentWeightOut = 1.0;
			}

			KeyFrames.Emplace(Time, KeyFrame);
		}
		CurveData.Curves.Emplace(FString(CurvePair.first.c_str()), MoveTemp(AnimCurve));
	}
}

void MStreamedEntity::OnAnimCurveEdited(const MString& AnimCurveNameIn, MObject& AnimCurveObject, const MPlug& Plug, double ConversionFactor)
{
	if (!IsLinked())
	{
		AnimCurves.clear();
		return;
	}

	const bool bBakeTransform = ShouldBakeTransform();

	auto GetCurveNameIndex = [](const std::string& CurveName, const std::array<std::string, 3>& CurveNames) -> int
	{
		int Index = -1;
		auto NamesEnd = CurveNames.end();
		auto Iter = std::find(CurveNames.begin(), NamesEnd, CurveName);
		if (Iter != NamesEnd)
		{
			Index = CurveNames.size() - std::distance(Iter, NamesEnd);
		}
		return Index;
	};

	std::string AnimCurveName = AnimCurveNameIn.asChar();
	int RotationIndex = GetCurveNameIndex(AnimCurveName, RotationNames);
	int LocationIndex = GetCurveNameIndex(AnimCurveName, LocationNames);
	int ScaleIndex = GetCurveNameIndex(AnimCurveName, ScaleNames);

	// Check to see if we're editing a rotation curve.
	// If we do and rotation curves are already baked, no need to process this one any further
	if ((RotationIndex >= 0 || ((LocationIndex >= 0 || ScaleIndex >= 0) && bBakeTransform)) && bTransformCurvesBaked)
	{
		return;
	}

	if (RotationIndex == -1 && ((LocationIndex == -1 && ScaleIndex == -1) || !bBakeTransform))
	{
		// Add or clear the anim curve to the list of anim curves
		auto AnimCurveIter = AnimCurves.find(AnimCurveName);
		if (AnimCurveIter != AnimCurves.end())
		{
			AnimCurveIter->second.KeyFrames.clear();
		}
		else
		{
			MAnimCurve Curve;
			// Using AnimCurveName instead of AnimCurveNameIn.asChar() in the next line 
			// is resulting in memory corruption on Linux in Release build, probably due to 
			// a compiler optimization.
			AnimCurves.emplace(AnimCurveNameIn.asChar(), std::move(Curve));
		}
	}
	else
	{
		auto CreateOrClearAnimCurves = [this](const std::array<std::string, 3>& CurveNames)
		{
			for (auto& CurveName : CurveNames)
			{
				auto AnimCurveIter = AnimCurves.find(CurveName);
				if (AnimCurveIter != AnimCurves.end())
				{
					// Clear the rotation curves if we haven't already baked them
					if (!bTransformCurvesBaked)
					{
						AnimCurveIter->second.KeyFrames.clear();
					}
				}
				else
				{
					MAnimCurve Curve;
					AnimCurves.emplace(CurveName, std::move(Curve));
				}
			}
		};

		CreateOrClearAnimCurves(RotationNames);

		if (bBakeTransform)
		{
			CreateOrClearAnimCurves(LocationNames);
			CreateOrClearAnimCurves(ScaleNames);
		}
	}

	const bool ValidAnimCurve = !AnimCurveObject.isNull();
	const auto LinearUnit = MDistance::uiUnit();
	const auto AngularUnit = MAngle::uiUnit();
	double Conversion = ConversionFactor;
	if (ValidAnimCurve && ConversionFactor == 1.0)
	{
		MFnAnimCurve FnAnimCurve(AnimCurveObject);

		switch (FnAnimCurve.animCurveType())
		{
			case MFnAnimCurve::kAnimCurveTA:
			case MFnAnimCurve::kAnimCurveUA:
			{
				MAngle angle(1.0);
				Conversion = angle.as(AngularUnit);
				break;
			}
			case MFnAnimCurve::kAnimCurveTL:
			case MFnAnimCurve::kAnimCurveUL:
			{
				MDistance distance(1.0);
				Conversion = distance.as(LinearUnit);
				break;
			}
		}
	}

	// Special case where we bake transform curves because Maya and Unreal are not using the same
	// coordinate system
	if (RotationIndex >= 0 || ((LocationIndex >= 0 || ScaleIndex >= 0) && bBakeTransform))
	{
		if (!bTransformCurvesBaked)
		{
			BakeTransformCurves(!bBakeTransform);
		}
	}
	else
	{
		auto AnimCurveIter = AnimCurves.find(AnimCurveName);
		if (AnimCurveIter != AnimCurves.end())
		{
			MAnimCurve& AnimCurve = AnimCurveIter->second;
			if (ValidAnimCurve)
			{
				UpdateAnimCurveKeys(AnimCurveObject, AnimCurve, LocationIndex, ScaleIndex, Conversion);
			}
			else
			{
				// AnimCurve doesn't exist because the attribute is not keyed

				// Need to update the current frame because the value in MFnAnimCurve is from the previous time
				auto& KeyFrame = AnimCurve.FindOrAddKeyFrame(MAnimControl::currentTime().value(), true);
				if (LocationIndex >= 0)
				{
					FTransform UnrealTransform = ComputeUnrealTransform();
					KeyFrame.Value = UnrealTransform.GetTranslation()[LocationIndex];
				}
				else if (ScaleIndex >= 0)
				{
					FTransform UnrealTransform = ComputeUnrealTransform();
					KeyFrame.Value = IsScaleSupported() ? UnrealTransform.GetScale3D()[ScaleIndex] : 1.0f;
				}
				else
				{
					KeyFrame.Value = Plug.asDouble() * Conversion;
				}

				KeyFrame.TangentLocked = true;
				KeyFrame.UpdateTangentValue(0.0, MFnAnimCurve::kTangentLinear);
			}
		}
	}
}

void MStreamedEntity::UpdateAnimCurveKeys(MObject& AnimCurveObject,
										  MAnimCurve& AnimCurve,
										  int LocationIndex,
										  int ScaleIndex,
										  double Conversion)
{
	if (LocationIndex >= 0 && ScaleIndex >= 0)
	{
		return;
	}

	// Compute the anim curve using the available keyframes
	MFnAnimCurve FnAnimCurve(AnimCurveObject);
	const auto IsYAxisUp = MGlobal::isYAxisUp();

	for (unsigned int Index = 0; Index < FnAnimCurve.numKeys(); ++Index)
	{
		auto MayaTime = FnAnimCurve.time(Index);
		auto Time = MayaTime.value();

		// Find the keyframe time
		MKeyFrame& KeyFrame = AnimCurve.FindOrAddKeyFrame(Time);

		float Factor = 1.0f;

		// Key frame value
		if (LocationIndex >= 0)
		{
			MDGContext timeContext(MayaTime);
			FTransform UnrealTransform = ComputeUnrealTransform(timeContext);
			KeyFrame.Value = UnrealTransform.GetTranslation()[LocationIndex];
			if (!IsYAxisUp && LocationIndex == 1)
			{
				Factor = -1.0f;
			}
		}
		else if (ScaleIndex >= 0)
		{
			MDGContext timeContext(MayaTime);
			FTransform UnrealTransform = ComputeUnrealTransform(timeContext);
			KeyFrame.Value = IsScaleSupported() ? UnrealTransform.GetScale3D()[ScaleIndex] : 1.0f;
		}
		else
		{
			KeyFrame.Value = FnAnimCurve.value(Index) * Conversion;
		}

		// Key frame tangent parameters
		KeyFrame.TangentTypeIn = FnAnimCurve.inTangentType(Index);
		MAngle Angle;
		FnAnimCurve.getTangent(Index, Angle, KeyFrame.TangentValueIn[1], true);
		KeyFrame.TangentLocked = FnAnimCurve.tangentsLocked(Index);
		KeyFrame.TangentValueIn[0] = SAFE_TANGENT(tan(Angle.asRadians())) * Factor * Conversion;
		KeyFrame.TangentTypeIn = FnAnimCurve.inTangentType(Index);
		FnAnimCurve.getTangent(Index, Angle, KeyFrame.TangentValueOut[1], false);
		KeyFrame.TangentValueOut[0] = SAFE_TANGENT(tan(Angle.asRadians())) * Factor * Conversion;
		KeyFrame.TangentTypeOut = FnAnimCurve.outTangentType(Index);
	}
}

void MStreamedEntity::RegisterNodeCallbacks(const MDagPath& DagPath, bool IsRoot)
{
	if (!DagPath.isValid())
	{
		return;
	}

	MStatus Status;
	MObject Node = DagPath.node(&Status);
	if (!Status)
	{
		MGlobal::displayWarning("Could not get node for DAG path " + DagPath.fullPathName());
		return;
	}

	// AboutToDelete callback
	auto AboutToDeleteCallbackId = MNodeMessage::addNodeAboutToDeleteCallback(Node, AboutToDeleteCallback, nullptr, &Status);
	if (Status)
	{
		CallbackIds.append(AboutToDeleteCallbackId);
	}
	else
	{
		MGlobal::displayWarning("Could not attach about to delete callback for node.");
		UnregisterNodeCallbacks();
		return;
	}

	// NameChanged callback
	auto NameChangedCallbackId = MNodeMessage::addNameChangedCallback(Node, NameChangedCallback, nullptr, &Status);
	if (Status)
	{
		CallbackIds.append(NameChangedCallbackId);
	}
	else
	{
		MGlobal::displayWarning("Could not attach name changed callback for node.");
		UnregisterNodeCallbacks();
		return;
	}

	// AttributeChanged callback
	auto AttributeChangedCallbackId = MNodeMessage::addAttributeChangedCallback(Node, AttributeChangedCallback, &RootDagPath, &Status);
	if (Status)
	{
		CallbackIds.append(AttributeChangedCallbackId);
	}
	else
	{
		MGlobal::displayWarning("Could not attach attribute changed callback for node.");
		UnregisterNodeCallbacks();
		return;
	}

	// Add children
	MFnDagNode DagNode(Node, &Status);
	if (Status)
	{
		if (IsRoot)
		{
			ProcessBlendShapes(Node);

			RegisterParentNodeRecursive(Node);
		}

		if (Node.hasFn(MFn::kIkEffector))
		{
			MPlug HandlePathPlugArray = DagNode.findPlug("handlePath", false);
			if (!HandlePathPlugArray.isNull() && HandlePathPlugArray.isArray())
			{
				for (unsigned int i = 0; i < HandlePathPlugArray.numElements(); ++i)
				{
					const MPlug& HandlePathPlug = HandlePathPlugArray[i];
					if (!HandlePathPlug.isNull())
					{
						MPlugArray SrcPlugArray;
						HandlePathPlug.connectedTo(SrcPlugArray, false, true);
						for (unsigned int PlugIdx = 0; PlugIdx < SrcPlugArray.length(); ++PlugIdx)
						{
							MPlug& SrcPlug = SrcPlugArray[PlugIdx];
							MObject SrcObject = SrcPlug.node();
							if (SrcObject.hasFn(MFn::kIkHandle))
							{
								MCallbackId CallbackId = MNodeMessage::addAttributeChangedCallback(SrcObject, AttributeChangedCallback, &RootDagPath, &Status);
								if (Status)
								{
									CallbackIds.append(CallbackId);
								}
							}
						}
					}
				}
			}
		}

		if (!HIKEffectorsProcessed && Node.hasFn(MFn::kJoint))
		{
			ProcessHumanIKEffectors(Node);
		}

		if (Node.hasFn(MFn::kConstraint))
		{
			ProcessConstraints(DagNode);
		}

		ProcessMotionPaths(DagNode);

		const auto ChildCount = DagNode.childCount();
		for (unsigned int Child = 0; Child < ChildCount; ++Child)
		{
			MObject ChildObject = DagNode.child(Child);
			if (ChildObject.hasFn(MFn::kDagNode))
			{
				MDagPath ChildDagPath;
				Status = MDagPath::getAPathTo(ChildObject, ChildDagPath);
				if (Status && ChildDagPath.isValid())
				{
					RegisterNodeCallbacks(ChildDagPath, false);
				}
			}
		}
	}
}

void MStreamedEntity::UnregisterNodeCallbacks()
{
	if (CallbackIds.length() != 0)
	{
		MMessage::removeCallbacks(CallbackIds);
		CallbackIds.clear();
	}
}

void MStreamedEntity::RegisterParentNode(MObject& ParentNode)
{
	MStatus Status;
	MCallbackId CallbackId = MNodeMessage::addAttributeChangedCallback(ParentNode, AttributeChangedCallback, &RootDagPath, &Status);
	if (Status)
	{
		CallbackIds.append(CallbackId);
	}
}

void MStreamedEntity::AboutToDeleteCallback(MObject& Node, MDGModifier& Modifier, void* ClientData)
{
	if (Node.hasFn(MFn::kDagNode))
	{
		MDagPath DagPath;
		MStatus Status = MDagPath::getAPathTo(Node, DagPath);
		if (Status && DagPath.isValid())
		{
			// Remove the subject from LiveLink
			MayaLiveLinkStreamManager::TheOne().RemoveSubject(DagPath.fullPathName());
			MayaUnrealLiveLinkUtils::RefreshUI();
		}
	}
}

void MStreamedEntity::NameChangedCallback(MObject& Node, const MString& OldName, void* ClientData)
{
	MayaUnrealLiveLinkUtils::RefreshUI();
}

void MStreamedEntity::AttributeChangedCallback(MNodeMessage::AttributeMessage Msg,
											   MPlug& Plug,
											   MPlug& OtherPlug,
											   void* ClientData)
{
	// Check which attribute was set
	if (Msg & MNodeMessage::kAttributeSet)
	{
		MStatus Status;
		MObject Object = Plug.node(&Status);
		if (Status && ClientData)
		{
			MDagPath& DagPath = *reinterpret_cast<MDagPath*>(ClientData);
			MayaLiveLinkStreamManager::TheOne().OnAttributeChanged(DagPath, Object, Plug, OtherPlug);
		}
	}
}

void MStreamedEntity::OnAttributeChanged(const MObject& Object, const MPlug& Plug, const MPlug& OtherPlug)
{
	if (!IsLinked() || Object.isNull())
	{
		return;
	}

	if (Object.hasFn(MFn::kNurbsCurve) || (Object.hasFn(MFn::kTransform) && ShouldBakeTransform()))
	{
		MFnDagNode DagNode(RootDagPath);
		MPlug TransformPlug = DagNode.findPlug("translateX", false);
		if (!TransformPlug.isNull())
		{
			MObjectArray ObjectArray;
			MAnimUtil::findAnimation(TransformPlug, ObjectArray);

			// Notify that we want to send these anim curves
			if (ObjectArray.length() != 0)
			{
				bool bInternalUpdate = true;
				::OnAnimCurveEdited(ObjectArray, &bInternalUpdate);
			}
		}
	}
}

void MStreamedEntity::ProcessBlendShapes(const MObject& SubjectObject)
{
	MStatus Status;

	BlendShapeNames.clear();

	MDagPathArray DagPathArray;

	// Iterate through blendshapes
	MItDependencyNodes BlendShapeIterator(MFn::kBlendShape);
	while (!BlendShapeIterator.isDone())
	{
		MFnBlendShapeDeformer BlendShape(BlendShapeIterator.thisNode());

		MPlug WeightPlug = BlendShape.findPlug("weight", false);
		if (WeightPlug.isNull() || !WeightPlug.isArray())
		{
			BlendShapeIterator.next();
			continue;
		}

		// Get the base objects of the current blendshape.
		// The base objects are the shapes that are to be deformed.
		MObjectArray BaseObjects;
		BlendShape.getBaseObjects(BaseObjects);
		const MString InMeshString("inMesh");

		// Iterate through the base objects to see if they are associated with the subject.
		for (unsigned int IdxBaseObject = 0; IdxBaseObject < BaseObjects.length(); IdxBaseObject++)
		{
			auto& BaseObject = BaseObjects[IdxBaseObject];
			if (!BaseObject.hasFn(MFn::kMesh))
			{
				continue;
			}

			// Find the "inMesh" plug.
			MFnMesh Mesh(BaseObject);
			auto InMeshPlug = Mesh.findPlug(InMeshString, true, &Status);
			if (!Status)
			{
				continue;
			}

			// Check if a skin cluster is attached to the mesh.
			MPlugArray PlugArray;
			InMeshPlug.connectedTo(PlugArray, true, false);
			for (unsigned int i = 0; i < PlugArray.length(); ++i)
			{
				auto PlugNode = PlugArray[i].node();
				if (!PlugNode.hasFn(MFn::kSkinClusterFilter))
				{
					continue;
				}

				// Find the objects influenced by the skin cluster.
				MFnSkinCluster SkinCluster(PlugNode);
				MDagPathArray InfluenceObjectPaths;
				const unsigned int NumInfluenceObjects = SkinCluster.influenceObjects(InfluenceObjectPaths);
				for (unsigned int Dag = 0; Dag < NumInfluenceObjects; ++Dag)
				{
					// Check if the object is part of the subject hierarchy.
					MFnDagNode ChildNode(InfluenceObjectPaths[Dag]);
					if (!ChildNode.isChildOf(SubjectObject))
					{
						continue;
					}

					// Add a callback on the blendshape node to know when it changes.
					auto BlendShapeObj = BlendShapeIterator.thisNode();
					auto CallbackId = MNodeMessage::addAttributeChangedCallback(BlendShapeObj,
																				AttributeChangedCallback,
																				&RootDagPath,
																				&Status);
					if (Status)
					{
						BlendShapeNames.append(BlendShape.name());

						CallbackIds.append(CallbackId);

						ProcessBlendShapeControllers(BlendShape, DagPathArray);
					}
					break;
				}
			}
		}

		BlendShapeIterator.next();
	}
}

bool MStreamedEntity::RegisterController(const MPlug& Plug, MDagPathArray& DagPathArray)
{
	bool Registered = false;
	MStatus Status;
	MPlugArray DstPlugArray;
	Plug.connectedTo(DstPlugArray, true, false);
	for (unsigned int Dst = 0; Dst < DstPlugArray.length(); ++Dst)
	{
		MPlug& DstPlug = DstPlugArray[Dst];
		MObject DstPlugObject = DstPlug.node();
		if (DstPlugObject.hasFn(MFn::kTransform))
		{
			MFnDagNode TransformNode(DstPlugObject, &Status);
			if (Status)
			{
				MDagPath DagPath;
				TransformNode.getPath(DagPath);
				if (DagPath.isValid() && MayaUnrealLiveLinkUtils::AddUnique(DagPath, DagPathArray))
				{
					auto CallbackId = MNodeMessage::addAttributeChangedCallback(DstPlugObject,
																				AttributeChangedCallback,
																				&RootDagPath,
																				&Status);
					if (Status)
					{
						CallbackIds.append(CallbackId);

						DynamicPlugs.append(DstPlug);

						Registered = true;

						RegisterController(DstPlug, DagPathArray);
					}
				}
			}
		}
	}

	return Registered;
}

void MStreamedEntity::ProcessBlendShapeControllers(const MFnBlendShapeDeformer& BlendShape, MDagPathArray& DagPathArray)
{
	MStatus Status;
	MPlugArray Connections;
	BlendShape.getConnections(Connections);

	for (unsigned int Src = 0; Src < Connections.length(); ++Src)
	{
		MPlug& Plug = Connections[Src];
		RegisterController(Plug, DagPathArray);
	}
}

void MStreamedEntity::ProcessHumanIKEffectors(const MObject& Node)
{
	MFnIkJoint Joint(Node);

	// Find the HIK character plug
	MStatus Status;
	auto CharacterPlug = Joint.findPlug("Character", true, &Status);
	if (!Status)
	{
		return;
	}

	// Get the connected plugs to the HIK character plug
	MPlugArray ConnectedPlugs;
	CharacterPlug.connectedTo(ConnectedPlugs, false, true);
	if (ConnectedPlugs.length() == 0)
	{
		return;
	}

	// The connected plugs are on the HIKCharacter node that will be used to
	// match with the HikIKEffectors
	MFnDependencyNode HIKCharacterNode(ConnectedPlugs[0].node());
	HIKCharacterNodeName = HIKCharacterNode.name();

	HIKEffectorsProcessed = true;

	// Look at all the HikIKEffectors in the scene to find the ones affecting the selected subject
	MItDependencyNodes HikIKEffectorIterator(MFn::kHikIKEffector);
	while (!HikIKEffectorIterator.isDone())
	{
		MObject Object = HikIKEffectorIterator.thisNode();

		if (IsUsingHikIKEffector(Object))
		{
			// Add a callback so that we can stream the transforms when an effector is moved
			MCallbackId CallbackId = MNodeMessage::addAttributeChangedCallback(Object,
																			   AttributeChangedCallback,
																			   &RootDagPath,
																			   &Status);
			if (Status)
			{
				CallbackIds.append(CallbackId);
			}
			else
			{
				MGlobal::displayWarning("Could not attach attribute changed callback for node.");
				UnregisterNodeCallbacks();
				return;
			}
		}

		HikIKEffectorIterator.next();
	}
}

bool MStreamedEntity::IsUsingHikIKEffector(const MObject& HikIKEffectorObject)
{
	MStatus Status;
	MFnTransform HikIKEffector(HikIKEffectorObject, &Status);
	if (!Status)
	{
		return false;
	}

	// Get the control set plug which will refer to the control rig
	auto ControlSetPlug = HikIKEffector.findPlug("ControlSet", true, &Status);
	if (!Status)
	{
		return false;
	}

	// Get the source plugs connected to the control set
	MPlugArray ControlSetPlugSrcs;
	ControlSetPlug.connectedTo(ControlSetPlugSrcs, false, true);
	if (ControlSetPlugSrcs.length() == 0)
	{
		return false;
	}

	// Get the control rig node and find the InputCharacterDefinition plug which will refer a HIKCharacter node
	MFnDependencyNode ControlRigNode(ControlSetPlugSrcs[0].node());
	auto ICDPlug = ControlRigNode.findPlug("InputCharacterDefinition", true, &Status);
	if (!Status)
	{
		return false;
	}

	MPlugArray ICDPlugs;
	ICDPlug.connectedTo(ICDPlugs, true, false);
	for (unsigned int i = 0; i < ICDPlugs.length(); ++i)
	{
		MFnDependencyNode ICD(ICDPlugs[i].node());

		// Try to match the InputCharacterDefinition from the effector to the one of this subject
		if (ICD.name() == HIKCharacterNodeName)
		{
			return true;
		}
	}

	return false;
}

void MStreamedEntity::ProcessConstraints(const MFnDagNode& DagNode)
{
	MStatus Status;
	MDagPathArray DagPathArray;
	MPlug TargetPlug = DagNode.findPlug("target", false, &Status);
	if (!Status || TargetPlug.isNull() || !TargetPlug.isArray())
	{
		return;
	}

	for (unsigned int i = 0; i < TargetPlug.numElements(); ++i)
	{
		const MPlug& TargetPlugElement = TargetPlug[i];

		// The target plug has several children, check if any is connected to another node
		for (unsigned int Child = 0; Child < TargetPlugElement.numChildren(); ++Child)
		{
			MPlug ChildPlug = TargetPlugElement.child(Child);
			if (ChildPlug.isNull() || !ChildPlug.isConnected())
			{
				continue;
			}

			// Find the connections
			MPlugArray ChildDests;
			ChildPlug.connectedTo(ChildDests, true, false);
			for (unsigned int j = 0; j < ChildDests.length(); ++j)
			{
				// Check for transforms like locators that could affect this node
				MObject NodeObject = ChildDests[j].node();

				if (!NodeObject.hasFn(MFn::kTransform))
				{
					continue;
				}

				MFnDagNode Transform(NodeObject);
				MDagPath TransformDagPath = Transform.dagPath();
				auto DagPathFound = std::find_if(DagPathArray.begin(), DagPathArray.end(),
												 [&TransformDagPath](const MDagPath& DagPath)
				{
					return DagPath == TransformDagPath;
				});

				// Add a callback to this transform, so that we are aware that it has changed this node
				if (DagPathFound == DagPathArray.end())
				{
					DagPathArray.append(TransformDagPath);
					auto CallbackId = MNodeMessage::addAttributeChangedCallback(NodeObject, AttributeChangedCallback, &RootDagPath, &Status);
					if (Status)
					{
						CallbackIds.append(CallbackId);
						bHasConstraint = true;
					}
				}
			}
		}
	}
}

void MStreamedEntity::ProcessMotionPaths(const MFnDagNode& DagNode)
{
	MStatus Status;
	MObjectArray MotionPaths;
	MPlugArray Connections;
	DagNode.getConnections(Connections);
	for (unsigned int i = 0; i < Connections.length(); ++i)
	{
		MPlug& Connection = Connections[i];
		MObject ConnectionObject = Connection.node();

		MPlugArray PlugArray;
		Connection.connectedTo(PlugArray, true, false);
		for (unsigned int PlugIdx = 0; PlugIdx < PlugArray.length(); ++PlugIdx)
		{
			MPlug& Plug = PlugArray[PlugIdx];
			MObject PlugObject = Plug.node();

			if (PlugObject.hasFn(MFn::kMotionPath))
			{
				auto MotionPathFound = std::find_if(MotionPaths.begin(), MotionPaths.end(),
													[&PlugObject](const MObject& Object)
				{
					return PlugObject == Object;
				});
				if (MotionPathFound == MotionPaths.end())
				{
					MFnMotionPath MotionPath(PlugObject);
					MPlug GeometryPath = MotionPath.findPlug("geometryPath", false);
					if (!GeometryPath.isNull())
					{
						MPlugArray GeometryCurves;
						GeometryPath.connectedTo(GeometryCurves, true, false);
						for (unsigned int GeoIdx = 0; GeoIdx < GeometryCurves.length(); ++GeoIdx)
						{
							MObject GeometryPathNode = GeometryCurves[GeoIdx].node();
							if (GeometryPathNode.hasFn(MFn::kNurbsCurve))
							{
								MFnNurbsCurve Curve(GeometryPathNode);
								auto CallbackId = MNodeMessage::addAttributeChangedCallback(GeometryPathNode,
																							AttributeChangedCallback,
																							&RootDagPath,
																							&Status);
								if (Status)
								{
									CallbackIds.append(CallbackId);
									bHasMotionPath = true;
								}

								if (Curve.parentCount() > 0)
								{
									MObject ParentObject = Curve.parent(0);
									CallbackId = MNodeMessage::addAttributeChangedCallback(ParentObject,
																						   AttributeChangedCallback,
																						   &RootDagPath,
																						   &Status);
									if (Status)
									{
										CallbackIds.append(CallbackId);
									}
								}
							}
						}
					}

					auto CallbackId = MNodeMessage::addAttributeChangedCallback(PlugObject,
																				AttributeChangedCallback,
																				&RootDagPath,
																				&Status);
					if (Status)
					{
						CallbackIds.append(CallbackId);
					}

					MotionPaths.append(PlugObject);
				}
			}
		}
	}
}

void MStreamedEntity::OnStreamCurrentTime()
{
	OnStream(FPlatformTime::Seconds(), MAnimControl::currentTime().value());
}

FTransform MStreamedEntity::ComputeUnrealTransform()
{
	MObject TransformObject = RootDagPath.node();
	MFnTransform TransformNode(TransformObject);
	double Scales[3] = { 1.0, 1.0, 1.0 };
	TransformNode.getScale(Scales);
	MMatrix MayaTransform = MMatrix::identity;
	MayaUnrealLiveLinkUtils::ComputeTransformHierarchy(TransformObject, MayaTransform);
	MayaUnrealLiveLinkUtils::RotateCoordinateSystemForUnreal(MayaTransform);
	FTransform UnrealTransform = MayaUnrealLiveLinkUtils::BuildUETransformFromMayaTransform(MayaTransform);
	FRotator Rotator(GetLevelSequenceRotationOffset().x, GetLevelSequenceRotationOffset().y, GetLevelSequenceRotationOffset().z);
	UnrealTransform.SetRotation(UnrealTransform.GetRotation() * Rotator.Quaternion());

	if (MGlobal::isYAxisUp())
	{
		UnrealTransform.SetScale3D(FVector(Scales[0], Scales[2], Scales[1]));
	}
	else
	{
		UnrealTransform.SetScale3D(FVector(Scales[0], Scales[1], Scales[2]));
	}
	return UnrealTransform;
}

FTransform MStreamedEntity::ComputeUnrealTransform(MDGContext& TimeDGContext)
{
	MDGContextGuard ContextGuard(TimeDGContext);
	return ComputeUnrealTransform();
}

void MStreamedEntity::MAnimCurve::BakeKeyFrame(double Time, double Value, int32 Key, int32 NumKeys)
{
	// Find the keyframe time
	MKeyFrame KeyFrame;

	// Key frame value
	KeyFrame.Value = Value;

	KeyFrame.TangentLocked = true;

	// Key frame tangent parameters
	if (NumKeys > 1)
	{
		// We're computing tangent values inside the current loop where we will not
		// have the curve value for the next frame.
		//
		// To workaround this issue, we're going to compute the tangent value for the
		// previous frame and to have special cases for the first and last frames.
		if (KeyFrames.size() > 0)
		{
			auto PrevKeyFrameIter = KeyFrames.rbegin();
			auto Prev2KeyFrameIter = PrevKeyFrameIter;
			if (KeyFrames.size() > 1)
			{
				++Prev2KeyFrameIter;
			}

			MKeyFrame& PrevKeyFrame = PrevKeyFrameIter->second;
			const MKeyFrame& Prev2KeyFrame = Prev2KeyFrameIter->second;

			// Compute the previous frame tangent value and clamp using Unreal' function
			double TangentValue = ClampFloatTangent(Prev2KeyFrame.Value, Prev2KeyFrameIter->first,
													PrevKeyFrame.Value, PrevKeyFrameIter->first,
													KeyFrame.Value, Time);
			PrevKeyFrame.UpdateTangentValue(TangentValue, MFnAnimCurve::kTangentAuto);

			if (FMath::IsNearlyEqual(KeyFrame.Value, PrevKeyFrame.Value, 1.e-4) &&
				FMath::IsNearlyEqual(TangentValue, Prev2KeyFrame.TangentValueIn[0]))
			{
				PrevKeyFrame.Value = KeyFrame.Value;
				return;
			}
			else if (Time - PrevKeyFrameIter->first > 1.0)
			{
				// Insert a key with the same value but with different tangent
				// to take into account the difference in tangent values
				MKeyFrame NewKeyFrame;
				NewKeyFrame.Value = PrevKeyFrame.Value;
				NewKeyFrame.UpdateTangentValue(ClampFloatTangent(Prev2KeyFrame.Value, Prev2KeyFrameIter->first,
																	NewKeyFrame.Value, Time - 1.0,
																	KeyFrame.Value, Time),
												MFnAnimCurve::kTangentAuto);
				KeyFrames.emplace(Time - 1.0, std::move(NewKeyFrame));
			}
		}

		if (Key == 0)
		{
			// Set the tangent value for the first frame.
			KeyFrame.UpdateTangentValue(0, MFnAnimCurve::kTangentAuto);
		}
		else if (Key == NumKeys - 1)
		{
			// Set the tangent value for the last frame.
			KeyFrame.UpdateTangentValue(0, MFnAnimCurve::kTangentAuto);
		}
	}
	else
	{
		// Special case when the curve has only one frame, don't bake anything
		KeyFrame.UpdateTangentValue(0.0, MFnAnimCurve::kTangentLinear);
	}

	KeyFrames.emplace(Time, std::move(KeyFrame));
}

void MStreamedEntity::BakeTransformCurves(bool bRotationOnly)
{
	const double MaxTime = MAnimControl::maxTime().value();
	const int NumKeys = static_cast<int>(std::ceil(MaxTime));
	MTime MayaTime(0, MTime::uiUnit());

	auto UpdateCurve = [this, NumKeys, MaxTime](int Key,
												double Time,
												const std::array<std::string, 3>& CurveNames,
												const FVector& Values)
	{
		for (auto Curve = 0; Curve < CurveNames.size(); ++Curve)
		{
			auto AnimCurveIter = AnimCurves.find(CurveNames[Curve]);
			if (AnimCurveIter == AnimCurves.end())
			{
				MAnimCurve AnimCurve;
				AnimCurveIter = AnimCurves.emplace(CurveNames[Curve], std::move(AnimCurve)).first;
			}
			MAnimCurve& AnimCurve = AnimCurveIter->second;

			AnimCurve.BakeKeyFrame(Time, Values[Curve], Key, NumKeys);
		}
	};

	for (int Key = 0; Key < NumKeys; ++Key, ++MayaTime)
	{
		MDGContext timeContext(MayaTime);
		double Time = MayaTime.value();

		FTransform UnrealTransform = ComputeUnrealTransform(timeContext);
		UpdateCurve(Key, Time, RotationNames, UnrealTransform.GetRotation().Euler());
		if (!bRotationOnly)
		{
			UpdateCurve(Key, Time, LocationNames, UnrealTransform.GetLocation());
			UpdateCurve(Key, Time, ScaleNames, UnrealTransform.GetScale3D());
		}
	}

	bTransformCurvesBaked = true;
}

bool MStreamedEntity::IsOwningBlendShape(const MString& Name) const
{
	return BlendShapeNames.indexOf(Name) >= 0;
}

void MStreamedEntity::RebuildLevelSequenceSubject(const MString& SubjectName,
												  const MDagPath& DagPath,
												  const MString& SavedAssetName,
												  const MString& SavedAssetPath,
												  const MString& UnrealAssetName,
												  const MString& UnrealAssetPath,
												  bool ForceRelink)
{
	FMayaLiveLinkLevelSequenceStaticData& StaticData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FMayaLiveLinkLevelSequenceStaticData>();
	InitializeStaticData(StaticData, SavedAssetName, SavedAssetPath, UnrealAssetName, UnrealAssetPath);
	MayaLiveLinkStreamManager::TheOne().RebuildLevelSequenceSubject(SubjectName);

	if (ForceRelink)
	{
		UpdateAnimCurves(DagPath);
	}
}

bool MStreamedEntity::ShouldBakeTransform() const
{
	if (bHasMotionPath || bHasConstraint)
	{
		return true;
	}

	MStatus Status;
	MFnDagNode DagNode(RootDagPath, &Status);
	return Status && DagNode.parentCount() != 0;
}

void MStreamedEntity::RegisterParentNodeRecursive(MObject& Node)
{
	if (Node.hasFn(MFn::kDagNode))
	{
		MFnDagNode DagNode(Node);
		if (DagNode.parentCount() != 0)
		{
			MObject ParentObject = DagNode.parent(0);
			RegisterParentNodeRecursive(ParentObject);
		}
		RegisterParentNode(Node);
	}
}
