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
#include "../MayaCommonIncludes.h"
#include "LiveLinkTypes.h"

#include "../MayaUnrealLiveLinkUtils.h"

MLiveLinkLightSubject::MLiveLinkLightSubject(const MString& InSubjectName, const MDagPath& InRootPath, FLightStreamMode InStreamMode)
	: MStreamedEntity(InRootPath)
	, SubjectName(InSubjectName)
	, RootDagPath(InRootPath)
	, StreamMode(InStreamMode >= 0 && InStreamMode < LightStreamOptions.length() ? InStreamMode : FLightStreamMode::Light)
{}

MString LightStreams[3] ={"Root Only", "Full Hierarchy", "Light"};
MStringArray MLiveLinkLightSubject::LightStreamOptions(LightStreams, 3);
MLiveLinkLightSubject::~MLiveLinkLightSubject()
{
	MayaLiveLinkStreamManager::TheOne().RemoveSubjectFromLiveLink(SubjectName);
}

bool MLiveLinkLightSubject::ShouldDisplayInUI() const
{
    return true;
}

 MDagPath MLiveLinkLightSubject::GetDagPath() const
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

MString MLiveLinkLightSubject::GetSubjectTypeDisplayText() const
{
    return MString("Light");
}

bool MLiveLinkLightSubject::ValidateSubject() const
{
    return true;
}

bool MLiveLinkLightSubject::RebuildSubjectData()
{
    bool ValidSubject = false;
	bool IsSpotLight = RootDagPath.hasFn(MFn::kSpotLight);

    if (StreamMode == FLightStreamMode::RootOnly)
    {
		MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkTransformStaticData>();
		return MayaLiveLinkStreamManager::TheOne().RebuildLightSubjectData(SubjectName, "RootOnly");
    }
    else if (StreamMode == FLightStreamMode::FullHierarchy)
    {
		MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkSkeletonStaticData>();
		return MayaLiveLinkStreamManager::TheOne().RebuildLightSubjectData(SubjectName, "FullHierarchy");
    }
	else if (StreamMode == FLightStreamMode::Light)
	{
		auto& LightData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkLightStaticData>();
		LightData.bIsInnerConeAngleSupported = IsSpotLight;
		LightData.bIsOuterConeAngleSupported = IsSpotLight;
		return MayaLiveLinkStreamManager::TheOne().RebuildLightSubjectData(SubjectName, "Light");
	}
    return ValidSubject;
}

void MLiveLinkLightSubject::OnStream(double StreamTime, double CurrentTime)
{
	MFnTransform TransformNode(RootDagPath);
	MMatrix MayaTransform = TransformNode.transformation().asMatrix();
	MayaUnrealLiveLinkUtils::RotateCoordinateSystemForUnreal(MayaTransform);
	auto UnrealTransform = MayaUnrealLiveLinkUtils::BuildUETransformFromMayaTransform(MayaTransform);
	UnrealTransform.SetRotation(UnrealTransform.GetRotation() * FRotator(-90.0f, 0.0f, 0.0f).Quaternion());
	const auto SceneTime = MayaUnrealLiveLinkUtils::GetMayaFrameTimeAsUnrealTime();

	if (StreamMode == FLightStreamMode::RootOnly)
	{
		auto& TransformFrameData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkTransformFrameData>();
		TransformFrameData.Transform = UnrealTransform;
		TransformFrameData.WorldTime = StreamTime;
		TransformFrameData.MetaData.SceneTime = SceneTime;

		MayaLiveLinkStreamManager::TheOne().OnStreamLightSubject(SubjectName, "RootOnly");
	}
	else if (StreamMode == FLightStreamMode::FullHierarchy)
	{
		auto& AnimationData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkAnimationFrameData>();
		AnimationData.Transforms.Add(UnrealTransform);
		AnimationData.WorldTime = StreamTime;
		AnimationData.MetaData.SceneTime = SceneTime;

		MayaLiveLinkStreamManager::TheOne().OnStreamLightSubject(SubjectName, "FullHierarchy");
	}
	else if (StreamMode == FLightStreamMode::Light)
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
}

void MLiveLinkLightSubject::SetStreamType(const MString& StreamTypeIn)
{
    for (uint32_t StreamTypeIdx = 0; StreamTypeIdx < LightStreamOptions.length(); ++StreamTypeIdx)
    {
        if (LightStreamOptions[StreamTypeIdx] == StreamTypeIn && StreamMode != (FLightStreamMode)StreamTypeIdx)
        {
            StreamMode = (FLightStreamMode)StreamTypeIdx;
            RebuildSubjectData();
            return;
        }
    }
}

int MLiveLinkLightSubject::GetStreamType() const
{
    return StreamMode;
}