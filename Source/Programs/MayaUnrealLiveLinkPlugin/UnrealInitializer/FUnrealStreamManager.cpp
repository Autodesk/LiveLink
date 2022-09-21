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

#include "FUnrealStreamManager.h"

#include "FMessageBusLiveLinkProducer.h"
#include "JSONLiveLinkProducer.h"

#include "Interfaces/IPv4/IPv4Endpoint.h"

#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkAnimationTypes.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkCameraTypes.h"
#include "Roles/LiveLinkLightRole.h"
#include "Roles/LiveLinkLightTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "Roles/MayaLiveLinkAnimSequenceRole.h"
#include "Roles/MayaLiveLinkLevelSequenceRole.h"
#include "Roles/MayaLiveLinkTimelineTypes.h"

//======================================================================
/*!	\brief	Initialize and return the mutable reference to working static data that will be sent to UE.

This function creates a static struct from type T and assigns it to WorkingStaticData.
This function is called by MayaLiveLinkStreamManager to provide access to WorkingStaticData for
subjects to fill with relevant static data that needs to sent to UE.

T must be static data for LiveLink subjects.

\return	T Reference to Subject specific static data in WorkingStaticData.
*/
template<typename T>
T& FUnrealStreamManager::InitializeAndGetStaticData()
{
	// Allocate the static data struct with the given type T
	WorkingStaticData = FLiveLinkStaticDataStruct(T::StaticStruct());
	return *WorkingStaticData.Cast<T>();
}

//======================================================================
//
/*!	\brief	Explicit function template instantiation.
*/
template FLiveLinkLightStaticData&     FUnrealStreamManager::InitializeAndGetStaticData();
template FLiveLinkCameraStaticData&    FUnrealStreamManager::InitializeAndGetStaticData();
template FLiveLinkSkeletonStaticData&  FUnrealStreamManager::InitializeAndGetStaticData();
template FLiveLinkTransformStaticData& FUnrealStreamManager::InitializeAndGetStaticData();
template FMayaLiveLinkAnimSequenceStaticData& FUnrealStreamManager::InitializeAndGetStaticData();
template FMayaLiveLinkLevelSequenceStaticData& FUnrealStreamManager::InitializeAndGetStaticData();


//======================================================================
/*!	\brief	Initialize and return the mutable reference to working static data that will be sent to UE.

This function creates a frame data struct from type T and assigns it to WorkingFrameData.
This function is called by MayaLiveLinkStreamManager to provide access to WorkingFrameData for
subjects to fill with relevant frame data that needs to sent to UE.

T must be frame data for LiveLink subjects.

\return	T Reference to Subject specific frame data in WorkingFrameData.
*/
template<typename T>
T& FUnrealStreamManager::InitializeAndGetFrameData()
{
	// Allocate the frame data struct with the given type T
	WorkingFrameData = FLiveLinkFrameDataStruct(T::StaticStruct());
	return *WorkingFrameData.Cast<T>();
}

//======================================================================
//
/*!	\brief	Explicit function template instantiation.
*/
template FLiveLinkLightFrameData&     FUnrealStreamManager::InitializeAndGetFrameData();
template FLiveLinkCameraFrameData&    FUnrealStreamManager::InitializeAndGetFrameData();
template FLiveLinkAnimationFrameData& FUnrealStreamManager::InitializeAndGetFrameData();
template FLiveLinkTransformFrameData& FUnrealStreamManager::InitializeAndGetFrameData();
template FMayaLiveLinkAnimSequenceFrameData& FUnrealStreamManager::InitializeAndGetFrameData();
template FMayaLiveLinkLevelSequenceFrameData& FUnrealStreamManager::InitializeAndGetFrameData();

//======================================================================
//
/*!	\brief	Private default constructor.
*/
FUnrealStreamManager::FUnrealStreamManager()
: bUpdateWhenDisconnected(false)
{
}

//======================================================================
//
/*!	\brief	Private destructor resets the shared pointer.
*/
FUnrealStreamManager::~FUnrealStreamManager()
{
	LiveLinkProvider.Reset();
}

//======================================================================
//
/*!	\brief	Access to this singleton object.
*/
FUnrealStreamManager& FUnrealStreamManager::TheOne()
{
	static FUnrealStreamManager SFUnrealStreamManager;
	return SFUnrealStreamManager;
}

//======================================================================
/*!	\brief	Get the current live link provider.

\return	LiveLinkProvider shared pointer that is currently active.
*/
TSharedPtr<ILiveLinkProducer> FUnrealStreamManager::GetLiveLinkProvider()
{
	return LiveLinkProvider;
}

//======================================================================
/*!	\brief	Set the current live link provider.

\param[in] Producer Live link source that we want to set as new producer.

\return	True when live link provider was set successfully.
*/
bool	FUnrealStreamManager::SetLiveLinkProvider(LiveLinkSource Producer)
{
	if (LiveLinkSource::MessageBus == Producer)
	{
		LiveLinkProvider = TSharedPtr<FMessageBusLiveLinkProducer>(new FMessageBusLiveLinkProducer(TEXT("Maya Live Link MessageBus")));
		FPlatformMisc::LowLevelOutputDebugString(TEXT("Messagebus live link producer created\n"));
		return true;
	}
	else if (LiveLinkSource::JSON == Producer)
	{
		auto JSONProvider = TSharedPtr<FJSONLiveLinkProducer>(new FJSONLiveLinkProducer(TEXT("Maya Live Link JSON")));
		JSONProvider->Connect(FIPv4Endpoint(FIPv4Address(127, 0, 0, 1), 54321));
		LiveLinkProvider = JSONProvider;
		FPlatformMisc::LowLevelOutputDebugString(TEXT("JSON live link producer created\n"));
		return true;
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("Invalid LiveLink source\n"));
		return false;
	}
}

//======================================================================
/*!	\brief	Update the "Prop Subject" static data.

\param[in] SubjectName Name of the subject to be updated.
\param[in] StreamMode  Stream mode for the subject.

\return	True when subject's data was updated successfully.
*/
bool FUnrealStreamManager::RebuildPropSubjectData(const FName& SubjectName, const FString& StreamMode)
{
	if (!HasConnection())
	{
		return false;
	}

	bool ValidSubject = false;
	if (StreamMode == "RootOnly")
	{
		auto& TransformData = *WorkingStaticData.Cast<FLiveLinkTransformStaticData>();
		TransformData.bIsScaleSupported = true;

		LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkTransformRole::StaticClass(), MoveTemp(WorkingStaticData));
		ValidSubject = true;
	}
	else if (StreamMode == "FullHierarchy")
	{
		auto& AnimationData = *WorkingStaticData.Cast<FLiveLinkSkeletonStaticData>();

		AnimationData.BoneNames.Add(FName("root"));
		AnimationData.BoneParents.Add(-1);

		LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkAnimationRole::StaticClass(), MoveTemp(WorkingStaticData));
		ValidSubject = true;
	}

	return ValidSubject;
}

//======================================================================
/*!	\brief	Update the "Prop Subject" frame(Stream or animated i.e. data that changes per frame) data.

\param[in] SubjectName  Name of the subject to be updated.
\param[in] StreamMode   Stream mode for the subject.
*/
void FUnrealStreamManager::OnStreamPropSubject(const FName& SubjectName, const FString& StreamMode)
{
	if (!HasConnection())
	{
		return;
	}

	if (StreamMode == "RootOnly")
	{
		LiveLinkProvider->UpdateSubjectFrameData(SubjectName, ULiveLinkTransformRole::StaticClass(), MoveTemp(WorkingFrameData));
	}
	else if (StreamMode == "FullHierarchy")
	{
		LiveLinkProvider->UpdateSubjectFrameData(SubjectName, ULiveLinkAnimationRole::StaticClass(), MoveTemp(WorkingFrameData));
	}
}

//======================================================================
/*!	\brief	Update the "Light Subject" static data.

\param[in] SubjectName Name of the subject to be updated.
\param[in] StreamMode  Stream mode for the subject.

\return	True when subject's data was updated successfully.
*/
bool FUnrealStreamManager::RebuildLightSubjectData(const FName& SubjectName, const FString& StreamMode)
{
	if (!HasConnection())
	{
		return false;
	}

	bool ValidSubject = false;
	if (StreamMode == "RootOnly")
	{
		LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkTransformRole::StaticClass(), MoveTemp(WorkingStaticData));
		ValidSubject = true;
	}
	else if (StreamMode == "FullHierarchy")
	{
		auto& AnimationData = *WorkingStaticData.Cast<FLiveLinkSkeletonStaticData>();
		AnimationData.BoneNames.Add(FName("root"));
		AnimationData.BoneParents.Add(-1);

		LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkAnimationRole::StaticClass(), MoveTemp(WorkingStaticData));
		ValidSubject = true;
	}
	else if (StreamMode == "Light")
	{
		auto& LightData = *WorkingStaticData.Cast<FLiveLinkLightStaticData>();
		LightData.bIsIntensitySupported = true;
		LightData.bIsLightColorSupported = true;
		LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkLightRole::StaticClass(), MoveTemp(WorkingStaticData));
		ValidSubject = true;
	}
	return ValidSubject;
}

//======================================================================
/*!	\brief	Update the "Light Subject" frame(Stream or animated i.e. data that changes per frame) data.

\param[in] SubjectName  Name of the subject to be updated.
\param[in] StreamMode   Stream mode for the subject.
*/
void FUnrealStreamManager::OnStreamLightSubject(const FName& SubjectName, const FString& StreamMode)
{
	if (!HasConnection())
	{
		return;
	}

	if (StreamMode == "RootOnly")
	{
		LiveLinkProvider->UpdateSubjectFrameData(SubjectName, ULiveLinkTransformRole::StaticClass(), MoveTemp(WorkingFrameData));
	}
	else if (StreamMode == "FullHierarchy")
	{
		LiveLinkProvider->UpdateSubjectFrameData(SubjectName, ULiveLinkAnimationRole::StaticClass(), MoveTemp(WorkingFrameData));
	}
	else if (StreamMode == "Light")
	{
		LiveLinkProvider->UpdateSubjectFrameData(SubjectName, ULiveLinkLightRole::StaticClass(), MoveTemp(WorkingFrameData));
	}
}

//======================================================================
/*!	\brief	Update the "Base Camera Subject" static data.

\param[in] SubjectName Name of the subject to be updated.
\param[in] StreamMode  Stream mode for the subject.

\return	True when subject's data was updated successfully.
*/
bool FUnrealStreamManager::RebuildBaseCameraSubjectData(const FName& SubjectName, const FString& StreamMode)
{
	if (!HasConnection())
	{
		return false;
	}

	bool ValidSubject = false;
	if (StreamMode == "RootOnly")
	{
		LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkTransformRole::StaticClass(), MoveTemp(WorkingStaticData));
		ValidSubject = true;
	}
	else if (StreamMode == "FullHierarchy")
	{
		auto& AnimationData = *WorkingStaticData.Cast<FLiveLinkSkeletonStaticData>();
		AnimationData.BoneNames.Add(FName("root"));
		AnimationData.BoneParents.Add(-1);

		LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkAnimationRole::StaticClass(), MoveTemp(WorkingStaticData));
		ValidSubject = true;
	}
	else if (StreamMode == "Camera")
	{
		LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkCameraRole::StaticClass(), MoveTemp(WorkingStaticData));
		ValidSubject = true;
	}
	return ValidSubject;
}

//======================================================================
/*!	\brief	Update the "Camera Subject" frame(Stream or animated i.e. data that changes per frame) data.

\param[in] SubjectName     Name of the subject to be updated.
\param[in] StreamMode      Stream mode for the subject.
*/
void FUnrealStreamManager::StreamCamera(const FName& SubjectName, const FString& StreamMode)
{
	if (!HasConnection())
	{
		return;
	}

	if (StreamMode == "RootOnly")
	{
		LiveLinkProvider->UpdateSubjectFrameData(SubjectName, ULiveLinkTransformRole::StaticClass(), MoveTemp(WorkingFrameData));
	}
	else if (StreamMode == "FullHierarchy")
	{
		LiveLinkProvider->UpdateSubjectFrameData(SubjectName, ULiveLinkAnimationRole::StaticClass(), MoveTemp(WorkingFrameData));
	}
	else if (StreamMode == "Camera")
	{
		LiveLinkProvider->UpdateSubjectFrameData(SubjectName, ULiveLinkCameraRole::StaticClass(), MoveTemp(WorkingFrameData));
	}
}

//======================================================================
/*!	\brief	Update the "Camera Subject" static data.

\param[in] SubjectName Name of the subject to be updated.
\param[in] StreamMode  Stream mode for the subject.

\return	True when subject's data was updated successfully.
*/
bool FUnrealStreamManager::RebuildCameraSubjectData(const FName& SubjectName, const FString& StreamMode)
{
	if (!HasConnection())
	{
		return false;
	}

	auto& CameraData = *WorkingStaticData.Cast<FLiveLinkCameraStaticData>();
	CameraData.bIsApertureSupported = true;
	CameraData.bIsFocusDistanceSupported = true;

	LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkCameraRole::StaticClass(), MoveTemp(WorkingStaticData));
	return true;
}


//======================================================================
/*!	\brief	Update the "Joint Hierarchy Subject" static data.

\param[in] SubjectName  Name of the subject to be updated.
\param[in] StreamMode   Stream mode for the subject.

\return	True when subject's data was updated successfully.
*/
bool FUnrealStreamManager::RebuildJointHierarchySubjectData(const FName& SubjectName, const FString& StreamMode)
{
	if (!HasConnection())
	{
		return false;
	}

	bool ValidSubject = false;

	if (StreamMode == "RootOnly")
	{
		auto& TransformData = *WorkingStaticData.Cast<FLiveLinkTransformStaticData>();
		TransformData.bIsScaleSupported = true;

		LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkTransformRole::StaticClass(), MoveTemp(WorkingStaticData));
		ValidSubject = true;
	}
	else if (StreamMode == "FullHierarchy")
	{
		LiveLinkProvider->UpdateSubjectStaticData(SubjectName, ULiveLinkAnimationRole::StaticClass(), MoveTemp(WorkingStaticData));
		ValidSubject = true;
	}

	return ValidSubject;
}

//======================================================================
/*!	\brief	Update the "Joint Hierarchy Subject" frame(Stream or animated i.e. data that changes per frame) data.

\param[in] SubjectName     Name of the subject to be updated.
\param[in] StreamMode      Stream mode for the subject.
*/
void FUnrealStreamManager::OnStreamJointHierarchySubject(const FName& SubjectName, const FString& StreamMode)
{
	if (!HasConnection())
	{
		return;
	}

	if (StreamMode == "RootOnly")
	{
		LiveLinkProvider->UpdateSubjectFrameData(SubjectName, ULiveLinkTransformRole::StaticClass(), MoveTemp(WorkingFrameData));
	}
	else if (StreamMode == "FullHierarchy")
	{
		LiveLinkProvider->UpdateSubjectFrameData(SubjectName, ULiveLinkAnimationRole::StaticClass(), MoveTemp(WorkingFrameData));
	}
}

void FUnrealStreamManager::RebuildAnimSequence(const FName& SubjectName)
{
	if (!HasConnection())
	{
		return;
	}

	LiveLinkProvider->UpdateSubjectStaticData(SubjectName, UMayaLiveLinkAnimSequenceRole::StaticClass(), MoveTemp(WorkingStaticData));
}

void FUnrealStreamManager::OnStreamAnimSequence(const FName& SubjectName)
{
	if (!HasConnection())
	{
		return;
	}

	LiveLinkProvider->UpdateSubjectFrameData(SubjectName, UMayaLiveLinkAnimSequenceRole::StaticClass(), MoveTemp(WorkingFrameData));
}

void FUnrealStreamManager::RebuildLevelSequence(const FName& SubjectName)
{
	if (!HasConnection())
	{
		return;
	}

	LiveLinkProvider->UpdateSubjectStaticData(SubjectName, UMayaLiveLinkLevelSequenceRole::StaticClass(), MoveTemp(WorkingStaticData));
}

void FUnrealStreamManager::OnStreamLevelSequence(const FName& SubjectName)
{
	if (!HasConnection())
	{
		return;
	}

	LiveLinkProvider->UpdateSubjectFrameData(SubjectName, UMayaLiveLinkLevelSequenceRole::StaticClass(), MoveTemp(WorkingFrameData));
}

bool FUnrealStreamManager::HasConnection() const
{
	return bUpdateWhenDisconnected || (LiveLinkProvider && LiveLinkProvider->HasConnection());
}
