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

#include "LiveLinkTypes.h"
#include "MayaLiveLinkTimelineTypes.generated.h"

class UAnimSequence;
class USkeleton;


struct MAYALIVELINKINTERFACE_API FMayaLiveLinkTimelineBaseParams
{
	FFrameRate FrameRate;
	int32 StartFrame;
	int32 EndFrame;
	FString SequenceName;
	FString SequencePath;
	FString LinkedAssetPath;
};

struct MAYALIVELINKINTERFACE_API FMayaLiveLinkAnimSequenceParams : public FMayaLiveLinkTimelineBaseParams
{
	TArray<FName> BoneTrackRemapping;
	TArray<FName> CurveNames;
	FString FullSequenceName;
};

USTRUCT(BlueprintType)
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkTimelineBaseStaticData : public FLiveLinkBaseStaticData
{
	GENERATED_BODY()

public:
	// Timeline parameters
	UPROPERTY(EditAnywhere, Category="TimelineParams")
	FFrameRate FrameRate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimelineParams")
	int32 StartFrame;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimelineParams")
	int32 EndFrame;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimelineParams")
	FString SequenceName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimelineParams")
	FString SequencePath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimelineParams")
	FString LinkedAssetPath;
};

/**
* Static data for AnimSequence purposes. Contains data about bones that shouldn't change every frame.
*/
USTRUCT(BlueprintType)
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkAnimSequenceStaticData : public FMayaLiveLinkTimelineBaseStaticData
{
	GENERATED_BODY()

public:

	// Set the bone names for this skeleton
	void SetBoneNames(const TArray<FName>& InBoneNames) { BoneNames = InBoneNames; }

	// Get the bone names for this skeleton
	const TArray<FName>& GetBoneNames() const { return BoneNames; }

	// Set the parent bones for this skeleton (Array of indices to parent)
	void SetBoneParents(const TArray<int32> InBoneParents) { BoneParents = InBoneParents; }

	//Get skeleton's parent bones array
	const TArray<int32>& GetBoneParents() const { return BoneParents; }

	// Find skeleton root bone, which is the bone with an invalid parent bone index.
	int32 FindRootBone() const { return BoneParents.IndexOfByPredicate([](int32 BoneParent){ return BoneParent < 0; }); }

public:

	// Names of each bone in the skeleton
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skeleton")
	TArray<FName> BoneNames;

	// Parent Indices: For each bone it specifies the index of its parent
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skeleton")
	TArray<int32> BoneParents;
};

USTRUCT()
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkAnimSequenceFrame
{
	GENERATED_BODY()

public:
	// Array of transforms for each bone of the skeleton
	UPROPERTY()
	TArray<FVector> Locations;

	UPROPERTY()
	TArray<FQuat> Rotations;

	UPROPERTY()
	TArray<FVector> Scales;

	UPROPERTY()
	TArray<float> PropertyValues;
};

struct MAYALIVELINKINTERFACE_API FMayaLiveLinkLevelSequenceParams : public FMayaLiveLinkTimelineBaseParams
{
	FGuid ActorBinding;
	FGuid TrackBinding;
};

/**
* Static data for LevelSequence purposes. Contains data about a level sequence that shouldn't change every frame.
*/
USTRUCT(BlueprintType)
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkLevelSequenceStaticData : public FMayaLiveLinkTimelineBaseStaticData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="AnimCurve")
	FString ClassName;
};

UENUM(BlueprintType)
enum ELiveLinkTangentMode
{
	/** Automatically calculates tangents to create smooth curves between values. */
	LLTM_Auto UMETA(DisplayName="Auto"),
	/** User specifies the tangent as a unified tangent where the two tangents are locked to each other, presenting a consistent curve before and after. */
	LLTM_User UMETA(DisplayName="User"),
	/** User specifies the tangent as two separate broken tangents on each side of the key which can allow a sharp change in evaluation before or after. */
	LLTM_Break UMETA(DisplayName="Break"),
	/** No tangents. */
	LLTM_None UMETA(Hidden)
};

UENUM(BlueprintType)
enum ELiveLinkTangentWeightMode
{
	/** Don't take tangent weights into account. */
	LLTWM_WeightedNone UMETA(DisplayName="None"),
	/** Only take the arrival tangent weight into account for evaluation. */
	LLTWM_WeightedArrive UMETA(DisplayName="Arrive"),
	/** Only take the leaving tangent weight into account for evaluation. */
	LLTWM_WeightedLeave UMETA(DisplayName="Leave"),
	/** Take both the arrival and leaving tangent weights into account for evaluation. */
	LLTWM_WeightedBoth UMETA(DisplayName="Both")
};

UENUM(BlueprintType)
enum ELiveLinkInterpMode
{
	/** Use linear interpolation between values. */
	LLIM_Linear UMETA(DisplayName = "Linear"),
	/** Use a constant value. Represents stepped values. */
	LLIM_Constant UMETA(DisplayName = "Constant"),
	/** Cubic interpolation. See TangentMode for different cubic interpolation options. */
	LLIM_Cubic UMETA(DisplayName = "Cubic"),
	/** No interpolation. */
	LLIM_None UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkKeyFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="KeyFrame")
	double Value;

	UPROPERTY(EditAnywhere, Category="KeyFrame")
	TEnumAsByte<ELiveLinkInterpMode> InterpMode;

	UPROPERTY(EditAnywhere, Category="KeyFrame")
	TEnumAsByte<ELiveLinkTangentMode> TangentMode;

	UPROPERTY(EditAnywhere, Category="KeyFrame")
	TEnumAsByte<ELiveLinkTangentWeightMode> TangentWeightMode;

	//MFnAnimCurve::TangentType TangentTypeIn;
	UPROPERTY(EditAnywhere, Category="KeyFrame")
	double TangentAngleIn;

	UPROPERTY(EditAnywhere, Category="KeyFrame")
	double TangentWeightIn;

	//MFnAnimCurve::TangentType TangentTypeOut;
	UPROPERTY(EditAnywhere, Category="KeyFrame")
	double TangentAngleOut;

	UPROPERTY(EditAnywhere, Category="KeyFrame")
	double TangentWeightOut;
};

USTRUCT(BlueprintType)
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkCurve
{
	GENERATED_BODY()

	// KeyFrame time
	UPROPERTY(EditAnywhere, Category="AnimCurve")
	TMap<double, FMayaLiveLinkKeyFrame> KeyFrames;
};

USTRUCT(BlueprintType)
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkAnimCurveData : public FLiveLinkBaseFrameData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AnimCurve")
	TMap<FString, FMayaLiveLinkCurve> Curves;
};

/**
* Dynamic data for LevelSequence purposes. Updates a frame inside a level sequence.
*/
USTRUCT(BlueprintType)
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkLevelSequenceFrameData : public FMayaLiveLinkAnimCurveData
{
	GENERATED_BODY()
};

/**
* Dynamic data for AnimSequence purposes. 
*/
USTRUCT()
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkAnimSequenceFrameData : public FMayaLiveLinkAnimCurveData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 StartFrame;

	UPROPERTY()
	TArray<FMayaLiveLinkAnimSequenceFrame> Frames;
};
