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

#include "MLiveLinkCameraSubject.h"
#include "MFilmbackRedirectCurve.h"
#include "MFocalLengthRedirectCurve.h"
#include "../MayaLiveLinkStreamManager.h"
#include "Roles/LiveLinkCameraTypes.h"

static const std::map<const std::string, const std::shared_ptr<const MRedirectCurve<MFnCamera>>> RedirectedCurves =
{
	{ "CurrentFocalLength", std::make_shared<MFocalLengthRedirectCurve>("FieldOfView") },
	{ "Filmback.SensorWidth", std::make_shared<MFilmbackRedirectCurve>("AspectRatio") },
	{ "Filmback.SensorHeight", std::make_shared<MFilmbackRedirectCurve>("AspectRatio") },
};

MLiveLinkCameraSubject::MLiveLinkCameraSubject(const MString& InSubjectName,
											   MDagPath InDagPath,
											   MLiveLinkBaseCameraSubject::MCameraStreamMode InStreamMode)
: MLiveLinkBaseCameraSubject(InSubjectName, InStreamMode, InDagPath), CameraPath(InDagPath)
, bIsCineCamera(false)
, bLinked(false)
{
}

bool MLiveLinkCameraSubject::ShouldDisplayInUI() const
{
	return true;
}

const MDagPath& MLiveLinkCameraSubject::GetDagPath() const
{
	return CameraPath;
}

bool MLiveLinkCameraSubject::RebuildSubjectData(bool ForceRelink)
{
	bool ValidSubject = false;
	MLiveLinkBaseCameraSubject::RebuildSubjectData(ForceRelink);

	if (StreamMode == MCameraStreamMode::Camera)
	{
		MFnCamera MayaCamera(CameraPath);
		MStatus Status;
		const bool IsDepthOfFieldEnabled = MayaCamera.isDepthOfField(&Status);
		if (IsDepthOfFieldEnabled)
		{
			if (!IsLinked())
			{
				FLiveLinkCameraStaticData& StaticData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkCameraStaticData>();
				InitializeStaticData(StaticData);
				MayaLiveLinkStreamManager::TheOne().RebuildCameraSubjectData(SubjectName, "Camera");
			}
			else
			{
				RebuildLevelSequenceSubject(SubjectName, GetDagPath(), SavedAssetName, SavedAssetPath, UnrealAssetClass, UnrealAssetPath, ForceRelink);
			}
		}
		ValidSubject = true;
	}
	return ValidSubject;
}

void MLiveLinkCameraSubject::OnStream(double StreamTime, double CurrentTime)
{
	StreamCamera(CameraPath, StreamTime, CurrentTime);
}

void MLiveLinkCameraSubject::SetStreamType(MCameraStreamMode StreamModeIn)
{
	if (StreamModeIn != MLiveLinkBaseCameraSubject::Camera)
	{
		UnrealAssetPath.clear();
		UnrealAssetClass.clear();
		SavedAssetPath.clear();
		SavedAssetName.clear();
	}
	MLiveLinkBaseCameraSubject::SetStreamType(StreamModeIn);
}

void MLiveLinkCameraSubject::LinkUnrealAsset(const LinkAssetInfo& LinkInfo)
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
		bIsCineCamera = LinkInfo.UnrealNativeClass == "CineCameraActor";

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

void MLiveLinkCameraSubject::UnlinkUnrealAsset()
{
	bIsCineCamera = false;
	bLinked = false;
	SetStreamType(StreamMode);
	OnStreamCurrentTime();
}

bool MLiveLinkCameraSubject::IsLinked() const
{
	return bLinked &&
		   UnrealAssetPath.length() &&
		   UnrealAssetClass.length() &&
		   SavedAssetPath.length() &&
		   SavedAssetName.length();
}

void MLiveLinkCameraSubject::OnAnimCurveEdited(const MString& AnimCurveNameIn, MObject& AnimCurveObject, const MPlug& Plug, double ConversionFactor)
{
	MLiveLinkBaseCameraSubject::OnAnimCurveEdited(AnimCurveNameIn, AnimCurveObject, Plug, ConversionFactor);

	if (IsLinked() && !IsCineCamera())
	{
		MObject Object = Plug.node();
		if (!Object.hasFn(MFn::kCamera))
		{
			return;
		}

		MFnCamera Camera(Object);

		// Check if the anim curve should be redirected to another attribute equivalent on the Unreal side
		const auto RedirectedCurveIter = RedirectedCurves.find(AnimCurveNameIn.asChar());
		if (RedirectedCurveIter != RedirectedCurves.end())
		{
			auto CurveIter = AnimCurves.find(RedirectedCurveIter->first);
			if (CurveIter != AnimCurves.end())
			{
				MString PlugName = Plug.partialName(false, false, false, false, false, false);
				RedirectedCurveIter->second->BakeKeyFrameRange(CurveIter->second, Camera, PlugName, AnimCurves);

				// Replace the anim curve by the one with the name Unreal is expecting
				AnimCurves.insert({ RedirectedCurveIter->second->GetName(), std::move(CurveIter->second) });
				AnimCurves.erase(CurveIter);
			}
		}
	}
}
