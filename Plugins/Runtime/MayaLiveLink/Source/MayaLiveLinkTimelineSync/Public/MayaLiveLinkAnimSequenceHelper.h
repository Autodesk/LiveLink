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

#include "MayaLiveLinkAnimSequenceHelper.generated.h"

UCLASS(HideCategories=Object)
class UMayaLiveLinkAnimSequenceHelper : public UObject
{
	GENERATED_UCLASS_BODY()

	static MAYALIVELINKTIMELINESYNC_API void PushStaticDataToAnimSequence(const struct FMayaLiveLinkAnimSequenceStaticData& StaticData,
																		  TArray<FName>& BoneTrackRemapping,
																		  FString& AnimSequenceName);
	static MAYALIVELINKTIMELINESYNC_API void PushFrameDataToAnimSequence(const struct FMayaLiveLinkAnimSequenceFrameData& FrameData,
																		 const struct FMayaLiveLinkAnimSequenceParams& TimelineParams);

private:
	static bool StaticUpdateAnimSequence(class UAnimSequence& AnimSequence,
										 USkeleton* Skeleton,
										 float SequenceLength,
										 int32 NumberOfFrames,
										 const FFrameRate& FrameRate);

	static float ComputeAnimSequenceLength(int32 InNumberOfFrames, double InFrameRate)
	{
		// To calculate the frame rate, Unreal uses this formula:
		return (InNumberOfFrames > 0 && InFrameRate > 0.0) ? static_cast<float>((InNumberOfFrames - 1) / InFrameRate) : 0.0f;
	}
};
