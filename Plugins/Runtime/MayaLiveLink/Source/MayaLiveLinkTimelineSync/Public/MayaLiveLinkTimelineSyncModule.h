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

#include "CoreMinimal.h"

#include "Misc/QualifiedFrameTime.h"
#include "Modules/ModuleManager.h"

class FMayaLiveLinkTimelineSyncModule : public IModuleInterface
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTimeChanged, const FQualifiedFrameTime& Time);

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline FMayaLiveLinkTimelineSyncModule& GetModule()
	{
		static const FName ModuleName = "MayaLiveLinkTimelineSync";
		return FModuleManager::LoadModuleChecked<FMayaLiveLinkTimelineSyncModule>(ModuleName);
	}

	static inline bool IsModuleLoaded()
	{
		static const FName ModuleName = "MayaLiveLinkTimelineSync";
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

	MAYALIVELINKTIMELINESYNC_API void OpenAnimEditorWindow(const FString& Path, const FString& Name) const;
	MAYALIVELINKTIMELINESYNC_API bool CloseAnimEditorWindow(const FString& Path, const FString& Name) const;

	MAYALIVELINKTIMELINESYNC_API void SetCurrentTime(const FQualifiedFrameTime& Time);

	MAYALIVELINKTIMELINESYNC_API void SetLastTime();

	MAYALIVELINKTIMELINESYNC_API void EnableAnimSequenceEditorTimeSync(bool bEnable) { bAnimSequenceEditorTimeSync = bEnable; }

	MAYALIVELINKTIMELINESYNC_API FOnTimeChanged& GetOnTimeChangedDelegate() { return OnTimeChangedDelegate; }

	MAYALIVELINKTIMELINESYNC_API void AddAnimSequenceStartFrame(const FString& Name, int32 StartTime);
	MAYALIVELINKTIMELINESYNC_API void RemoveAnimSequenceStartFrame(const FString& Name);
	MAYALIVELINKTIMELINESYNC_API void RemoveAllAnimSequenceStartFrames() { AnimSequenceStartFrames.Empty(); }

private:
	// Sequencer events
	void OnSequencerCreated(TSharedRef<class ISequencer> Sequencer);
	void OnSequencerClosed(TSharedRef<class ISequencer> Sequencer);

	void UnregisterSequencer(TSharedRef<ISequencer> Sequencer);

	void OnSequencerTimeChanged(TWeakPtr<class ISequencer> InSequencer);

	// Animation editor events
	void OnAnimSequenceEditorPreviewSceneCreated(const TSharedRef<class IPersonaPreviewScene>& InPreviewScene);
	void HandleInvalidateViews();

	void SetAnimSequenceEditorTime(const FQualifiedFrameTime& Time, class UAnimPreviewInstance* PreviewAnimInstance = nullptr);
	void SetSequencerTime(const FQualifiedFrameTime& Time);

private:
	// Sequencer
	FDelegateHandle OnSequencerCreatedHandle;
	FDelegateHandle OnSequencerClosedHandle;
	FDelegateHandle OnSequencerGlobalTimeChangedHandle;
	TWeakPtr<ISequencer> WeakSequencer;
	FQualifiedFrameTime LastFrameTime;
	bool bLevelSequenceEditorTimeSync;
	bool bSetGlobalTime;

	// Animation sequence editor
	FDelegateHandle OnPreviewSceneCreatedHandle;
	TWeakPtr<IPersonaPreviewScene> WeakPreviewScene;
	bool bAnimSequenceEditorTimeSync;

	FOnTimeChanged OnTimeChangedDelegate;
	bool bIgnoreTimeChange;
	bool bBlockTimeChangeFeedback;

	TMap<FString, int32> AnimSequenceStartFrames;
};
