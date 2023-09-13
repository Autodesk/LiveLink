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

#include "MayaLiveLinkAnimSequenceHelper.h"

#include "MayaLiveLinkUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceHelpers.h"
#include "Animation/Skeleton.h"

#include "AssetRegistry/AssetRegistryModule.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#include "Roles/MayaLiveLinkTimelineTypes.h"

#include "AssetToolsModule.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "MayaLiveLinkAnimSequenceHelper"

namespace
{
	float GetAnimSequenceLength(const UAnimSequence& AnimSequence)
	{
		return AnimSequence.GetPlayLength();
	}

	int32 GetAnimSequenceNumberOfFrames(const UAnimSequence& AnimSequence)
	{
		return AnimSequence.GetDataModel()->GetNumberOfKeys();
	}
}

UMayaLiveLinkAnimSequenceHelper::UMayaLiveLinkAnimSequenceHelper(const FObjectInitializer& Initializer)
{
}

void UMayaLiveLinkAnimSequenceHelper::PushStaticDataToAnimSequence(const FMayaLiveLinkAnimSequenceStaticData& StaticData,
																   TArray<FName>& BoneTrackRemapping,
																   FString& AnimSequenceName)
{
	if (StaticData.LinkedAssetPath.IsEmpty() ||
		StaticData.SequencePath.IsEmpty() ||
		StaticData.SequenceName.IsEmpty() ||
		StaticData.BoneNames.Num() != StaticData.BoneParents.Num())
	{
		return;
	}

	// Find the skeleton asset
	auto Skeleton = FMayaLiveLinkUtils::FindAsset<USkeleton>(StaticData.LinkedAssetPath);
	if (!Skeleton)
	{
		// Sometimes, the Skeleton can't be found, so look deeper into the asset registry (slower)
		FString LinkedPath, LinkedName, LinkedExtension;
		FPaths::Split(StaticData.LinkedAssetPath, LinkedPath, LinkedName, LinkedExtension);
		Skeleton = FMayaLiveLinkUtils::FindAssetInRegistry<USkeleton>(LinkedPath, LinkedName);

		if (!Skeleton)
		{
			UE_LOG(LogMayaLiveLink, Warning, TEXT("Could not find Skeleton %s"), *StaticData.LinkedAssetPath);
			return;
		}
	}

	// Find the AnimSequence asset if it exists, otherwise create it
	auto AnimSequence = FMayaLiveLinkUtils::FindAsset<UAnimSequence>(FPaths::Combine(StaticData.SequencePath,
																					 StaticData.SequenceName),
																	 StaticData.SequenceName);
	if (!AnimSequence)
	{
		// Sometimes, the AnimSequence can't be found, so look deeper into the asset registry (slower)
 		AnimSequence = FMayaLiveLinkUtils::FindAssetInRegistry<UAnimSequence>(StaticData.SequencePath,
																			  StaticData.SequenceName);
	}
	if (!AnimSequence)
	{
		UE_LOG(LogMayaLiveLink, Log, TEXT("AnimSequence %s not found, creating it."), *FPaths::Combine(StaticData.SequencePath, StaticData.SequenceName));
		if (FAssetToolsModule::IsModuleLoaded())
		{
			// Use the asset tools to create the AnimSequence
			auto& AssetTools = FAssetToolsModule::GetModule().Get();
			AnimSequence = Cast<UAnimSequence>(AssetTools.CreateAsset(StaticData.SequenceName, StaticData.SequencePath, UAnimSequence::StaticClass(), nullptr));
		}
		
		if (!AnimSequence)
		{
			UE_LOG(LogMayaLiveLink, Warning,
				   TEXT("Could not find or create AnimSequence %s located at %s"),
				   *StaticData.SequenceName,
				   *StaticData.SequencePath);
			return;
		}
	}
	else
	{
		UE_LOG(LogMayaLiveLink, Log, TEXT("AnimSequence %s already found, updating it."), *FPaths::Combine(StaticData.SequencePath, StaticData.SequenceName));
	}

	if (AnimSequence)
	{
		UE::Anim::Compression::FScopedCompressionGuard CompressionGuard(AnimSequence);

		// Setup the AnimSequence frame count and frame rate
		auto NumberOfFrames = StaticData.EndFrame - StaticData.StartFrame + 1;
		StaticUpdateAnimSequence(*AnimSequence,
								 Skeleton,
								 ComputeAnimSequenceLength(NumberOfFrames,
														   StaticData.FrameRate.AsDecimal()),
								 NumberOfFrames,
								 StaticData.FrameRate);

		// Create a bone remapping array to match the skeleton structure received from Maya to the one in Unreal Editor
		BoneTrackRemapping.Empty();
		BoneTrackRemapping.Reserve(StaticData.BoneNames.Num());
		auto& RefSkeleton = Skeleton->GetReferenceSkeleton();

		auto& Controller = AnimSequence->GetController();
		Controller.OpenBracket(LOCTEXT("AddNewRawTrack_Bracket", "Adding new Bone Animation Track"), false);
		{
			for (auto& BoneName : StaticData.BoneNames)
			{
				if (RefSkeleton.FindBoneIndex(BoneName) != INDEX_NONE)
				{
					bool bValidBone = AnimSequence->GetDataModel()->IsValidBoneTrackName(BoneName);

					if (!bValidBone)
					{
						FRawAnimSequenceTrack RawTrack;
						RawTrack.PosKeys.Init(FVector3f::ZeroVector, NumberOfFrames);
						RawTrack.RotKeys.Init(FQuat4f::Identity, NumberOfFrames);
						RawTrack.ScaleKeys.Init(FVector3f::OneVector, NumberOfFrames);

						bValidBone = Controller.AddBoneCurve(BoneName, false);
						if (bValidBone)
						{
							Controller.SetBoneTrackKeys(BoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys, false);
						}
						else
						{
							continue;
						}
					}

					FRawAnimSequenceTrack RawTrack;
					bool ResizeSequenceRequested = false;
					if (NumberOfFrames != RawTrack.PosKeys.Num())
					{
						ResizeSequenceRequested = true;
						RawTrack.PosKeys.Init(FVector3f::ZeroVector, NumberOfFrames);
					}
					if (NumberOfFrames != RawTrack.RotKeys.Num())
					{
						ResizeSequenceRequested = true;
						RawTrack.RotKeys.Init(FQuat4f::Identity, NumberOfFrames);
					}
					if (NumberOfFrames != RawTrack.ScaleKeys.Num())
					{
						ResizeSequenceRequested = true;
						RawTrack.ScaleKeys.Init(FVector3f::OneVector, NumberOfFrames);
					}

					if (ResizeSequenceRequested)
					{
						Controller.SetBoneTrackKeys(BoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys, false);
					}
				}
				BoneTrackRemapping.Add(BoneName);
			}
		}
		Controller.CloseBracket(false);
	}

	AnimSequenceName = AnimSequence->GetName();
}

void UMayaLiveLinkAnimSequenceHelper::PushFrameDataToAnimSequence(const FMayaLiveLinkAnimSequenceFrameData& FrameData,
																  const FMayaLiveLinkAnimSequenceParams& TimelineParams)
{
	if (TimelineParams.SequencePath.IsEmpty() ||
		TimelineParams.SequenceName.IsEmpty() ||
		(FrameData.Frames.IsEmpty() &&
		 FrameData.Curves.IsEmpty()))
	{
		return;
	}

	// Find the AnimSequence
	auto AnimSequence = FMayaLiveLinkUtils::FindAsset<UAnimSequence>(FPaths::Combine(TimelineParams.SequencePath,
																					 TimelineParams.SequenceName),
																	 TimelineParams.SequenceName);
	if (!AnimSequence)
	{
		UE_LOG(LogMayaLiveLink, Warning,
			   TEXT("Could not find or create AnimSequence %s located at %s"),
			   *TimelineParams.SequenceName,
			   *TimelineParams.SequencePath);
		return;

	}

	// Update the baked animation frame for each bone
	bool SequenceUpdated = false;
	TMap<FName, FMayaLiveLinkAnimSequenceFrame> FramesByBone;
	const int32 NumberOfFrames = GetAnimSequenceNumberOfFrames(*AnimSequence);
	for (int FrameIndex = 0; FrameIndex < FrameData.Frames.Num(); ++FrameIndex)
	{
		auto& Frame = FrameData.Frames[FrameIndex];

		auto BoneArraySize = Frame.Locations.Num();
		for (int32 BoneIndex = 0; BoneIndex < BoneArraySize; ++BoneIndex)
		{
			if (BoneIndex >= TimelineParams.BoneTrackRemapping.Num())
			{
				continue;
			}
			const FName& TrackName = TimelineParams.BoneTrackRemapping[BoneIndex];
			if (!TrackName.IsValid())
			{
				continue;
			}

			if (NumberOfFrames > 0 && FrameIndex < NumberOfFrames)
			{
				auto BoneTrackPtr = FramesByBone.Find(TrackName);
				if (!BoneTrackPtr)
				{
					BoneTrackPtr = &FramesByBone.Emplace(TrackName);
					BoneTrackPtr->Locations.Init(FVector::ZeroVector, FrameData.Frames.Num());
					BoneTrackPtr->Rotations.Init(FQuat::Identity, FrameData.Frames.Num());
					BoneTrackPtr->Scales.Init(FVector::OneVector, FrameData.Frames.Num());
				}

				FMayaLiveLinkAnimSequenceFrame& BoneTrack = *BoneTrackPtr;
				BoneTrack.Locations[FrameIndex] = Frame.Locations[BoneIndex];
				BoneTrack.Rotations[FrameIndex] = Frame.Rotations[BoneIndex];
				BoneTrack.Scales[FrameIndex] = Frame.Scales[BoneIndex];
				SequenceUpdated = true;
			}
		}
	}

	if (SequenceUpdated)
	{
		UE::Anim::Compression::FScopedCompressionGuard CompressionGuard(AnimSequence);

		auto& Controller = AnimSequence->GetController();
		Controller.OpenBracket(LOCTEXT("SetBoneTrackKeys_Bracket", "Setting Bone Animation Tracks"), false);
		{
			for (auto& FramePair : FramesByBone)
			{
				FName& BoneName = FramePair.Key;
				FMayaLiveLinkAnimSequenceFrame& BoneData = FramePair.Value;
				Controller.UpdateBoneTrackKeys(BoneName,
											   FInt32Range(FInt32Range::BoundsType::Inclusive(FrameData.StartFrame), FInt32Range::BoundsType::Inclusive(FrameData.StartFrame + BoneData.Locations.Num() - 1)),
											   BoneData.Locations,
											   BoneData.Rotations,
											   BoneData.Scales,
											   false);
			}
		}
		Controller.CloseBracket(false);
	}

	// Update animation curves (blendshape/morph target and custom attributes)
	if (FrameData.Curves.Num() > 0)
	{
		UE::Anim::Compression::FScopedCompressionGuard CompressionGuard(AnimSequence);

		FName ContainerName("AnimationCurves");

		auto& Controller = AnimSequence->GetController();
		const FFrameRate& FrameRate = AnimSequence->GetDataModel()->GetFrameRate();
		const double Interval = FrameRate.AsInterval();

		for (auto& CurvePair : FrameData.Curves)
		{
			const FString& CurveName = CurvePair.Key;
			const FMayaLiveLinkCurve& Curve = CurvePair.Value;

			FSmartName SmartName;
			FName CurveFName(*CurveName);
			AnimSequence->GetSkeleton()->GetSmartNameByName(ContainerName, CurveFName, SmartName);

			if (SmartName.IsValid())
			{
				FAnimationCurveIdentifier CurveId(SmartName, ERawCurveTrackTypes::RCT_Float);
				const FAnimCurveBase* RichCurve = Controller.GetModel()->FindCurve(CurveId);
				if (!RichCurve)
				{
					Controller.AddCurve(CurveId, EAnimAssetCurveFlags::AACF_Editable, false);
				}

				TArray<FRichCurveKey> RichCurves;

				for (const auto& KeyPair : Curve.KeyFrames)
				{
					const auto& Value = KeyPair.Value;
					FRichCurveKey CurveKey;
					CurveKey.Time = KeyPair.Key * Interval;
					CurveKey.Value = Value.Value;
					CurveKey.ArriveTangent = FMath::RadiansToDegrees(Value.TangentAngleIn) * 0.5f;
					CurveKey.ArriveTangentWeight = Value.TangentWeightIn;
					CurveKey.LeaveTangent = FMath::RadiansToDegrees(Value.TangentAngleOut) * 0.5f;
					CurveKey.LeaveTangentWeight = Value.TangentWeightOut;
					CurveKey.InterpMode = static_cast<ERichCurveInterpMode>(Value.InterpMode.GetValue());
					CurveKey.TangentMode = static_cast<ERichCurveTangentMode>(Value.TangentMode.GetValue());
					CurveKey.TangentWeightMode = static_cast<ERichCurveTangentWeightMode>(Value.TangentWeightMode.GetValue());
					RichCurves.Emplace(MoveTemp(CurveKey));
				}

				Controller.SetCurveKeys(CurveId, RichCurves, false);
			}
		}
	}

	FMayaLiveLinkUtils::RefreshContentBrowser(*AnimSequence);
}

bool UMayaLiveLinkAnimSequenceHelper::StaticUpdateAnimSequence(UAnimSequence& AnimSequence,
															   USkeleton* Skeleton,
															   float SequenceLength,
															   int32 NumberOfFrames,
															   const FFrameRate& FrameRate)
{
	bool Updated = false;

	// Update the AnimSequence's skeleton
	if (Skeleton &&
		AnimSequence.GetSkeleton() != Skeleton)
	{
		AnimSequence.SetSkeleton(Skeleton);
		Updated = true;

		AnimSequence.GetController().InitializeModel();
	}

	// Resize the AnimSequence length
	const float AnimSequenceLength = GetAnimSequenceLength(AnimSequence);
	const int32 AnimSequenceNumberOfFrames = GetAnimSequenceNumberOfFrames(AnimSequence);
	if ((SequenceLength > 0.0f &&
		 !FMath::IsNearlyEqual(AnimSequenceLength, SequenceLength, KINDA_SMALL_NUMBER)) ||
		(NumberOfFrames > 0 &&
		 AnimSequenceNumberOfFrames != NumberOfFrames))
	{
		auto& Controller = AnimSequence.GetController();
		Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float);
		Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Transform);
		Controller.SetFrameRate(FrameRate);
		Controller.SetNumberOfFrames(NumberOfFrames);
		AnimSequence.ImportResampleFramerate = FrameRate.AsDecimal();
		AnimSequence.ImportFileFramerate = FrameRate.AsDecimal();

		// Trigger a notification to update the target/sampling frame rate used to playback the anim sequence
		Controller.NotifyPopulated();

		Updated = true;
	}

	return Updated;
}

#undef LOCTEXT_NAMESPACE
