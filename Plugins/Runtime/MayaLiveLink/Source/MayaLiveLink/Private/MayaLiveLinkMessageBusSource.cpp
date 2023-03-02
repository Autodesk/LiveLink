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

#include "MayaLiveLinkMessageBusSource.h"
#include "MayaLiveLinkInterface.h"

#include "Async/Async.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorClassUtils.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "ILiveLinkClient.h"
#include "MayaLiveLinkModule.h"
#include "LiveLinkLog.h"
#include "MayaLiveLinkMessages.h"
#include "LiveLinkRoleTrait.h"
#include "LiveLinkTypes.h"

#include "Kismet/GameplayStatics.h"

#include "Roles/MayaLiveLinkAnimSequenceRole.h"
#include "Roles/MayaLiveLinkLevelSequenceRole.h"
#include "Roles/MayaLiveLinkTimelineTypes.h"

#include "MayaLiveLinkTimelineSync.h"
#include "MayaLiveLinkAnimSequenceHelper.h"
#include "MayaLiveLinkLevelSequenceHelper.h"
#include "MayaLiveLinkUtils.h"

#include "MessageEndpointBuilder.h"
#include "Misc/App.h"


FMayaLiveLinkMessageBusSource::FMayaLiveLinkMessageBusSource(const FText& InSourceType, const FText& InSourceMachineName, const FMessageAddress& InConnectionAddress, double InMachineTimeOffset)
: FLiveLinkMessageBusSource(InSourceType, InSourceMachineName, InConnectionAddress, InMachineTimeOffset)
{
}

void FMayaLiveLinkMessageBusSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	FLiveLinkMessageBusSource::ReceiveClient(InClient, InSourceGuid);

	if (IsMessageEndpointConnected())
	{
		FMayaLiveLinkTimelineSyncModule::GetModule().GetOnTimeChangedDelegate().AddRaw(this, &FMayaLiveLinkMessageBusSource::HandleTimeChangeReturn);
	}
}

void FMayaLiveLinkMessageBusSource::InitializeMessageEndpoint(FMessageEndpointBuilder& EndpointBuilder)
{
	EndpointBuilder
		.Handling<FMayaLiveLinkListAssetsRequestMessage>(this, &FMayaLiveLinkMessageBusSource::HandleListAssetsRequest)
		.Handling<FMayaLiveLinkListAnimSequenceSkeletonRequestMessage>(this, &FMayaLiveLinkMessageBusSource::HandleListAnimSequenceSkeletonRequest)
		.Handling<FMayaLiveLinkListAssetsByParentClassRequestMessage>(this, &FMayaLiveLinkMessageBusSource::HandleListAssetsByParentClassRequest)
		.Handling<FMayaLiveLinkListActorsRequestMessage>(this, &FMayaLiveLinkMessageBusSource::HandleListActorsRequest)
		.Handling<FMayaLiveLinkTimeChangeRequestMessage>(this, &FMayaLiveLinkMessageBusSource::HandleTimeChangeRequest);

	FLiveLinkMessageBusSource::InitializeMessageEndpoint(EndpointBuilder);
}

void FMayaLiveLinkMessageBusSource::InitializeAndPushStaticData_AnyThread(FName SubjectName,
																		  TSubclassOf<ULiveLinkRole> SubjectRole,
																		  const FLiveLinkSubjectKey& SubjectKey,
																		  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
																		  UScriptStruct* MessageTypeInfo)
{
	check(MessageTypeInfo->IsChildOf(FLiveLinkBaseStaticData::StaticStruct()));

	const FLiveLinkBaseStaticData* Message = reinterpret_cast<const FLiveLinkBaseStaticData*>(Context->GetMessage());

	if (SubjectRole->IsChildOf(UMayaLiveLinkAnimSequenceRole::StaticClass()) &&
		MessageTypeInfo->IsChildOf(FMayaLiveLinkAnimSequenceStaticData::StaticStruct()))
	{
		const FMayaLiveLinkAnimSequenceStaticData& StaticData = *reinterpret_cast<const FMayaLiveLinkAnimSequenceStaticData*>(Context->GetMessage());
		{
			FScopeLock Lock(&SubjectTimelineParamsCriticalSection);
			FMayaLiveLinkAnimSequenceParams& TimelineParams = SubjectTimelineParams.FindOrAdd(SubjectName);
			TimelineParams.SequenceName = StaticData.SequenceName;
			TimelineParams.SequencePath = StaticData.SequencePath;
			TimelineParams.FrameRate = StaticData.FrameRate;
			TimelineParams.StartFrame = StaticData.StartFrame;
			TimelineParams.EndFrame = StaticData.EndFrame;
			TimelineParams.LinkedAssetPath = StaticData.LinkedAssetPath;
		}

		// Keep a copy of the static data and push it on the game thread.
		// Creating an animation sequence and looking for assets must happen on the game thread.
		TSharedPtr<FLiveLinkStaticDataStruct, ESPMode::ThreadSafe> StaticDataStruct = MakeShareable(new FLiveLinkStaticDataStruct(MessageTypeInfo));
		StaticDataStruct->InitializeWith(MessageTypeInfo, Message);
		AsyncTask(ENamedThreads::GameThread, [this, SubjectName, StaticDataStruct]()
		{
			PushStaticDataToAnimSequence(SubjectName, StaticDataStruct);
		});

		FLiveLinkStaticDataStruct DataStruct(MessageTypeInfo);
		PushClientSubjectStaticData_AnyThread(SubjectKey, SubjectRole, MoveTemp(DataStruct));
	}
	else if (SubjectRole->IsChildOf(UMayaLiveLinkLevelSequenceRole::StaticClass()) &&
			 MessageTypeInfo->IsChildOf(FMayaLiveLinkLevelSequenceStaticData::StaticStruct()))
	{
		const FMayaLiveLinkLevelSequenceStaticData& StaticData = *reinterpret_cast<const FMayaLiveLinkLevelSequenceStaticData*>(Context->GetMessage());
		{
			FScopeLock Lock(&SubjectTimelineParamsCriticalSection);
			FMayaLiveLinkLevelSequenceParams& SequenceParams = SubjectLevelSequenceParams.FindOrAdd(SubjectName);
			SequenceParams.SequenceName = StaticData.SequenceName;
			SequenceParams.SequencePath = StaticData.SequencePath;
			SequenceParams.FrameRate = StaticData.FrameRate;
			SequenceParams.StartFrame = StaticData.StartFrame;
			SequenceParams.EndFrame = StaticData.EndFrame;
			SequenceParams.LinkedAssetPath = StaticData.LinkedAssetPath;
		}

		// Keep a copy of the static data and push it on the game thread.
		// Creating an level sequence and looking for assets must happen on the game thread.
		TSharedPtr<FLiveLinkStaticDataStruct, ESPMode::ThreadSafe> StaticDataStruct = MakeShareable(new FLiveLinkStaticDataStruct(MessageTypeInfo));
		StaticDataStruct->InitializeWith(MessageTypeInfo, Message);

		FLiveLinkStaticDataStruct DataStruct(MessageTypeInfo);
		auto DummyStaticData = DataStruct.Cast<FMayaLiveLinkLevelSequenceStaticData>();
		DummyStaticData->LinkedAssetPath = StaticData.LinkedAssetPath;
		DummyStaticData->SequenceName = StaticData.SequenceName;
		DummyStaticData->SequencePath = StaticData.SequencePath;
		DummyStaticData->FrameRate = StaticData.FrameRate;
		DummyStaticData->StartFrame = StaticData.StartFrame;
		DummyStaticData->EndFrame = StaticData.EndFrame;

		AsyncTask(ENamedThreads::GameThread, [this, SubjectName, StaticDataStruct]()
		{
			PushStaticDataToLevelSequence(SubjectName, StaticDataStruct);
		});

		PushClientSubjectStaticData_AnyThread(SubjectKey, SubjectRole, MoveTemp(DataStruct));
	}
	else
	{
		FLiveLinkMessageBusSource::InitializeAndPushStaticData_AnyThread(SubjectName, SubjectRole, SubjectKey, Context, MessageTypeInfo);
	}
}

void FMayaLiveLinkMessageBusSource::InitializeAndPushFrameData_AnyThread(FName SubjectName,
																		 const FLiveLinkSubjectKey& SubjectKey,
																		 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
																		 UScriptStruct* MessageTypeInfo)
{
	check(MessageTypeInfo->IsChildOf(FLiveLinkBaseFrameData::StaticStruct()));

	FLiveLinkFrameDataStruct DataStruct(MessageTypeInfo);
	const FLiveLinkBaseFrameData* Message = reinterpret_cast<const FLiveLinkBaseFrameData*>(Context->GetMessage());

	if (MessageTypeInfo->IsChildOf(FMayaLiveLinkAnimSequenceFrameData::StaticStruct()))
	{
		// Keep a copy of the frame data and push it on the game thread.
		// Updating an animation sequence and looking for assets must happen on the game thread.
		TSharedPtr<FLiveLinkFrameDataStruct, ESPMode::ThreadSafe> FrameDataStruct = MakeShareable(new FLiveLinkFrameDataStruct(MessageTypeInfo));
		FrameDataStruct->InitializeWith(MessageTypeInfo, Message);
		FrameDataStruct->GetBaseData()->WorldTime = Message->WorldTime.GetOffsettedTime();

		FScopeLock Lock(&SubjectTimelineParamsCriticalSection);
		if (auto TimelineParams = SubjectTimelineParams.Find(SubjectName))
		{
			TSharedPtr<FMayaLiveLinkAnimSequenceParams, ESPMode::ThreadSafe> TimelineParamsDup = MakeShareable(new FMayaLiveLinkAnimSequenceParams(*TimelineParams));
			AsyncTask(ENamedThreads::GameThread, [this, FrameDataStruct, TimelineParamsDup]()
			{
				auto FrameDataPtr = FrameDataStruct.Get();
				UMayaLiveLinkAnimSequenceHelper::PushFrameDataToAnimSequence(*FrameDataPtr->Cast<FMayaLiveLinkAnimSequenceFrameData>(),
																			 *TimelineParamsDup.Get());
			});
		}
		DataStruct.GetBaseData()->WorldTime = Message->WorldTime.GetOffsettedTime();
		PushClientSubjectFrameData_AnyThread(SubjectKey, MoveTemp(DataStruct));
	}
	else if (MessageTypeInfo->IsChildOf(FMayaLiveLinkLevelSequenceFrameData::StaticStruct()))
	{
		// Keep a copy of the frame data and push it on the game thread.
		// Updating an animation sequence and looking for assets must happen on the game thread.
		TSharedPtr<FLiveLinkFrameDataStruct, ESPMode::ThreadSafe> FrameDataStruct = MakeShareable(new FLiveLinkFrameDataStruct(MessageTypeInfo));
		FrameDataStruct->InitializeWith(MessageTypeInfo, Message);
		FrameDataStruct->GetBaseData()->WorldTime = Message->WorldTime.GetOffsettedTime();

		FScopeLock Lock(&SubjectTimelineParamsCriticalSection);
		if (auto SequenceParams = SubjectLevelSequenceParams.Find(SubjectName))
		{
			TSharedPtr<FMayaLiveLinkLevelSequenceParams, ESPMode::ThreadSafe> SequenceParamsDup = MakeShareable(new FMayaLiveLinkLevelSequenceParams(*SequenceParams));
			AsyncTask(ENamedThreads::GameThread, [this, FrameDataStruct, SequenceParamsDup]()
			{
				auto FrameDataPtr = FrameDataStruct.Get();
				UMayaLiveLinkLevelSequenceHelper::PushFrameDataToLevelSequence(*FrameDataPtr->Cast<FMayaLiveLinkLevelSequenceFrameData>(),
																			   *SequenceParamsDup.Get());
			});
		}
		DataStruct.GetBaseData()->WorldTime = Message->WorldTime.GetOffsettedTime();
		PushClientSubjectFrameData_AnyThread(SubjectKey, MoveTemp(DataStruct));
	}
	else
	{
		FLiveLinkMessageBusSource::InitializeAndPushFrameData_AnyThread(SubjectName, SubjectKey, Context, MessageTypeInfo);
	}
}

void FMayaLiveLinkMessageBusSource::HandleListAssetsRequest(const FMayaLiveLinkListAssetsRequestMessage& Message,
															const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (Message.AssetClass.IsEmpty())
	{
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [Message, this]()
	{
		TArray<FAssetData> OutAssetData;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// Get the list of assets of the given class
		FMayaLiveLinkListAssetsReturnMessage* ReturnMessage = FMessageEndpoint::MakeMessage<FMayaLiveLinkListAssetsReturnMessage>();
		UClass* AssetClass = FMayaLiveLinkUtils::FindObject<UClass>(Message.AssetClass);
		if (AssetClass)
		{
			FTopLevelAssetPath AssetClassPath(AssetClass->GetPathName());
			AssetRegistry.GetAssetsByClass(AssetClassPath, OutAssetData, Message.SearchSubClasses);

			if (OutAssetData.Num() > 0)
			{
				// Build the list of assets of the matching class, including child classes
				TMap<FString, FStringArray>& AssetsByClass = ReturnMessage->AssetsByClass;
				for (auto& AssetData : OutAssetData)
				{
					auto& Class = AssetsByClass.FindOrAdd(AssetData.AssetClassPath.ToString());
					Class.Array.Add(AssetData.GetSoftObjectPath().ToString());
				}
			}
		}
		SendMessage(ReturnMessage);
	});
}

void FMayaLiveLinkMessageBusSource::HandleListAnimSequenceSkeletonRequest(const FMayaLiveLinkListAnimSequenceSkeletonRequestMessage& Message,
																		  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	AsyncTask(ENamedThreads::GameThread, [Message, this]()
	{
		TArray<FAssetData> OutAssetData;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// Get the list of assets of the given class
		auto ReturnMessage = FMessageEndpoint::MakeMessage<FMayaLiveLinkListAnimSequenceSkeletonReturnMessage>();
		FTopLevelAssetPath AssetClassPath(UAnimSequence::StaticClass()->GetPathName());
		AssetRegistry.GetAssetsByClass(AssetClassPath, OutAssetData, true);

		if (OutAssetData.Num() > 0)
		{
			// Build the list of assets of the matching class, including child classes
			TMap<FString, FStringArray>& AssetsByClass = ReturnMessage->AnimSequencesBySkeleton;
			for (auto& AssetData : OutAssetData)
			{
				UAnimSequence* AnimSequence = Cast<UAnimSequence>(AssetData.GetAsset());
				if (AnimSequence && AnimSequence->GetSkeleton())
				{
					// Determine the skeleton name
					FString SkeletonName;
					UPackage* Package = AnimSequence->GetSkeleton()->GetPackage();
					if (Package)
					{
						SkeletonName = Package->GetName();
					}
					else
					{
						FString PathName = AnimSequence->GetSkeleton()->GetPathName();
						int32 Index = 0;
						PathName.FindLastChar('.', Index);
						SkeletonName = Index > 0 ? PathName.Mid(0, Index) : PathName;
					}

					// Retrieve the list of AnimSequences for this skeleton
					FStringArray& Class = ReturnMessage->AnimSequencesBySkeleton.FindOrAdd(SkeletonName);
					FString ObjectPath = AssetData.GetSoftObjectPath().ToString();

					// Strip the "." part
					int32 Index = 0;
					ObjectPath.FindLastChar('.', Index);
					Class.Array.Add(Index > 0 ? ObjectPath.Mid(0, Index) : ObjectPath);
				}
			}
		}

		SendMessage(ReturnMessage);
	});
}

void FMayaLiveLinkMessageBusSource::HandleListAssetsByParentClassRequest(const FMayaLiveLinkListAssetsByParentClassRequestMessage& Message,
																		 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (Message.AssetClass.IsEmpty() ||
		Message.ParentClasses.Num() == 0)
	{
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [Message, this]()
	{
		TArray<FAssetData> OutAssetData;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// List blueprint assets
		FTopLevelAssetPath AssetClassPath(UBlueprint::StaticClass()->GetPathName());
		AssetRegistry.GetAssetsByClass(AssetClassPath, OutAssetData, Message.SearchSubClasses);

		// Search for blueprint classes which are child of one of the provided parent classes
		auto ReturnMessage = FMessageEndpoint::MakeMessage<FMayaLiveLinkListAssetsByParentClassReturnMessage>();
		if (OutAssetData.Num() > 0)
		{
			auto& Assets = ReturnMessage->Assets.Array;
			auto& NativeClasses = ReturnMessage->NativeAssetClasses.Array;

			// Get the parent UClass
			TArray<UClass*> ParentClasses;
			TSet<FString> ParentClassesSet;
			for (const FString& Parent : Message.ParentClasses)
			{
				if (UClass* Class = FEditorClassUtils::GetClassFromString(Parent))
				{
					ParentClasses.Add(Class);
					ParentClassesSet.Add(Parent);
					NativeClasses.Add(Parent);
				}
			}

			// Look for blueprint classes
			TSet<FString> BlueprintClasses;
			for (const FAssetData& AssetData : OutAssetData)
			{
				const FString PackageName = AssetData.PackageName.ToString();
				if (PackageName.StartsWith("/Engine/"))
				{
					// Ignore Engine blueprints that will take too long to load the first time around
					continue;
				}

				const UBlueprint* BlueprintAsset = Cast<UBlueprint>(AssetData.GetAsset());
				if (BlueprintAsset && !BlueprintClasses.Contains(BlueprintAsset->GetClass()->GetName()))
				{
					for (const UClass* ParentClass : ParentClasses)
					{
						// Verify if the blueprint is a child of one of the parent classes
						auto Class = BlueprintAsset->GeneratedClass;
						if (Class && Class->IsChildOf(ParentClass))
						{
							ParentClassesSet.Add(PackageName);

							FString AssetNativeParentClassName;
							if (AssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, AssetNativeParentClassName))
							{
								int32 EndIndex = -1;
								if (AssetNativeParentClassName.FindLastChar(TEXT('.'), EndIndex) && EndIndex >= 0)
								{
									NativeClasses.Add(AssetNativeParentClassName.Mid(EndIndex + 1, AssetNativeParentClassName.Len() - (EndIndex + 2)));
								}
								else
								{
									NativeClasses.Add(AssetNativeParentClassName);
								}
							}
							else
							{
								NativeClasses.Add(PackageName);
							}
							break;
						}
					}
				}
			}

			Assets = ParentClassesSet.Array();
		}

		SendMessage(ReturnMessage);
	});
}

void FMayaLiveLinkMessageBusSource::HandleListActorsRequest(const FMayaLiveLinkListActorsRequestMessage& Message,
															const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (Message.ActorClass.IsEmpty())
	{
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [Message, this]()
	{
		// Get the list of actor of the given class
 		TArray<AActor*> OutActors;
		UWorld* EditorWorld = nullptr;
#if WITH_EDITOR
		if (UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
		{
			EditorWorld = UnrealEditorSubsystem->GetEditorWorld();
		}
#endif // WITH_EDITOR

		if (!EditorWorld)
		{
			return;
		}

		UGameplayStatics::GetAllActorsOfClass(EditorWorld, FEditorClassUtils::GetClassFromString(Message.ActorClass), OutActors);

		FMayaLiveLinkListActorsReturnMessage* ReturnMessage = FMessageEndpoint::MakeMessage<FMayaLiveLinkListActorsReturnMessage>();
		if (OutActors.Num() > 0)
		{
			// Build the list of actors of the matching class, including child classes
			auto& ActorsByClass = ReturnMessage->ActorsByClass;
			for (auto& Actor : OutActors)
			{
				auto& Class = ActorsByClass.FindOrAdd(Actor->GetClass()->GetName());
				FString ActorPath;
				if (Actor->GetFolderPath() != NAME_None)
				{
					// Include the folder name that can be seen in the World Outliner
					ActorPath = Actor->GetFolderPath().ToString() + "/";
				}
				ActorPath += Actor->GetActorLabel();
				Class.Array.Add(ActorPath);
			}
		}
		SendMessage(ReturnMessage);
	});
}

void FMayaLiveLinkMessageBusSource::HandleTimeChangeRequest(const FMayaLiveLinkTimeChangeRequestMessage& Message,
															const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	AsyncTask(ENamedThreads::GameThread, [Message, this]()
	{
		FMayaLiveLinkTimelineSyncModule::GetModule().SetCurrentTime(Message.Time);
	});
}

void FMayaLiveLinkMessageBusSource::HandleTimeChangeReturn(const FQualifiedFrameTime& Time)
{
	if (IsMessageEndpointConnected())
	{
		auto Message = FMessageEndpoint::MakeMessage<FMayaLiveLinkTimeChangeReturnMessage>();
		Message->Time = Time;
		SendMessage(Message);
	}
}
/* TODO PLB
FORCEINLINE void FMayaLiveLinkMessageBusSource::UpdateConnectionLastActive()
{
	FScopeLock ConnectionTimeLock(&ConnectionLastActiveSection);

	ConnectionLastActive = FPlatformTime::Seconds();
}

void FMayaLiveLinkMessageBusSource::SendConnectMessage()
{
	FMayaLiveLinkConnectMessage* ConnectMessage = FMessageEndpoint::MakeMessage<FMayaLiveLinkConnectMessage>();
	ConnectMessage->LiveLinkVersion = 2;
	ConnectMessage->MayaLiveLinkVersion = FMayaLiveLinkInterfaceModule::GetPluginVersion();
	ConnectMessage->UnrealVersion = FMayaLiveLinkInterfaceModule::GetEngineVersion();

	MessageEndpoint->Send(ConnectMessage, ConnectionAddress);
	FMayaLiveLinkHeartbeatEmitter& HeartbeatEmitter = IMayaLiveLinkModule::Get().GetHeartbeatEmitter();
	HeartbeatEmitter.StartHeartbeat(ConnectionAddress, MessageEndpoint);
	bIsValid = true;
}
*/
bool FMayaLiveLinkMessageBusSource::RequestSourceShutdown()
{
	if (IsMessageEndpointConnected())
	{
		SendMessage(FMessageEndpoint::MakeMessage<FMayaLiveLinkSourceShutdownMessage>());
	}

	FMayaLiveLinkTimelineSyncModule::GetModule().GetOnTimeChangedDelegate().RemoveAll(this);
	FMayaLiveLinkTimelineSyncModule::GetModule().RemoveAllAnimSequenceStartFrames();

	return FLiveLinkMessageBusSource::RequestSourceShutdown();
}

void FMayaLiveLinkMessageBusSource::PushStaticDataToAnimSequence(const FName& SubjectName,
																 TSharedPtr<FLiveLinkStaticDataStruct, ESPMode::ThreadSafe> StaticDataPtr)
{
	if (!StaticDataPtr)
	{
		return;
	}

	auto TimelineStaticDataPtr = StaticDataPtr->Cast<FMayaLiveLinkAnimSequenceStaticData>();
	if (!TimelineStaticDataPtr)
	{
		return;
	}

	auto& StaticData = *TimelineStaticDataPtr;

	// Push to the static data to generate an animation sequence
	TArray<int32> BoneTrackRemapping;
	FString AnimSequenceName;
	UMayaLiveLinkAnimSequenceHelper::PushStaticDataToAnimSequence(StaticData, BoneTrackRemapping, AnimSequenceName);
	{
		FScopeLock Lock(&SubjectTimelineParamsCriticalSection);
		if (auto Params = SubjectTimelineParams.Find(SubjectName))
		{
			// Keep track of the remapping between the static data bones and the reference skeleton's bones
			Params->BoneTrackRemapping = MoveTemp(BoneTrackRemapping);
			Params->FullSequenceName = AnimSequenceName;
		}
	}

	if (!AnimSequenceName.IsEmpty())
	{
		FMayaLiveLinkTimelineSyncModule::GetModule().AddAnimSequenceStartFrame(AnimSequenceName, StaticData.StartFrame);
	}
}

void FMayaLiveLinkMessageBusSource::PushStaticDataToLevelSequence(const FName& SubjectName,
																  TSharedPtr<FLiveLinkStaticDataStruct, ESPMode::ThreadSafe> StaticDataPtr)
{
	if (!StaticDataPtr)
	{
		return;
	}

	auto TimelineStaticDataPtr = StaticDataPtr->Cast<FMayaLiveLinkLevelSequenceStaticData>();
	if (!TimelineStaticDataPtr)
	{
		return;
	}

	// Push to the static data to generate an level sequence
	FGuid ActorBinding, TrackBinding;
	UMayaLiveLinkLevelSequenceHelper::PushStaticDataToLevelSequence(*TimelineStaticDataPtr, ActorBinding, TrackBinding);
	{
		FScopeLock Lock(&SubjectTimelineParamsCriticalSection);
		if (auto Params = SubjectLevelSequenceParams.Find(SubjectName))
		{
			// Keep track of the remapping between the static data and the linked asset
			Params->ActorBinding = ActorBinding;
			Params->TrackBinding = TrackBinding;
		}
	}
}
