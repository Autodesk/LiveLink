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
#include "Templates/SharedPointer.h"
#include "LiveLinkRole.h"
#include "LiveLinkTypes.h"
#include "MayaLiveLinkMessages.h"

/** Delegate called when the connection status of the provider has changed. */
DECLARE_MULTICAST_DELEGATE(FMayaLiveLinkProviderConnectionStatusChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FMayaLiveLinkProviderTimeChangedReceived, const FQualifiedFrameTime&);

#include "LiveLinkProviderImpl.h"

class FMayaLiveLinkProvider : public FLiveLinkProvider
{
public:
	FMayaLiveLinkProvider(const FString& InProviderName, FMessageEndpointBuilder&& EndpointBuilder);

	virtual bool HasConnection() const override
	{
		{
			FScopeLock Lock(&CriticalSection);
			if (SourceShutDown)
			{
				return false;
			}
		}

		return FLiveLinkProvider::HasConnection();
	}

	virtual FDelegateHandle RegisterConnStatusChangedHandle(const FMayaLiveLinkProviderConnectionStatusChanged::FDelegate& ConnStatusChanged) override
	{
		FLiveLinkProvider::RegisterConnStatusChangedHandle(ConnStatusChanged);
		return OnConnectionStatusChanged.Add(ConnStatusChanged);
	}

	virtual void UnregisterConnStatusChangedHandle(FDelegateHandle Handle) override
	{
		FLiveLinkProvider::UnregisterConnStatusChangedHandle(Handle);
		OnConnectionStatusChanged.Remove(Handle);
	}

	FDelegateHandle RegisterTimeChangedReceived(const FMayaLiveLinkProviderTimeChangedReceived::FDelegate& TimeChangedReceived)
	{
		return OnTimeChangedReceived.Add(TimeChangedReceived);
	}

	void UnregisterTimeChangedReceived(const FDelegateHandle& TimeChangedReceivedHandle)
	{
		OnTimeChangedReceived.Remove(TimeChangedReceivedHandle);
	}

	bool GetAssetsByClass(const FString& ClassName,
						  bool bSearchSubClasses,
						  TMap<FString, FStringArray>& Assets);
	bool GetAssetsByParentClass(const FString& ClassName,
								bool bSearchSubClasses,
								const TArray<FString>& ParentClasses,
								FStringArray& Assets,
								FStringArray& NativeAssetClasses);
	bool GetActorsByClass(const FString& ClassName,
						  TMap<FString, FStringArray>& Assets);
	bool GetAnimSequencesBySkeleton(TMap<FString, FStringArray>& Assets);

	void OnTimeChange(const FQualifiedFrameTime& FrameTime);

private:
	void ResetSourceShutdown()
	{
		FScopeLock Lock(&CriticalSection);
		SourceShutDown = false;
	}

	void HandleSourceShutdown()
	{
		FScopeLock Lock(&CriticalSection);
		SourceShutDown = true;

		OnConnectionStatusChanged.Broadcast();
	}

	void HandlePingMessage(const FMayaLiveLinkPingMessage& Message,
						   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleListAssetsReturn(const FMayaLiveLinkListAssetsReturnMessage& Message,
								const TSharedRef<class IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleListAssetsByParentClassReturn(const FMayaLiveLinkListAssetsByParentClassReturnMessage& Message,
											 const TSharedRef<class IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleListActorsReturn(const FMayaLiveLinkListActorsReturnMessage& Message,
								const TSharedRef<class IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleListAnimSequenceSkeletonReturn(const FMayaLiveLinkListAnimSequenceSkeletonReturnMessage& Message,
											  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleTimeChangeReturn(const FMayaLiveLinkTimeChangeReturnMessage& Message,
								const TSharedRef<class IMessageContext, ESPMode::ThreadSafe>& Context);
private:
	bool SourceShutDown = false;

	mutable FCriticalSection CriticalSection;

	// Delegate to notify interested parties when the client sources have changed
	FMayaLiveLinkProviderConnectionStatusChanged OnConnectionStatusChanged;

	FMayaLiveLinkProviderTimeChangedReceived OnTimeChangedReceived;

	// Query results
	TMap<FString, FStringArray> QueriedAssets;
	bool QueriedResultReady = false;

	friend class FMessageBusLiveLinkProducer;
};
