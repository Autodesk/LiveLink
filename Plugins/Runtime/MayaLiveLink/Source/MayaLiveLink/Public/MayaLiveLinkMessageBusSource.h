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

#include "LiveLinkMessageBusSource.h"

#include "HAL/ThreadSafeBool.h"
#include "IMessageContext.h"
#include "LiveLinkRole.h"
#include "MessageEndpoint.h"
#include "Roles/MayaLiveLinkTimelineTypes.h"

class MAYALIVELINK_API FMayaLiveLinkMessageBusSource : public FLiveLinkMessageBusSource
{
public:
	FMayaLiveLinkMessageBusSource(const FText& InSourceType, const FText& InSourceMachineName, const FMessageAddress& InConnectionAddress, double InMachineTimeOffset);

	//~ Begin ILiveLinkSource interface
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;

	virtual bool RequestSourceShutdown() override;
	//~ End ILiveLinkSource interface

protected:
	virtual const FName& GetSourceName() const override { static FName Name(TEXT("MayaLiveLinkMessageBusSource")); return Name; }
	virtual void InitializeMessageEndpoint(FMessageEndpointBuilder& EndpointBuilder) override;

	virtual void InitializeAndPushStaticData_AnyThread(FName SubjectName,
													   TSubclassOf<ULiveLinkRole> SubjectRole,
													   const FLiveLinkSubjectKey& SubjectKey,
													   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
													   UScriptStruct* MessageTypeInfo) override;
	virtual void InitializeAndPushFrameData_AnyThread(FName SubjectName,
													  const FLiveLinkSubjectKey& SubjectKey,
													  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
													  UScriptStruct* MessageTypeInfo) override;
private:
	//~ Message bus message handlers
	void HandleListAssetsRequest(const struct FMayaLiveLinkListAssetsRequestMessage& Message,
								 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleListAnimSequenceSkeletonRequest(const struct FMayaLiveLinkListAnimSequenceSkeletonRequestMessage& Message,
											   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleListAssetsByParentClassRequest(const struct FMayaLiveLinkListAssetsByParentClassRequestMessage& Message,
											  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleListActorsRequest(const struct FMayaLiveLinkListActorsRequestMessage& Message,
								 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleTimeChangeRequest(const struct FMayaLiveLinkTimeChangeRequestMessage& Message,
								 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleTimeChangeReturn(const FQualifiedFrameTime& Time);
	//~ End Message bus message handlers

	void PushStaticDataToAnimSequence(const FName& SubjectName,
									  TSharedPtr<FLiveLinkStaticDataStruct, ESPMode::ThreadSafe> StaticDataPtr);
	void PushStaticDataToLevelSequence(const FName& SubjectName,
									  TSharedPtr<FLiveLinkStaticDataStruct, ESPMode::ThreadSafe> StaticDataPtr);

private:
	TMap<FName, FMayaLiveLinkAnimSequenceParams> SubjectTimelineParams;
	TMap<FName, FMayaLiveLinkLevelSequenceParams> SubjectLevelSequenceParams;

	// Lock to stop multiple threads accessing the Subjects from the collection at the same time
	FCriticalSection SubjectTimelineParamsCriticalSection;
};