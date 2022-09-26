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

#include "FMessageBusLiveLinkProducer.h"

#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "MayaLiveLinkMessageInterceptor.h"
#include "MayaLiveLinkMessages.h"

FMessageBusLiveLinkProducer::FMessageBusLiveLinkProducer(const FString& ProviderName)
{
	FMessageEndpointBuilder EndpointBuilder(*ProviderName);
	EndpointBuilder
		.Handling<FLiveLinkConnectMessage>(this, &FMessageBusLiveLinkProducer::HandleConnectMessage)
		.Handling<FMayaLiveLinkPingMessage>(this, &FMessageBusLiveLinkProducer::HandlePingMessage)
		.Handling<FMayaLiveLinkSourceShutdownMessage>(this, &FMessageBusLiveLinkProducer::HandleSourceShutdown)
		.Handling<FMayaLiveLinkListAssetsReturnMessage>(this, &FMessageBusLiveLinkProducer::HandleListAssetsReturn)
		.Handling<FMayaLiveLinkListAssetsByParentClassReturnMessage>(this, &FMessageBusLiveLinkProducer::HandleListAssetsByParentClassReturn)
		.Handling<FMayaLiveLinkListActorsReturnMessage>(this, &FMessageBusLiveLinkProducer::HandleListActorsReturn)
		.Handling<FMayaLiveLinkListAnimSequenceSkeletonReturnMessage>(this, &FMessageBusLiveLinkProducer::HandleListAnimSequenceSkeletonReturn)
		.Handling<FMayaLiveLinkTimeChangeReturnMessage>(this, &FMessageBusLiveLinkProducer::HandleTimeChangeReturn);

	TSharedPtr<ILiveLinkProvider> Provider = ILiveLinkProvider::CreateLiveLinkProvider<FMayaLiveLinkProvider>(ProviderName, MoveTemp(EndpointBuilder));
	LiveLinkProvider = StaticCastSharedPtr<FMayaLiveLinkProvider>(Provider);
	LiveLinkProvider->Subscribe<FMayaLiveLinkPingMessage>();

	Interceptor = MakeShared<FMayaLiveLinkMessageInterceptor, ESPMode::ThreadSafe>();
	TSharedPtr<IMessageBus, ESPMode::ThreadSafe> Bus = IMessagingModule::Get().GetDefaultBus();
	Bus->Intercept(Interceptor->AsShared(), FTopLevelAssetPath(FLiveLinkPingMessage::StaticStruct()->GetPathName()));
}

FMessageBusLiveLinkProducer::~FMessageBusLiveLinkProducer()
{
	auto Bus = IMessagingModule::Get().GetDefaultBus();
	if (Interceptor)
	{
		if (Interceptor->GetSenderAddress().IsValid())
		{
			Bus->Unregister(Interceptor->GetSenderAddress());
		}
		Interceptor.Reset();
	}

	LiveLinkProvider.Reset();
}

void FMessageBusLiveLinkProducer::HandleConnectMessage(const FLiveLinkConnectMessage& Message,
													   const TSharedRef<class IMessageContext, ESPMode::ThreadSafe>& Context)
{
	LiveLinkProvider->ResetSourceShutdown();
}

void FMessageBusLiveLinkProducer::HandlePingMessage(const FMayaLiveLinkPingMessage& Message,
													const TSharedRef<class IMessageContext, ESPMode::ThreadSafe>& Context)
{
	LiveLinkProvider->HandlePingMessage(Message, Context);
}

void FMessageBusLiveLinkProducer::HandleSourceShutdown(const FMayaLiveLinkSourceShutdownMessage& Message,
													   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	LiveLinkProvider->HandleSourceShutdown();
}

void FMessageBusLiveLinkProducer::HandleListAssetsReturn(const FMayaLiveLinkListAssetsReturnMessage& Message,
														 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	LiveLinkProvider->HandleListAssetsReturn(Message, Context);
}

void FMessageBusLiveLinkProducer::HandleListAssetsByParentClassReturn(const FMayaLiveLinkListAssetsByParentClassReturnMessage& Message,
																	  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	LiveLinkProvider->HandleListAssetsByParentClassReturn(Message, Context);
}

void FMessageBusLiveLinkProducer::HandleListActorsReturn(const FMayaLiveLinkListActorsReturnMessage& Message,
														 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	LiveLinkProvider->HandleListActorsReturn(Message, Context);
}

void FMessageBusLiveLinkProducer::HandleListAnimSequenceSkeletonReturn(const FMayaLiveLinkListAnimSequenceSkeletonReturnMessage& Message,
																	   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	LiveLinkProvider->HandleListAnimSequenceSkeletonReturn(Message, Context);
}

void FMessageBusLiveLinkProducer::HandleTimeChangeReturn(const FMayaLiveLinkTimeChangeReturnMessage& Message,
														 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	LiveLinkProvider->HandleTimeChangeReturn(Message, Context);
}
