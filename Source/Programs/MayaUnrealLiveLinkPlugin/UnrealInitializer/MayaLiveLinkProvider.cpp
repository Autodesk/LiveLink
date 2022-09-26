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

#include "MayaLiveLinkProvider.h"

#include "ILiveLinkClient.h"
#include "IMessageContext.h"
#include "LiveLinkTypes.h"
#include "MayaLiveLinkInterface.h"
#include "MayaLiveLinkMessages.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"

#include "HAL/PlatformProcess.h"

#include "Logging/LogMacros.h"

#include "Misc/ScopeLock.h"

DEFINE_LOG_CATEGORY_STATIC(LogMayaLiveLinkProvider, Warning, All);

FMayaLiveLinkProvider::FMayaLiveLinkProvider(const FString& InProviderName, FMessageEndpointBuilder&& EndpointBuilder)
: FLiveLinkProvider(InProviderName, MoveTemp(EndpointBuilder))
{
}

bool FMayaLiveLinkProvider::GetAssetsByClass(const FString& ClassName, bool bSearchSubClasses, TMap<FString, FStringArray>& Assets)
{
	if (ClassName.IsEmpty())
	{
		return false;
	}

	QueriedResultReady = false;

	// Request to get the list of assets by class
	auto Message = FMessageEndpoint::MakeMessage<FMayaLiveLinkListAssetsRequestMessage>();
	Message->AssetClass = ClassName;
	Message->SearchSubClasses = bSearchSubClasses;

	SendMessage(Message);

	// Wait for the return message or time out if it takes too long
	double Start = FPlatformTime::Seconds();
	while ((FPlatformTime::Seconds() - Start) < 5 && !QueriedResultReady)
	{
		FPlatformProcess::Sleep(0.1f);
	}

	Assets.Empty();
	{
		// If a return message was received, get the result
		FScopeLock Lock(&CriticalSection);
		if (QueriedResultReady && QueriedAssets.Num() > 0)
		{
			Assets = MoveTemp(QueriedAssets);
		}
		QueriedAssets.Empty();
	}

	return QueriedResultReady;
}

bool FMayaLiveLinkProvider::GetAssetsByParentClass(const FString& ClassName,
												   bool bSearchSubClasses,
												   const TArray<FString>& ParentClasses,
												   FStringArray& Assets,
												   FStringArray& NativeAssetClasses)
{
	if (ClassName.IsEmpty())
	{
		return false;
	}

	QueriedResultReady = false;

	// Request to get the list of blueprint assets by class
	auto Message = FMessageEndpoint::MakeMessage<FMayaLiveLinkListAssetsByParentClassRequestMessage>();
	Message->AssetClass = ClassName;
	Message->SearchSubClasses = bSearchSubClasses;
	Message->ParentClasses = ParentClasses;

	SendMessage(Message);

	// Wait for the return message or time out if it takes too long
	double Start = FPlatformTime::Seconds();
	while ((FPlatformTime::Seconds() - Start) < 10 && !QueriedResultReady)
	{
		FPlatformProcess::Sleep(0.1f);
	}

	Assets.Array.Empty();
	NativeAssetClasses.Array.Empty();
	{
		// If a return message was received, get the result
		FScopeLock Lock(&CriticalSection);
		if (QueriedResultReady && QueriedAssets.Num() > 0)
		{
			auto AssetsPtr = QueriedAssets.Find("Blueprint");
			if (AssetsPtr)
			{
				Assets = MoveTemp(*AssetsPtr);
			}
			auto NativeAssetClassesPtr = QueriedAssets.Find("NativeAssetClasses");
			if (AssetsPtr)
			{
				NativeAssetClasses = MoveTemp(*NativeAssetClassesPtr);
			}
		}
		QueriedAssets.Empty();
	}

	return QueriedResultReady;
}

bool FMayaLiveLinkProvider::GetActorsByClass(const FString& ClassName, TMap<FString, FStringArray>& Actors)
{
	if (ClassName.IsEmpty())
	{
		return false;
	}

	QueriedResultReady = false;

	// Request to get the list of actors by class
	auto Message = FMessageEndpoint::MakeMessage<FMayaLiveLinkListActorsRequestMessage>();
	Message->ActorClass = ClassName;

	SendMessage(Message);

	// Wait for the return message or time out if it takes too long
	double Start = FPlatformTime::Seconds();
	while ((FPlatformTime::Seconds() - Start) < 5 && !QueriedResultReady)
	{
		FPlatformProcess::Sleep(0.1f);
	}

	Actors.Empty();
	{
		// If a return message was received, get the result
		FScopeLock Lock(&CriticalSection);
		if (QueriedResultReady && QueriedAssets.Num() > 0)
		{
			Actors = MoveTemp(QueriedAssets);
		}
		QueriedAssets.Empty();
	}

	return QueriedResultReady;
}

bool FMayaLiveLinkProvider::GetAnimSequencesBySkeleton(TMap<FString, FStringArray>& Assets)
{
	QueriedResultReady = false;

	// Request to get the list of anim sequences by skeleton
	auto Message = FMessageEndpoint::MakeMessage<FMayaLiveLinkListAnimSequenceSkeletonRequestMessage>();

	SendMessage(Message);

	// Wait for the return message or time out if it takes too long
	double Start = FPlatformTime::Seconds();
	while ((FPlatformTime::Seconds() - Start) < 5 && !QueriedResultReady)
	{
		FPlatformProcess::Sleep(0.1f);
	}

	Assets.Empty();
	{
		// If a return message was received, get the result
		FScopeLock Lock(&CriticalSection);
		if (QueriedResultReady && QueriedAssets.Num() > 0)
		{
			Assets = MoveTemp(QueriedAssets);
		}
		QueriedAssets.Empty();
	}

	return QueriedResultReady;
}

void FMayaLiveLinkProvider::OnTimeChange(const FQualifiedFrameTime& FrameTime)
{
	// Request to change the time on the Unreal side
	auto Message = FMessageEndpoint::MakeMessage<FMayaLiveLinkTimeChangeRequestMessage>();
	Message->Time = FrameTime;

	SendMessage(Message);
}

void FMayaLiveLinkProvider::HandlePingMessage(const FMayaLiveLinkPingMessage& Message,
											  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (Message.LiveLinkVersion < ILiveLinkClient::LIVELINK_VERSION)
	{
		UE_LOG(LogMayaLiveLinkProvider, Warning,
			   TEXT("A unsupported version of LiveLink is trying to communicate. Requested version: '%d'. Supported version: '%d'."),
			   Message.LiveLinkVersion,
			   ILiveLinkClient::LIVELINK_VERSION);
		return;
	}

	if (Message.MayaLiveLinkVersion != FMayaLiveLinkInterfaceModule::GetPluginVersion())
	{
		UE_LOG(LogMayaLiveLinkProvider, Warning,
			   TEXT("A unsupported version of MayaLiveLink is trying to communicate. Requested version: '%s'. Supported version: '%s'."),
			   *Message.MayaLiveLinkVersion,
			   *FMayaLiveLinkInterfaceModule::GetPluginVersion());
		return;
	}

	SendMessage(FMessageEndpoint::MakeMessage<FMayaLiveLinkPongMessage>(GetProviderName(),
																		GetMachineName(),
																		Message.PollRequest,
																		ILiveLinkClient::LIVELINK_VERSION),
				Context->GetSender());
}

void FMayaLiveLinkProvider::HandleListAssetsReturn(const FMayaLiveLinkListAssetsReturnMessage& Message,
												   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	FScopeLock Lock(&CriticalSection);
	QueriedAssets = Message.AssetsByClass;
	QueriedResultReady = true;
}

void FMayaLiveLinkProvider::HandleListAssetsByParentClassReturn(const FMayaLiveLinkListAssetsByParentClassReturnMessage& Message,
																const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	FScopeLock Lock(&CriticalSection);
	QueriedAssets.Empty();
	QueriedAssets.Add("Blueprint", Message.Assets);
	QueriedAssets.Add("NativeAssetClasses", Message.NativeAssetClasses);
	QueriedResultReady = true;
}

void FMayaLiveLinkProvider::HandleListActorsReturn(const FMayaLiveLinkListActorsReturnMessage& Message,
												   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	FScopeLock Lock(&CriticalSection);
	QueriedAssets = Message.ActorsByClass;
	QueriedResultReady = true;
}

void FMayaLiveLinkProvider::HandleListAnimSequenceSkeletonReturn(const FMayaLiveLinkListAnimSequenceSkeletonReturnMessage& Message,
																 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	FScopeLock Locke(&CriticalSection);
	QueriedAssets = Message.AnimSequencesBySkeleton;
	QueriedResultReady = true;
}

void FMayaLiveLinkProvider::HandleTimeChangeReturn(const FMayaLiveLinkTimeChangeReturnMessage& Message,
												   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	OnTimeChangedReceived.Broadcast(Message.Time);
}
