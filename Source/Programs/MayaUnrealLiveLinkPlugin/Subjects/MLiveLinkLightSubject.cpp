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

#include "MLiveLinkLightSubject.h"
#include "../MayaLiveLinkStreamManager.h"
#include "LiveLinkTypes.h"
#include "Roles/MayaLiveLinkTimelineTypes.h"

#include "../MayaUnrealLiveLinkUtils.h"

THIRD_PARTY_INCLUDES_START
#include <maya/MFnLight.h>
#include <maya/MFnSpotLight.h>
THIRD_PARTY_INCLUDES_END

static const MString LightStreams[3] = { "Transform", "Animation", "Light" };
const MStringArray MLiveLinkLightSubject::LightStreamOptions(LightStreams, 3);

MLiveLinkLightSubject::MLiveLinkLightSubject(const MString& InSubjectName, const MDagPath& InRootPath, MLightStreamMode InStreamMode)
: IMStreamedEntity(InRootPath)
, SubjectName(InSubjectName)
, StreamMode(InStreamMode >= 0 && InStreamMode < LightStreamOptions.length() ? InStreamMode : MLightStreamMode::Light)
, bLinked(false)
{
}

MLiveLinkLightSubject::~MLiveLinkLightSubject()
{
	MayaLiveLinkStreamManager::TheOne().RemoveSubjectFromLiveLink(SubjectName);
}

bool MLiveLinkLightSubject::ShouldDisplayInUI() const
{
	return true;
}

const MDagPath& MLiveLinkLightSubject::GetDagPath() const
{
	return RootDagPath;
}

MString MLiveLinkLightSubject::GetNameDisplayText() const
{
	return SubjectName;
}

MString MLiveLinkLightSubject::GetRoleDisplayText() const
{
	return LightStreamOptions[StreamMode];
}

const MString& MLiveLinkLightSubject::GetSubjectTypeDisplayText() const
{
	static const MString LightText("Light");
	return LightText;
}

bool MLiveLinkLightSubject::ValidateSubject() const
{
	return true;
}

bool MLiveLinkLightSubject::RebuildSubjectData(bool ForceRelink)
{
	bool ValidSubject = false;
	bool IsSpotLight = RootDagPath.hasFn(MFn::kSpotLight);

	if (StreamMode == MLightStreamMode::RootOnly)
	{
		MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkTransformStaticData>();
		return MayaLiveLinkStreamManager::TheOne().RebuildLightSubjectData(SubjectName, "RootOnly");
	}
	else if (StreamMode == MLightStreamMode::FullHierarchy)
	{
		MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkSkeletonStaticData>();
		return MayaLiveLinkStreamManager::TheOne().RebuildLightSubjectData(SubjectName, "FullHierarchy");
	}
	else if (StreamMode == MLightStreamMode::Light)
	{
		if (!IsLinked())
		{
			auto& LightData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkLightStaticData>();
			LightData.bIsInnerConeAngleSupported = IsSpotLight;
			LightData.bIsOuterConeAngleSupported = IsSpotLight;
			return MayaLiveLinkStreamManager::TheOne().RebuildLightSubjectData(SubjectName, "Light");
		}
		else
		{
			RebuildLevelSequenceSubject(SubjectName, GetDagPath(), SavedAssetName, SavedAssetPath, UnrealAssetClass, UnrealAssetPath, ForceRelink);
			return true;
		}
	}
	return ValidSubject;
}

void MLiveLinkLightSubject::OnStream(double StreamTime, double CurrentTime)
{
	MMatrix MayaTransform = MayaTransform.identity;
	MObject TransformObject = RootDagPath.node();
	MayaUnrealLiveLinkUtils::ComputeTransformHierarchy(TransformObject, MayaTransform);
	MayaUnrealLiveLinkUtils::RotateCoordinateSystemForUnreal(MayaTransform);
	auto UnrealTransform = MayaUnrealLiveLinkUtils::BuildUETransformFromMayaTransform(MayaTransform);
	UnrealTransform.SetRotation(UnrealTransform.GetRotation() * FRotator(-90.0f, 0.0f, 0.0f).Quaternion());
	const auto SceneTime = MayaUnrealLiveLinkUtils::GetMayaFrameTimeAsUnrealTime();

	if (StreamMode == MLightStreamMode::RootOnly)
	{
		auto& TransformFrameData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkTransformFrameData>();
		TransformFrameData.Transform = UnrealTransform;
		TransformFrameData.WorldTime = StreamTime;
		TransformFrameData.MetaData.SceneTime = SceneTime;

		MayaLiveLinkStreamManager::TheOne().OnStreamLightSubject(SubjectName, "RootOnly");
	}
	else if (StreamMode == MLightStreamMode::FullHierarchy)
	{
		auto& AnimationData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkAnimationFrameData>();
		AnimationData.Transforms.Add(UnrealTransform);
		AnimationData.WorldTime = StreamTime;
		AnimationData.MetaData.SceneTime = SceneTime;

		MayaLiveLinkStreamManager::TheOne().OnStreamLightSubject(SubjectName, "FullHierarchy");
	}
	else if (StreamMode == MLightStreamMode::Light)
	{
		if (!IsLinked())
		{
			MFnLight Light(RootDagPath);

			auto& LightFrameData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkLightFrameData>();
			LightFrameData.Transform = UnrealTransform;
			LightFrameData.WorldTime = StreamTime;
			LightFrameData.MetaData.SceneTime = SceneTime;
			LightFrameData.Intensity = Light.intensity();
			LightFrameData.LightColor = MayaUnrealLiveLinkUtils::MayaColorToUnreal(Light.color());

			if (RootDagPath.hasFn(MFn::kSpotLight))
			{
				MFnSpotLight SpotLight(RootDagPath);
				auto outerAngle = SpotLight.coneAngle() * 0.5;
				auto penumbraAngle = SpotLight.penumbraAngle() * 0.5;
				if (penumbraAngle < 0.0)
				{
					LightFrameData.InnerConeAngle = static_cast<float>(MayaUnrealLiveLinkUtils::RadToDeg(-penumbraAngle));
				}
				else
				{
					outerAngle += penumbraAngle;
					LightFrameData.InnerConeAngle = static_cast<float>(MayaUnrealLiveLinkUtils::RadToDeg(outerAngle));
				}
				LightFrameData.OuterConeAngle = static_cast<float>(MayaUnrealLiveLinkUtils::RadToDeg(outerAngle));
			}
			MayaLiveLinkStreamManager::TheOne().OnStreamLightSubject(SubjectName, "Light");
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
}

void MLiveLinkLightSubject::SetStreamType(const MString& StreamTypeIn)
{
	for (uint32_t StreamTypeIdx = 0; StreamTypeIdx < LightStreamOptions.length(); ++StreamTypeIdx)
	{
		if (LightStreamOptions[StreamTypeIdx] == StreamTypeIn && StreamMode != (MLightStreamMode)StreamTypeIdx)
		{
			SetStreamType(static_cast<MLightStreamMode>(StreamTypeIdx));
			return;
		}
	}
}

void MLiveLinkLightSubject::SetStreamType(MLightStreamMode StreamModeIn)
{
	StreamMode = StreamModeIn;
	if (StreamModeIn != MLightStreamMode::Light)
	{
		UnrealAssetPath.clear();
		UnrealAssetClass.clear();
		SavedAssetPath.clear();
		SavedAssetName.clear();
	}
	RebuildSubjectData();
}

int MLiveLinkLightSubject::GetStreamType() const
{
	return StreamMode;
}

void MLiveLinkLightSubject::LinkUnrealAsset(const LinkAssetInfo& LinkInfo)
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

			UpdateAnimCurves(GetDagPath());
		}
	}
}

void MLiveLinkLightSubject::UnlinkUnrealAsset()
{
	bLinked = false;
	SetStreamType(StreamMode);
	OnStreamCurrentTime();
}

bool MLiveLinkLightSubject::IsLinked() const
{
	return bLinked &&
		   UnrealAssetPath.length() &&
		   UnrealAssetClass.length() &&
		   SavedAssetPath.length() &&
		   SavedAssetName.length();
}
