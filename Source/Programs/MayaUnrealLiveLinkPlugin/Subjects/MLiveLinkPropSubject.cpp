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

#include "MLiveLinkPropSubject.h"
#include "../MayaLiveLinkStreamManager.h"
#include "MayaUnrealLiveLinkPlugin/MayaUnrealLiveLinkUtils.h"
#include "LiveLinkTypes.h"
#include "Roles/MayaLiveLinkTimelineTypes.h"

MLiveLinkPropSubject::MLiveLinkPropSubject(const MString& InSubjectName, const MDagPath& InRootPath, MPropStreamMode InStreamMode)
: IMStreamedEntity(InRootPath)
, SubjectName(InSubjectName)
, StreamMode(InStreamMode >= 0 && InStreamMode < PropStreamOptions.length() ? InStreamMode : MPropStreamMode::RootOnly)
, bLinked(false)
{
}

MString PropStreams[2] = { "Transform", "Animation" };
MStringArray MLiveLinkPropSubject::PropStreamOptions(PropStreams, 2);

MLiveLinkPropSubject::~MLiveLinkPropSubject()
{
	MayaLiveLinkStreamManager::TheOne().RemoveSubjectFromLiveLink(SubjectName);
}

bool MLiveLinkPropSubject::ShouldDisplayInUI() const
{
	return true;
}

const MDagPath& MLiveLinkPropSubject::GetDagPath() const
{
	return RootDagPath;
}

MString MLiveLinkPropSubject::GetNameDisplayText() const
{
	return SubjectName;
}

MString MLiveLinkPropSubject::GetRoleDisplayText() const
{
	return PropStreamOptions[StreamMode];
}

const MString& MLiveLinkPropSubject::GetSubjectTypeDisplayText() const
{
	static const MString PropText("Prop");
	return PropText;
}

bool MLiveLinkPropSubject::ValidateSubject() const
{
	return true;
}

bool MLiveLinkPropSubject::RebuildSubjectData(bool ForceRelink)
{
	if (StreamMode == MPropStreamMode::RootOnly)
	{
		if (!IsLinked())
		{
			MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkTransformStaticData>();
			return MayaLiveLinkStreamManager::TheOne().RebuildPropSubjectData(SubjectName, "RootOnly");
		}
		else
		{
			RebuildLevelSequenceSubject(SubjectName, GetDagPath(), SavedAssetName, SavedAssetPath, UnrealAssetClass, UnrealAssetPath, ForceRelink);
		}
	}
	else if (StreamMode == MPropStreamMode::FullHierarchy)
	{
		auto& SkeletonStaticData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkSkeletonStaticData>();
		DynamicPlugs.clear();
		auto DagPath = GetDagPath();
		MFnDagNode PropNode(DagPath);
		// Add the dynamic attribute names to StaticData if any
		// that would correspond to values to be stored in FrameData.
		for (unsigned int i = 0; i < PropNode.attributeCount(); ++i)
		{
			MFnAttribute Attr(PropNode.attribute(i));
			auto Plug = PropNode.findPlug(Attr.name(), true);

			if (Plug.isDynamic() && Plug.isKeyable())
			{
				DynamicPlugs.append(Plug);
				SkeletonStaticData.PropertyNames.Add(Attr.name().asChar());
			}
		}

		return MayaLiveLinkStreamManager::TheOne().RebuildPropSubjectData(SubjectName, "FullHierarchy");
	}
	return false;
}

void MLiveLinkPropSubject::OnStream(double StreamTime, double CurrentTime)
{
	MFnTransform TransformNode(RootDagPath);
	double Scales[3] = { 1.0, 1.0, 1.0 };
	TransformNode.getScale(Scales);
	MMatrix MayaTransform = MayaTransform.identity;
	MObject TransformObject = RootDagPath.node();
	MayaUnrealLiveLinkUtils::ComputeTransformHierarchy(TransformObject, MayaTransform);
	MayaUnrealLiveLinkUtils::RotateCoordinateSystemForUnreal(MayaTransform);

	auto UnrealTransform = MayaUnrealLiveLinkUtils::BuildUETransformFromMayaTransform(MayaTransform);
	if (MGlobal::isYAxisUp())
	{
		UnrealTransform.SetRotation(UnrealTransform.GetRotation() * FRotator(0.0f, 0.0f, -90.0f).Quaternion());
	}

	const auto SceneTime = MayaUnrealLiveLinkUtils::GetMayaFrameTimeAsUnrealTime();

	if (StreamMode == MPropStreamMode::RootOnly)
	{
		if (!IsLinked())
		{
			if (MGlobal::isYAxisUp())
			{
				UnrealTransform.SetScale3D(FVector(Scales[0], Scales[2], Scales[1]));
			}
			else
			{
				UnrealTransform.SetScale3D(FVector(Scales[0], Scales[1], Scales[2]));
			}

			auto& TransformData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkTransformFrameData>();
			TransformData.Transform = UnrealTransform;
			TransformData.WorldTime = StreamTime;
			TransformData.MetaData.SceneTime = SceneTime;

			MayaLiveLinkStreamManager::TheOne().OnStreamPropSubject(SubjectName, "RootOnly");
		}
		else
		{
			if (AnimCurves.size() != 0)
			{
				auto& FrameData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FMayaLiveLinkLevelSequenceFrameData>();
				InitializeFrameData(FrameData);
				AnimCurves.clear();
				MayaLiveLinkStreamManager::TheOne().OnStreamLevelSequenceSubject(SubjectName);
			}
		}
	}
	else if (StreamMode == MPropStreamMode::FullHierarchy)
	{
		auto& AnimationData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkAnimationFrameData>();
		AnimationData.Transforms.Add(UnrealTransform);
		AnimationData.WorldTime = StreamTime;
		AnimationData.MetaData.SceneTime = SceneTime;

		// Add the dynamic plug values as property values in AnimaFrameData
		for (unsigned int i = 0; i < DynamicPlugs.length(); ++i)
		{
			auto& Plug = DynamicPlugs[i];
			AnimationData.PropertyValues.Add(Plug.asFloat());
		}

		MayaLiveLinkStreamManager::TheOne().OnStreamPropSubject(SubjectName, "FullHierarchy");
	}
}

void MLiveLinkPropSubject::SetStreamType(const MString& StreamTypeIn)
{
	for (uint32_t StreamTypeIdx = 0; StreamTypeIdx < PropStreamOptions.length(); ++StreamTypeIdx)
	{
		if (PropStreamOptions[StreamTypeIdx] == StreamTypeIn && StreamMode != (MPropStreamMode)StreamTypeIdx)
		{
			SetStreamType(static_cast<MPropStreamMode>(StreamTypeIdx));
			return;
		}
	}
}

void MLiveLinkPropSubject::SetStreamType(MPropStreamMode StreamModeIn)
{
	StreamMode = StreamModeIn;
	if (StreamModeIn != MPropStreamMode::RootOnly)
	{
		UnrealAssetPath.clear();
		SavedAssetPath.clear();
		SavedAssetName.clear();
	}
	RebuildSubjectData();
}

int MLiveLinkPropSubject::GetStreamType() const
{
	return StreamMode;
}

void MLiveLinkPropSubject::LinkUnrealAsset(const LinkAssetInfo& LinkInfo)
{
	if (!bLinked ||
		(bLinked &&
		 (LinkInfo.UnrealAssetPath != UnrealAssetPath ||
		  LinkInfo.UnrealAssetClass != UnrealAssetClass ||
		  LinkInfo.SavedAssetPath != SavedAssetPath ||
		  LinkInfo.SavedAssetName != SavedAssetName ||
		  LinkInfo.UnrealNativeClass != UnrealNativeClass)))
	{
		UnrealAssetPath = LinkInfo.UnrealAssetPath;
		UnrealAssetClass = LinkInfo.UnrealAssetClass;
		SavedAssetPath = LinkInfo.SavedAssetPath;
		SavedAssetName = LinkInfo.SavedAssetName;
		UnrealNativeClass = LinkInfo.UnrealNativeClass;

		if (!LinkInfo.bSetupOnly)
		{
			bLinked = true;

			RebuildSubjectData();

			// Wait a bit after rebuilding the subject data before sending the curve data to Unreal.
			// Otherwise, Unreal will ignore it.
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(100ms);

			UpdateAnimCurves(RootDagPath);
		}
	}
}

void MLiveLinkPropSubject::UnlinkUnrealAsset()
{
	bLinked = false;
	SetStreamType(StreamMode);
	OnStreamCurrentTime();
}

bool MLiveLinkPropSubject::IsLinked() const
{
	return bLinked &&
		   UnrealAssetPath.length() &&
		   UnrealAssetClass.length() &&
		   SavedAssetPath.length() &&
		   SavedAssetName.length();
}

const MVector& MLiveLinkPropSubject::GetLevelSequenceRotationOffset() const
{
	static const MVector RotationOffset(0.0f, 0.0f, -90.0f);
	return MGlobal::isYAxisUp() ? RotationOffset : MVector::zero;
}
