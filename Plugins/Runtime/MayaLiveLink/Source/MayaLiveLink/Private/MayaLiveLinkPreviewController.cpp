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

#include "MayaLiveLinkPreviewController.h"

#include "CameraController.h"
#include "IPersonaPreviewScene.h"
#include "LiveLinkClientReference.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"

const FName EditorCamera(TEXT("EditorActiveCamera"));

class FMayaLiveLinkCameraController : public FEditorCameraController
{
	FLiveLinkClientReference ClientRef;
	bool bUpdate = false;

public:
	virtual ~FMayaLiveLinkCameraController()
	{
		FMayaLiveLinkTimelineSyncModule::GetModule().EnableAnimSequenceEditorTimeSync(false);
	}

	virtual void UpdateSimulation(const FCameraControllerUserImpulseData& UserImpulseData,
								  const float DeltaTime,
								  const bool bAllowRecoilIfNoImpulse,
								  const float MovementSpeedScale,
								  FVector& InOutCameraPosition,
								  FVector& InOutCameraEuler,
								  float& InOutCameraFOV)
	{
		if (ILiveLinkClient* Client = ClientRef.GetClient())
		{
			TSubclassOf<ULiveLinkRole> SubjectRole = Client->GetSubjectRole_AnyThread(EditorCamera);
			if (SubjectRole)
			{
				FLiveLinkSubjectFrameData CurrentFrameData;
				if (Client->EvaluateFrame_AnyThread(EditorCamera, ULiveLinkTransformRole::StaticClass(), CurrentFrameData))
				{
					FLiveLinkTransformFrameData* FrameData = CurrentFrameData.FrameData.Cast<FLiveLinkTransformFrameData>();

					FTransform Camera = FrameData->Transform;
					InOutCameraPosition = Camera.GetLocation();
					InOutCameraEuler = Camera.GetRotation().Euler();

					bUpdate = true;
					return;
				}
				bUpdate = false;
			}
		}

		if (!bUpdate)
		{
			InOutCameraPosition = FVector(0.f);
			InOutCameraEuler = FVector(0.f);
		}
	}
};

void UMayaLiveLinkPreviewController::InitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const
{
	PreviewScene->ShowDefaultMode();

	if (bEnableCameraSync)
	{
		PreviewScene->SetCameraOverride(MakeShared<FMayaLiveLinkCameraController>());
	}
	auto& TimelineSyncModule = FMayaLiveLinkTimelineSyncModule::GetModule();
	TimelineSyncModule.EnableAnimSequenceEditorTimeSync(bEnableAnimSequenceEditorTimeSync);
	TimelineSyncModule.SetLastTime();
}

void UMayaLiveLinkPreviewController::UninitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const
{
	PreviewScene->SetCameraOverride(nullptr);
}
