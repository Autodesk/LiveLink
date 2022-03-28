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

#include "MLiveLinkBaseCameraSubject.h"
#include "../MayaLiveLinkStreamManager.h"
#include "IMStreamedEntity.h"
#include "MStreamedEntity.h"
#include "../MayaCommonIncludes.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkCameraTypes.h"

#include "../MayaUnrealLiveLinkUtils.h"

MString CameraStreams[3] = { "Root Only", "Full Hierarchy", "Camera" };
MStringArray MLiveLinkBaseCameraSubject::CameraStreamOptions (CameraStreams, 3);

MLiveLinkBaseCameraSubject::MLiveLinkBaseCameraSubject(const MString& InSubjectName, MCameraStreamMode InStreamMode, const MDagPath& InRootPath)
	: MStreamedEntity(InRootPath)
	, SubjectName(InSubjectName)
	, StreamMode(InStreamMode >= 0 && InStreamMode < CameraStreamOptions.length() ? InStreamMode : MCameraStreamMode::Camera)
{}

MLiveLinkBaseCameraSubject::~MLiveLinkBaseCameraSubject()
{
	MayaLiveLinkStreamManager::TheOne().RemoveSubjectFromLiveLink(SubjectName);
}

MString MLiveLinkBaseCameraSubject::GetNameDisplayText() const
{ 
	return SubjectName;
}

MString MLiveLinkBaseCameraSubject::GetRoleDisplayText() const
{
	return CameraStreamOptions[StreamMode];
}

MString MLiveLinkBaseCameraSubject::GetSubjectTypeDisplayText() const
{
	return MString("Camera");
}

bool MLiveLinkBaseCameraSubject::ValidateSubject() const
{
	return true;
}

bool MLiveLinkBaseCameraSubject::RebuildSubjectData()
{
	bool ValidSubject = false;
	if (StreamMode == MCameraStreamMode::RootOnly)
	{
		MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkTransformStaticData>();
		return MayaLiveLinkStreamManager::TheOne().RebuildBaseCameraSubjectData(SubjectName, "RootOnly");
	}
	else if (StreamMode == MCameraStreamMode::FullHierarchy)
	{
		MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkSkeletonStaticData>();
		return MayaLiveLinkStreamManager::TheOne().RebuildBaseCameraSubjectData(SubjectName, "FullHierarchy");
	}
	else if (StreamMode == MCameraStreamMode::Camera)
	{
		MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkCameraStaticData>();
		return MayaLiveLinkStreamManager::TheOne().RebuildBaseCameraSubjectData(SubjectName, "Camera");
	}
	return ValidSubject;
}

void MLiveLinkBaseCameraSubject::StreamCamera(const MDagPath& CameraPath, double StreamTime, double CurrentTime)
{
	MStatus Status;
	bool bIsValid = CameraPath.isValid(&Status);

	if (bIsValid && Status == MStatus::kSuccess)
	{
		MFnCamera C(CameraPath);

		MPoint EyeLocation = C.eyePoint(MSpace::kWorld);
		const auto SceneTime = MayaUnrealLiveLinkUtils::GetMayaFrameTimeAsUnrealTime();

		MMatrix CameraTransformMatrix;
		MayaUnrealLiveLinkUtils::SetMatrixRow(CameraTransformMatrix[0], C.rightDirection(MSpace::kWorld));
		MayaUnrealLiveLinkUtils::SetMatrixRow(CameraTransformMatrix[1], C.viewDirection(MSpace::kWorld));
		MayaUnrealLiveLinkUtils::SetMatrixRow(CameraTransformMatrix[2], C.upDirection(MSpace::kWorld));
		MayaUnrealLiveLinkUtils::SetMatrixRow(CameraTransformMatrix[3], EyeLocation);

		MayaUnrealLiveLinkUtils::RotateCoordinateSystemForUnreal(CameraTransformMatrix);
		FTransform CameraTransform = MayaUnrealLiveLinkUtils::BuildUETransformFromMayaTransform(CameraTransformMatrix);
		// Convert Maya Camera orientation to Unreal
		CameraTransform.SetRotation(CameraTransform.GetRotation() * FRotator(0.0f, -90.0f, 0.0f).Quaternion());

		if (StreamMode == MCameraStreamMode::RootOnly)
		{
			auto& CameraTransformData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkTransformFrameData>();
			CameraTransformData.Transform = CameraTransform;
			CameraTransformData.WorldTime = StreamTime;
			CameraTransformData.MetaData.SceneTime = SceneTime;

			MayaLiveLinkStreamManager::TheOne().StreamCamera(SubjectName, "RootOnly");
		}
		else if (StreamMode == MCameraStreamMode::FullHierarchy)
		{
			auto& AnimationData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkAnimationFrameData>();
			AnimationData.Transforms.Add(CameraTransform);
			AnimationData.WorldTime = StreamTime;
			AnimationData.MetaData.SceneTime = SceneTime;

			MayaLiveLinkStreamManager::TheOne().StreamCamera(SubjectName, "FullHierarchy");
		}
		else if (StreamMode == MCameraStreamMode::Camera)
		{
			auto& CameraData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkCameraFrameData>();;
			CameraData.Transform = CameraTransform;
			CameraData.WorldTime = StreamTime;
			CameraData.MetaData.SceneTime = SceneTime;
			CameraData.Aperture = C.fStop();
			CameraData.AspectRatio = C.aspectRatio();
			CameraData.FieldOfView = MayaUnrealLiveLinkUtils::RadToDeg(C.horizontalFieldOfView());
			CameraData.FocalLength = C.focalLength();
			CameraData.FocusDistance = C.focusDistance();
			CameraData.ProjectionMode = C.isOrtho() ? ELiveLinkCameraProjectionMode::Orthographic : ELiveLinkCameraProjectionMode::Perspective;

			MayaLiveLinkStreamManager::TheOne().StreamCamera(SubjectName, "Camera");
		}
	}
}

void MLiveLinkBaseCameraSubject::SetStreamType(const MString& StreamTypeIn)
{
	for (uint32_t StreamTypeIdx = 0; StreamTypeIdx < CameraStreamOptions.length(); ++StreamTypeIdx)
	{
		if (CameraStreamOptions[StreamTypeIdx] == StreamTypeIn && StreamMode != (MCameraStreamMode)StreamTypeIdx)
		{
			StreamMode = (MCameraStreamMode)StreamTypeIdx;
			RebuildSubjectData();
			return;
		}
	}
}

int MLiveLinkBaseCameraSubject::GetStreamType() const
{
	return StreamMode;
}

void MLiveLinkBaseCameraSubject::OnAttributeChanged(const MObject& Object, const MPlug& Plug, const MPlug& OtherPlug)
{
	if (Object.hasFn(MFn::kCamera))
	{
		MStatus Status;
		MString PlugName = Plug.partialName(false, false, false, false, false, false, &Status);
		if (Status && PlugName == "dof")
		{
			RebuildSubjectData();
		}
	}
}
