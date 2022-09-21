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
#include "UObject/ObjectMacros.h"
#include "MayaLiveLinkLevelSequenceHelper.generated.h"

UCLASS(HideCategories=Object)
class UMayaLiveLinkLevelSequenceHelper : public UObject
{
	GENERATED_UCLASS_BODY()

	static MAYALIVELINKTIMELINESYNC_API void PushStaticDataToLevelSequence(const struct FMayaLiveLinkLevelSequenceStaticData& StaticData,
																		   FGuid& ActorBinding,
																		   FGuid& TrackBinding);
	static MAYALIVELINKTIMELINESYNC_API void PushFrameDataToLevelSequence(const struct FMayaLiveLinkLevelSequenceFrameData& FrameData,
																		  const struct FMayaLiveLinkLevelSequenceParams& LevelSequenceParams);

private:
	template<typename T>
	static T* AddOrFindTrack(const FGuid& TrackBinding,
							 const FString& PropertyName,
							 class UMovieScene& MovieScene,
							 const TCHAR* Path = nullptr);

	static UWorld* FindWorld();

	static void ResizeTracks(UMovieScene& MovieScene,
							 const FGuid& ActorBinding,
							 const FGuid& TrackBinding);
};
