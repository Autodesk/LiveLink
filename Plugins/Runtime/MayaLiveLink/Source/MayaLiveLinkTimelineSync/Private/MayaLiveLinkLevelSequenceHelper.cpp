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

#include "MayaLiveLinkLevelSequenceHelper.h"

#include "AssetToolsModule.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "MayaLiveLinkUtils.h"
#include "MovieSceneFolder.h"
#include "PropertyPath.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "Channels/MovieSceneChannelProxy.h"

#include "Components/LightComponent.h"

#include "Engine/DirectionalLight.h"

#include "GameFramework/Actor.h"

// Sections
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneColorSection.h"
#include "Sections/MovieSceneFloatSection.h"

// Trackes
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneVisibilityTrack.h"

#define LOCTEXT_NAMESPACE "MayaLiveLinkLevelSequenceHelper"

UMayaLiveLinkLevelSequenceHelper::UMayaLiveLinkLevelSequenceHelper(const FObjectInitializer& Initializer)
{
}

// Mapping transform attribute names to their level sequence track equivalent
static const TMap<FString, TTuple<EMovieSceneTransformChannel, uint32>> TransformChannelMapping =
{
	{ "LocationX", TTuple<EMovieSceneTransformChannel, uint32>(EMovieSceneTransformChannel::TranslationX, 0u) },
 	{ "LocationY", TTuple<EMovieSceneTransformChannel, uint32>(EMovieSceneTransformChannel::TranslationY, 1u) },
	{ "LocationZ", TTuple<EMovieSceneTransformChannel, uint32>(EMovieSceneTransformChannel::TranslationZ, 2u) },
	{ "RotationX", TTuple<EMovieSceneTransformChannel, uint32>(EMovieSceneTransformChannel::RotationX, 3u) },
	{ "RotationY", TTuple<EMovieSceneTransformChannel, uint32>(EMovieSceneTransformChannel::RotationY, 4u) },
	{ "RotationZ", TTuple<EMovieSceneTransformChannel, uint32>(EMovieSceneTransformChannel::RotationZ, 5u) },
	{ "ScaleX", TTuple<EMovieSceneTransformChannel, uint32>(EMovieSceneTransformChannel::ScaleX, 6u) },
	{ "ScaleY", TTuple<EMovieSceneTransformChannel, uint32>(EMovieSceneTransformChannel::ScaleY, 7u) },
	{ "ScaleZ", TTuple<EMovieSceneTransformChannel, uint32>(EMovieSceneTransformChannel::ScaleZ, 8u) },
};

// Mapping color attribute names to their level sequence track equivalent
static const TMap<FString, int32> ColorSectionMapping =
{
	{ "LightColorR", 0 },
	{ "LightColorG", 1 },
	{ "LightColorB", 2 },
	{ "LightColorA", 3 },
};

// Function to set a value to a channel
template<typename T = FMovieSceneFloatChannel, typename S = FMovieSceneFloatValue>
void SetChannel(T& Channel, const TMap<double, FMayaLiveLinkKeyFrame>& KeyFrames)
{
	// Clear the keys from the anim curve
	Channel.Reset();

	auto Data = Channel.GetData();
	for (auto& KeyFramePair : KeyFrames)
	{
		auto& KeyFrame = KeyFramePair.Value;

		// Initialize the curve with the tangent information and set the value for the keyframe
		FMovieSceneTangentData TangentData;
		S Value(KeyFrame.Value);
		Value.InterpMode = static_cast<ERichCurveInterpMode>(KeyFrame.InterpMode.GetValue());
		Value.Tangent.ArriveTangent = KeyFrame.TangentAngleIn;
		Value.Tangent.ArriveTangentWeight = KeyFrame.TangentWeightIn;
		Value.Tangent.LeaveTangent = KeyFrame.TangentAngleOut;
		Value.Tangent.LeaveTangentWeight = KeyFrame.TangentWeightOut;
		Value.Tangent.TangentWeightMode = static_cast<ERichCurveTangentWeightMode>(KeyFrame.TangentWeightMode.GetValue());
		Value.TangentMode = static_cast<ERichCurveTangentMode>(KeyFrame.TangentMode.GetValue());
		Data.UpdateOrAddKey(static_cast<int32>(KeyFramePair.Key), Value);
	}
}

void UMayaLiveLinkLevelSequenceHelper::PushStaticDataToLevelSequence(const FMayaLiveLinkLevelSequenceStaticData& StaticData,
																	 FGuid& ActorBinding,
																	 FGuid& TrackBinding)
{
	// Sanity checks
	if (StaticData.LinkedAssetPath.IsEmpty() ||
		StaticData.SequencePath.IsEmpty() ||
		StaticData.SequenceName.IsEmpty() ||
		StaticData.ClassName.IsEmpty())
	{
		return;
	}

	// Get the world level
	auto World = FindWorld();
	if (!World)
	{
		return;
	}

	// Get the class
	UClass* ActorClass = nullptr;
	int32 PathIndex = INDEX_NONE;
	if (StaticData.ClassName.FindChar(TEXT('/'), PathIndex))
	{
		// The class name is a path, so it references a blueprint class
		ActorClass = UEditorAssetLibrary::LoadBlueprintClass(StaticData.ClassName);
	}
	else
	{
		ActorClass = FMayaLiveLinkUtils::FindObject<UClass>(StaticData.ClassName);
	}
	if (!ActorClass)
	{
		UE_LOG(LogMayaLiveLink, Warning, TEXT("Could not find class %s"), *StaticData.ClassName);
		return;
	}

	// Find the actor in the level
	AActor* LinkedObject = nullptr;
	int32 LinkAssetNameIndex = -1;
	StaticData.LinkedAssetPath.FindLastChar(TEXT('/'), LinkAssetNameIndex);
	FString LinkAssetName = LinkAssetNameIndex >= 0 ? StaticData.LinkedAssetPath.Mid(LinkAssetNameIndex+1) : StaticData.LinkedAssetPath;
	FString LinkAssetFolder = LinkAssetNameIndex >= 0 ? StaticData.LinkedAssetPath.Mid(0, LinkAssetNameIndex) : "";
	TActorIterator<AActor> It(World, ActorClass);
	for (; It; ++It)
	{
		auto Actor = *It;
		if (Actor->GetActorLabel() == LinkAssetName &&
			(Actor->GetFolderPath() == NAME_None || Actor->GetFolderPath().ToString() == LinkAssetFolder))
		{
			LinkedObject = Actor;
			break;
		}
	}
	if (!LinkedObject)
	{
		// Spawn an actor of the specified class
		LinkedObject = World->SpawnActor(ActorClass);
		if (!LinkedObject)
		{
			UE_LOG(LogMayaLiveLink, Warning, TEXT("Could not spawn object %s of class %s"), *StaticData.LinkedAssetPath, *ActorClass->GetName());
			return;
		}
		else
		{
			LinkedObject->SetActorLabel(LinkAssetName);
			if (LinkAssetFolder.Len() > 0)
			{
				LinkedObject->SetFolderPath(*LinkAssetFolder);
			}

			TInlineComponentArray<USceneComponent*> SceneComponents(LinkedObject);
			for (USceneComponent* SceneComponent : SceneComponents)
			{
				// Make the mobility is set to movable to avoid warnings
				SceneComponent->SetMobility(EComponentMobility::Movable);
			}
		}
	}

	ActorBinding.Invalidate();
	TrackBinding.Invalidate();

	// Find the level sequence if it exists, otherwise create it
	auto LevelSequence = FMayaLiveLinkUtils::FindAsset<ULevelSequence>(
								FPaths::Combine(StaticData.SequencePath, StaticData.SequenceName),
								StaticData.SequenceName);
	if (!LevelSequence)
	{
		// Sometimes, the level sequence can't be found, so look deeper into the asset registry (slower)
		LevelSequence = FMayaLiveLinkUtils::FindAssetInRegistry<ULevelSequence>(
								StaticData.SequencePath,
								StaticData.SequenceName);
	}

	// Create the level sequence if it doesn't exist
	UMovieScene* MovieScenePtr = nullptr;
	if (!LevelSequence)
	{
		UE_LOG(LogMayaLiveLink, Log, TEXT("LevelSequence %s not found, creating it."), *FPaths::Combine(StaticData.SequencePath, StaticData.SequenceName));
		if (FAssetToolsModule::IsModuleLoaded())
		{
			// Create a level sequence and its associated movie scene
			auto& AssetTools = FAssetToolsModule::GetModule().Get();
			LevelSequence = Cast<ULevelSequence>(AssetTools.CreateAsset(StaticData.SequenceName, StaticData.SequencePath, ULevelSequence::StaticClass(), nullptr));
			if (LevelSequence)
			{
				MovieScenePtr = NewObject<UMovieScene>(LevelSequence, NAME_None, RF_Transactional);
				if (MovieScenePtr)
				{
					// Initialize the movie scene with requested frame rate
					LevelSequence->MovieScene = MovieScenePtr;

					MovieScenePtr->SetEvaluationType(EMovieSceneEvaluationType::WithSubFrames);
					MovieScenePtr->SetTickResolutionDirectly(StaticData.FrameRate);
					MovieScenePtr->SetDisplayRate(StaticData.FrameRate);
					MovieScenePtr->SetClockSource(EUpdateClockSource::Tick);
				}
			}
		}

		if (!LevelSequence)
		{
			UE_LOG(LogMayaLiveLink, Warning,
				   TEXT("Could not find or create LevelSequence %s located at %s"),
				   *StaticData.SequenceName,
				   *StaticData.SequencePath);
			return;
		}
	}
	else
	{
		MovieScenePtr = LevelSequence->MovieScene;
		MovieScenePtr->SetTickResolutionDirectly(StaticData.FrameRate);
		MovieScenePtr->SetDisplayRate(StaticData.FrameRate);
		UE_LOG(LogMayaLiveLink, Log, TEXT("LevelSequence %s already found, updating it."), *FPaths::Combine(StaticData.SequencePath, StaticData.SequenceName));
	}

	if (MovieScenePtr)
	{
		UMovieScene& MovieScene = *MovieScenePtr;

 		auto NumberOfFrames = StaticData.EndFrame - StaticData.StartFrame + 1;
		MovieScene.SetPlaybackRange(StaticData.StartFrame, NumberOfFrames);

		if (LinkedObject)
		{
			// Find the binding for the linked object
			int32 PossessableCount = MovieScene.GetPossessableCount();
			for (int32 Index = 0; Index < PossessableCount; ++Index)
			{
				auto& Possessable = MovieScene.GetPossessable(Index);
				if (Possessable.GetName() == LinkedObject->GetActorLabel())
				{
					ActorBinding = Possessable.GetGuid();
				}
				else
				{
					// Curves are set on the linked objects' components
					// Check if the current possessable is a component (child)
					auto ParentGuid = Possessable.GetParent();
					if (ParentGuid.IsValid())
					{
						// Get the parent
						if (auto ParentPossessable = MovieScene.FindPossessable(ParentGuid))
						{
							// If the parent's name is the same as the linked object actor label,
							// we found the binding
							if (ParentPossessable->GetName() == LinkedObject->GetActorLabel())
							{
								TrackBinding = Possessable.GetGuid();
							}
						}
					}
				}

				if (ActorBinding.IsValid() && TrackBinding.IsValid())
				{
					break;
				}
			}

			const bool bResizeTracks = ActorBinding.IsValid();
			if (!ActorBinding.IsValid())
			{
				// No binding found, so bind the actor to this movie scene to be able to add tracks to it
				ActorBinding = MovieScene.AddPossessable(LinkedObject->GetActorLabel(), ActorClass);
				LevelSequence->BindPossessableObject(ActorBinding, *LinkedObject, World);
			}

			auto BindActorComponentToSequence = [LevelSequence, &MovieScene, &ActorBinding, LinkedObject](UActorComponent* ActorComponent)
			{
				// Bind its actor component to the movie scene to see it in the sequencer
				auto TrackBinding = MovieScene.AddPossessable(ActorComponent->GetName(), ActorComponent->GetClass());

				if (TrackBinding.IsValid())
				{
					FMovieScenePossessable* ChildPossessable = MovieScene.FindPossessable(TrackBinding);
					if (ensure(ChildPossessable))
					{
						ChildPossessable->SetParent(ActorBinding, &MovieScene);
					}
					LevelSequence->BindPossessableObject(TrackBinding, *ActorComponent, LinkedObject);
				}
				else
				{
					UE_LOG(LogMayaLiveLink, Warning,
							TEXT("Unable to bind possessable %s of class %s to level sequence %s"),
							*ActorComponent->GetName(),
							*ActorComponent->GetClass()->GetName(),
							*LevelSequence->GetName());
				}

				return TrackBinding;
			};

			// Adding default tracks for lights
			if (ActorClass->IsChildOf<ALight>())
			{
				ALight* LightActor = Cast<ALight>(LinkedObject);
				if (!TrackBinding.IsValid())
				{
					TrackBinding = BindActorComponentToSequence(LightActor->GetLightComponent());
				}
				if (TrackBinding.IsValid())
				{
					AddOrFindTrack<UMovieSceneFloatTrack>(TrackBinding, "Intensity", MovieScene);
					AddOrFindTrack<UMovieSceneColorTrack>(TrackBinding, "LightColor", MovieScene);
				}
			}
			// Adding default tracks for cameras
			else if (ActorClass->IsChildOf<ACameraActor>())
			{
				ACameraActor* CameraActor = Cast<ACameraActor>(LinkedObject);
				if (!TrackBinding.IsValid())
				{
					TrackBinding = BindActorComponentToSequence(CameraActor->GetCameraComponent());
				}
				if (TrackBinding.IsValid())
				{
					if (ActorClass->IsChildOf<ACineCameraActor>())
					{
						AddOrFindTrack<UMovieSceneFloatTrack>(TrackBinding, "CurrentAperture", MovieScene);
						AddOrFindTrack<UMovieSceneFloatTrack>(TrackBinding, "CurrentFocalLength", MovieScene);
					}
					else
					{
						AddOrFindTrack<UMovieSceneFloatTrack>(TrackBinding, "FieldOfView", MovieScene);
					}

					// Create a camera cut track with a camera if it doesn't already exist
					UMovieSceneTrack* CameraCutTrack = MovieScene.GetCameraCutTrack();
					if (!CameraCutTrack)
					{
						// Create a new camera cut section and add it to the camera cut track
						CameraCutTrack = MovieScene.AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
						if (UMovieSceneCameraCutSection* CameraCutSection = NewObject<UMovieSceneCameraCutSection>(CameraCutTrack,
																												   NAME_None,
																												   RF_Transactional))
						{
							CameraCutSection->SetRange(MovieScene.GetPlaybackRange());
							CameraCutSection->SetCameraGuid(ActorBinding);
							CameraCutTrack->AddSection(*CameraCutSection);
						}
					}
				}
			}

			// Adding default tracks for actors
			if (ActorClass->IsChildOf<AActor>())
			{
				AddOrFindTrack<UMovieScene3DTransformTrack>(ActorBinding, "Transform", MovieScene);
				AddOrFindTrack<UMovieSceneVisibilityTrack>(ActorBinding, "bHidden", MovieScene);
			}

			ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();

			if (bResizeTracks)
			{
				ResizeTracks(MovieScene, ActorBinding, TrackBinding);
			}
		}
	}
}

void UMayaLiveLinkLevelSequenceHelper::PushFrameDataToLevelSequence(const FMayaLiveLinkLevelSequenceFrameData& FrameData, const FMayaLiveLinkLevelSequenceParams& LevelSequenceParams)
{
	if (FrameData.Curves.IsEmpty())
	{
		return;
	}

	const FGuid& Binding = LevelSequenceParams.TrackBinding;
	const FGuid& ActorBinding = LevelSequenceParams.ActorBinding;
	if (LevelSequenceParams.SequencePath.IsEmpty() ||
		LevelSequenceParams.SequenceName.IsEmpty() ||
		!ActorBinding.IsValid())
	{
		return;
	}

	// Find the level sequence
	auto LevelSequence = FMayaLiveLinkUtils::FindAsset<ULevelSequence>(
									FPaths::Combine(LevelSequenceParams.SequencePath, LevelSequenceParams.SequenceName),
									LevelSequenceParams.SequenceName);
	if (!LevelSequence || !LevelSequence->MovieScene)
	{
		UE_LOG(LogMayaLiveLink, Warning,
			   TEXT("Could not find LevelSequence %s located at %s"),
			   *LevelSequenceParams.SequenceName,
			   *LevelSequenceParams.SequencePath);
		return;
	}

	// Make sure the binding referred to by the subject name still exists
	UMovieScene& MovieScene = *LevelSequence->MovieScene;
	FMovieScenePossessable* Possessable = nullptr;
	if (Binding.IsValid())
	{
		Possessable = MovieScene.FindPossessable(Binding);
	}

	auto ActorPossessable = MovieScene.FindPossessable(ActorBinding);
	if (!ActorPossessable)
	{
		UE_LOG(LogMayaLiveLink, Warning,
			   TEXT("Unable to find possessable for Guid %s"),
			   *ActorBinding.ToString());
		return;
	}

	// Check if any of the curves to update is a transform or a color curve
	bool HasTransform = false;
	bool HasColor = false;
	for (auto& CurvePair : FrameData.Curves)
	{
		if (!HasTransform && TransformChannelMapping.Contains(CurvePair.Key))
		{
			HasTransform = true;
		}
		if (!HasColor && ColorSectionMapping.Contains(CurvePair.Key))
		{
			HasColor = true;
		}
	}

	// Find the actor referred to by the subject name
	AActor* LinkedObject = nullptr;
	bool LinkedObjectNotFound = false;
	auto GetLinkedObject = [ActorPossessable, &LinkedObjectNotFound]() -> AActor*
	{
		if (LinkedObjectNotFound)
		{
			return nullptr;
		}

		AActor* LinkedObject = nullptr;
		auto World = FindWorld();
		if (World)
		{
			TActorIterator<AActor> It(World, (UClass*)ActorPossessable->GetPossessedObjectClass());
			for (; It; ++It)
			{
				auto Actor = *It;
				if (Actor->GetActorLabel() == ActorPossessable->GetName())
				{
					LinkedObject = Actor;
					break;
				}
			}
		}

		if (!LinkedObject)
		{
			UE_LOG(LogMayaLiveLink, Warning, TEXT("Could not find object %s"), *ActorPossessable->GetName());
			LinkedObjectNotFound = true;
		}

		return LinkedObject;
	};

	TSet<FString> ProcessedCurves;
	bool RefreshSequencer = false;

	// Update the transform curves
	if (HasTransform)
	{
		if (auto MovieSceneTrack = AddOrFindTrack<UMovieScene3DTransformTrack>(LevelSequenceParams.ActorBinding, "Transform", MovieScene))
		{
			TArray <UMovieSceneSection*> MovieSceneSections = MovieSceneTrack->GetAllSections();
			if (MovieSceneSections.Num())
			{
				auto Section = CastChecked<UMovieScene3DTransformSection>(MovieSceneSections[0]);
				if (Section && Section->TryModify(true))
				{
					auto Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();

					EMovieSceneTransformChannel Mask = Section->GetMask().GetChannels();
					for (auto& CurvePair : FrameData.Curves)
					{
						if (auto Channel = TransformChannelMapping.Find(CurvePair.Key))
						{
							EMovieSceneTransformChannel TransformMask = Channel->Key;
							ProcessedCurves.Add(CurvePair.Key);

							if (EnumHasAnyFlags(Mask, TransformMask))
							{
								SetChannel<FMovieSceneDoubleChannel, FMovieSceneDoubleValue>(*Channels[Channel->Value],
																							 CurvePair.Value.KeyFrames);
							}
						}
					}

					RefreshSequencer = true;
				}
			}
		}
	}

	// Update the color curves
	if (HasColor && Binding.IsValid())
	{
		if (auto MovieSceneTrack = AddOrFindTrack<UMovieSceneColorTrack>(Binding, "LightColor", MovieScene))
		{
			TArray <UMovieSceneSection*> MovieSceneSections = MovieSceneTrack->GetAllSections();
			if (MovieSceneSections.Num())
			{
				auto Section = CastChecked<UMovieSceneColorSection>(MovieSceneSections[0]);
				if (Section && Section->TryModify(true))
				{
					TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
					for (auto& CurvePair : FrameData.Curves)
					{
						if (auto ChannelPtr = ColorSectionMapping.Find(CurvePair.Key))
						{
							auto Channel = *ChannelPtr;
							ProcessedCurves.Add(CurvePair.Key);

							if (Channel < FloatChannels.Num())
							{
								SetChannel(*FloatChannels[Channel], CurvePair.Value.KeyFrames);
							}
						}
					}
					
					RefreshSequencer = true;
				}
			}
		}
	}

	// Update the visibility/custom attribute curves
	if (ProcessedCurves.Num() < FrameData.Curves.Num())
	{
		bool HasVisibility = false;
		for (auto& CurvePair : FrameData.Curves)
		{
			if (!ProcessedCurves.Contains(CurvePair.Key))
			{
				// The visibility curve is controlled by the bHidden property
				if (!HasVisibility && CurvePair.Key == "bHidden")
				{
					HasVisibility = true;
					if (auto MovieSceneTrack = AddOrFindTrack<UMovieSceneVisibilityTrack>(LevelSequenceParams.ActorBinding, CurvePair.Key, MovieScene))
					{
						TArray<UMovieSceneSection*> MovieSceneSections = MovieSceneTrack->GetAllSections();
						if (MovieSceneSections.Num())
						{
							auto Section = CastChecked<UMovieSceneBoolSection>(MovieSceneSections[0]);
							if (Section && Section->TryModify(true))
							{
								// Setup the boolean curve that controls the actor visibility
								TArrayView<FMovieSceneBoolChannel*> BoolChannels = Section->GetChannelProxy().GetChannels<FMovieSceneBoolChannel>();
								if (BoolChannels.Num())
								{
									auto BoolChannel = BoolChannels[0];
									BoolChannel->Reset();
									auto Data = BoolChannel->GetData();
									for (auto& KeyFramePair : CurvePair.Value.KeyFrames)
									{
										auto& KeyFrame = KeyFramePair.Value;
 										Data.UpdateOrAddKey(static_cast<int32>(KeyFramePair.Key), KeyFrame.Value >= 0.5);
 									}
									RefreshSequencer = true;
								}
							}
						}
					}
				}
				else
				{
					// Find the track to update, starting first by using the actor component binding
					UMovieSceneFloatTrack* MovieSceneTrack = nullptr;
					if (Binding.IsValid())
					{
						MovieSceneTrack = MovieScene.FindTrack<UMovieSceneFloatTrack>(Binding, FName(CurvePair.Key));
					}
 					if (!MovieSceneTrack)
					{
						// The binding is on the actor itself instead
						MovieSceneTrack = MovieScene.FindTrack<UMovieSceneFloatTrack>(ActorBinding, FName(CurvePair.Key));

						// Determine if we should add this curve as a new track
						if (!MovieSceneTrack)
						{
							// Get the actor in the level
							if (!LinkedObject)
							{
								LinkedObject = GetLinkedObject();
							}

							if (LinkedObject)
							{
								// Create the missing track from those that we support
								if (LinkedObject->IsA<ACineCameraActor>())
								{
									if (CurvePair.Key == "Filmback.SensorWidth")
									{
										MovieSceneTrack = AddOrFindTrack<UMovieSceneFloatTrack>(Binding, "SensorWidth", MovieScene,
																								GET_MEMBER_NAME_STRING_CHECKED(UCineCameraComponent, Filmback));
										AddOrFindTrack<UMovieSceneFloatTrack>(Binding, "SensorHeight", MovieScene,
																			  GET_MEMBER_NAME_STRING_CHECKED(UCineCameraComponent, Filmback));
									}
									else if (CurvePair.Key == "Filmback.SensorHeight")
									{
										AddOrFindTrack<UMovieSceneFloatTrack>(Binding, "SensorWidth", MovieScene,
																			  GET_MEMBER_NAME_STRING_CHECKED(UCineCameraComponent, Filmback));
										MovieSceneTrack = AddOrFindTrack<UMovieSceneFloatTrack>(Binding, "SensorHeight", MovieScene,
																								GET_MEMBER_NAME_STRING_CHECKED(UCineCameraComponent, Filmback));
									}
									else if (CurvePair.Key == "FocusSettings.ManualFocusDistance")
									{
										MovieSceneTrack = AddOrFindTrack<UMovieSceneFloatTrack>(Binding, "ManualFocusDistance", MovieScene,
																								GET_MEMBER_NAME_STRING_CHECKED(UCineCameraComponent, FocusSettings));
									}
									else if (CurvePair.Key == "CurrentAperture" || CurvePair.Key == "CurrentFocalLength")
									{
										MovieSceneTrack = AddOrFindTrack<UMovieSceneFloatTrack>(Binding, CurvePair.Key, MovieScene);
									}
								}
								else if (LinkedObject->IsA<ACameraActor>())
								{
									if (CurvePair.Key == "AspectRatio" || CurvePair.Key == "FieldOfView")
									{
										MovieSceneTrack = AddOrFindTrack<UMovieSceneFloatTrack>(Binding, CurvePair.Key, MovieScene);
									}
								}
								RefreshSequencer |= (MovieSceneTrack != nullptr);

								if (!MovieSceneTrack)
								{
									// Try to find the curve name as an actor's property
									FName CurveFName(*CurvePair.Key);
									for (TFieldIterator<FDoubleProperty> Property(LinkedObject->GetClass()); Property; ++Property)
									{
										if (Property->GetFName() == CurveFName)
										{
											MovieSceneTrack = AddOrFindTrack<UMovieSceneFloatTrack>(ActorBinding, CurvePair.Key, MovieScene);
											RefreshSequencer = true;
											break;
										}
									}

									if (!MovieSceneTrack && Binding.IsValid() && Possessable)
									{
										// Try to find the curve name as an actor component's property
										for (TFieldIterator<FDoubleProperty> Property(Possessable->GetPossessedObjectClass()); Property; ++Property)
										{
											if (Property->GetFName() == CurveFName)
											{
												MovieSceneTrack = AddOrFindTrack<UMovieSceneFloatTrack>(Binding, CurvePair.Key, MovieScene);
												RefreshSequencer = true;
												break;
											}
										}
									}
								}
							}
						}
					}

					if (MovieSceneTrack)
					{
						// Update the track with the anim curve floating values
						TArray<UMovieSceneSection*> MovieSceneSections = MovieSceneTrack->GetAllSections();
						if (MovieSceneSections.Num())
						{
							auto Section = CastChecked<UMovieSceneFloatSection>(MovieSceneSections[0]);
							if (Section && Section->TryModify(true))
							{
								TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
								if (FloatChannels.Num())
								{
									SetChannel(*FloatChannels[0], CurvePair.Value.KeyFrames);
								}
							}
						}
					}
				}
			}
		}

		if (RefreshSequencer)
		{
			// Sequence was changed, trigger a refresh of the Sequence UI to see the changes
			ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
		}
	}

	FMayaLiveLinkUtils::RefreshContentBrowser(*LevelSequence);
}

template<typename T>
T* UMayaLiveLinkLevelSequenceHelper::AddOrFindTrack(const FGuid& TrackBinding,
													const FString& PropertyName,
													UMovieScene& MovieScene,
													const TCHAR* Path)
{
	// Check if the track already exists
	T* Track = MovieScene.FindTrack<T>(TrackBinding, FName(PropertyName));
	if (Track)
	{
		return Track;
	}
	else if (Path)
	{
		// Find the track using the provided path
		Track = MovieScene.FindTrack<T>(TrackBinding, FName(*(FString(Path) + FString(".") + PropertyName)));
		if (Track)
		{
			return Track;
		}
	}

	// Add a track to the moving scene
	Track = MovieScene.AddTrack<T>(TrackBinding);
	if (Track)
	{
		// Setup the path to the property that Unreal will update (color, transform, etc)
		FString PropertyPath;
		if (Path)
		{
			TStringBuilder<256> PropertyPathBuilder;
			PropertyPathBuilder.Append(Path);
			PropertyPathBuilder.Append(TEXT("."));
			PropertyPathBuilder.Append(*PropertyName);
			PropertyPath = PropertyPathBuilder.ToString();
		}
		else
		{
			PropertyPath = PropertyName;
		}

		// Create a section that covers the whole track
		Track->SetPropertyNameAndPath(*FName::NameToDisplayString(PropertyName, false), PropertyPath);
		if (UMovieSceneSection* Section = Track->CreateNewSection())
		{
			Section->SetRange(MovieScene.GetPlaybackRange());
			Track->AddSection(*Section);
			Track->SetSectionToKey(Section);
		}
	}
	return Track;
}

UWorld* UMayaLiveLinkLevelSequenceHelper::FindWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	// Find the world to be able to iterate through the actors
	UWorld* World = nullptr;
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::Editor)
		{
			World = WorldContext.World();
			if (World)
			{
				break;
			}
		}
	}

	return World;
}

void UMayaLiveLinkLevelSequenceHelper::ResizeTracks(UMovieScene& MovieScene,
													const FGuid& ActorBinding,
													const FGuid& TrackBinding)
{
	auto PlaybackRange = MovieScene.GetPlaybackRange();

	auto ResizeTrack = [](UMovieSceneTrack& Track, const FFrameNumberRange& PlaybackRange)
	{
		if (Track.GetAllSections().Num() == 0)
		{
			return;
		}

		const auto& Sections = Track.GetAllSections();

		// Check if we need to update the track end
		auto LastSection = Track.GetAllSections().Last();
		auto Range = LastSection->GetTrueRange();
		bool bUpdateRange = false;
		if (Range.GetLowerBound().IsOpen() || Range.GetUpperBound().IsOpen())
		{
			Range = TRange<FFrameNumber>(PlaybackRange.GetLowerBoundValue(), PlaybackRange.GetUpperBoundValue());
			bUpdateRange = true;
		}
		if (bUpdateRange || PlaybackRange.GetUpperBoundValue() != Range.GetUpperBoundValue())
		{
			Range.SetUpperBoundValue(PlaybackRange.GetUpperBoundValue());
			LastSection->SetRange(Range);
		}

		// Check if we need to update the track beginning
		auto FirstSection = Track.GetAllSections().Num() > 1 ? Sections[0] : LastSection;
		auto FirstRange = FirstSection->GetTrueRange();
		bUpdateRange = false;
		if (FirstRange.GetLowerBound().IsOpen() || FirstRange.GetUpperBound().IsOpen())
		{
			FirstRange = TRange<FFrameNumber>(PlaybackRange.GetLowerBoundValue(), Range.GetLowerBoundValue());
			bUpdateRange = true;
		}
		if (bUpdateRange || PlaybackRange.GetLowerBoundValue() != FirstRange.GetLowerBoundValue())
		{
			Range.SetLowerBoundValue(PlaybackRange.GetLowerBoundValue());
			FirstSection->SetRange(Range);
		}
	};

	// Check if we need to resize the tracks
	auto ResizeBindingTracks = [&MovieScene, &PlaybackRange](const FGuid& Binding, auto& ResizeTrackFunc)
	{
		if (!Binding.IsValid())
		{
			return;
		}

		auto MovieSceneBinding = MovieScene.FindBinding(Binding);
		if (!MovieSceneBinding)
		{
			return;
		}

		for (auto& Track : MovieSceneBinding->GetTracks())
		{
			ResizeTrackFunc(*Track, PlaybackRange);
		}
	};

	// Resize the actor's tracks
	ResizeBindingTracks(ActorBinding, ResizeTrack);

	// Resize the actor component's tracks
	ResizeBindingTracks(TrackBinding, ResizeTrack);

	UMovieSceneTrack* CameraCutTrack = MovieScene.GetCameraCutTrack();
	if (CameraCutTrack)
	{
		ResizeTrack(*CameraCutTrack, PlaybackRange);
	}
}

#undef LOCTEXT_NAMESPACE
