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

#include "MayaLiveLinkPresenceDetector.h"

#include "ILiveLinkClient.h"
#include "LiveLinkSettings.h"
#include "MessageEndpointBuilder.h"

#include "HAL/RunnableThread.h"

#include "Misc/ScopeLock.h"

FMayaLiveLinkPresenceDetector::FMayaLiveLinkPresenceDetector()
: Thread(nullptr)
, bIsRunning(true)
, NumberRequests(0)
{
	PingRequestFrequency = GetDefault<ULiveLinkSettings>()->GetMessageBusPingRequestFrequency();

	MessageEndpoint = FMessageEndpoint::Builder(TEXT("MayaLiveLinkPresenceDetector"))
							.Handling<FMayaLiveLinkPongMessage>(this, &FMayaLiveLinkPresenceDetector::HandlePongMessage);

	bIsRunning = MessageEndpoint.IsValid();
	if (bIsRunning)
	{
		// Create the runnable thread
		Thread = FRunnableThread::Create(this, TEXT("MayaLiveLinkPresenceDetector"));
	}
}

FMayaLiveLinkPresenceDetector::~FMayaLiveLinkPresenceDetector()
{
	{
		FScopeLock Lock(&CriticalSection);
		
		// Reset the message end point
		if (MessageEndpoint)
		{
			MessageEndpoint->Disable();
			MessageEndpoint.Reset();
		}
	}

	Stop();

	// Kill the runnable thread
	if (Thread)
	{
		Thread->Kill(true);
		Thread = nullptr;
	}
}

uint32 FMayaLiveLinkPresenceDetector::Run()
{
	while (bIsRunning)
	{
		{
			FScopeLock Lock(&CriticalSection);

			if (NumberRequests > 0)
			{
				PollResults.Reset();
				PingId = FGuid::NewGuid();
				MessageEndpoint->Publish(FMessageEndpoint::MakeMessage<FMayaLiveLinkPingMessage>(PingId, ILiveLinkClient::LIVELINK_VERSION));
			}
		}

		// Wait before sending another message
		FPlatformProcess::Sleep(PingRequestFrequency);
	}
	return 0;
}

void FMayaLiveLinkPresenceDetector::Stop()
{
	bIsRunning = false;
}

void FMayaLiveLinkPresenceDetector::AddPresenceRequest()
{
	FScopeLock Lock(&CriticalSection);
	if (++NumberRequests == 1)
	{
		PollResults.Reset();
	}
}

void FMayaLiveLinkPresenceDetector::RemovePresenceRequest()
{
	--NumberRequests;
}

 void FMayaLiveLinkPresenceDetector::GetResults(TArray<FProviderPollResultPtr>& Results)
{
	FScopeLock Lock(&CriticalSection);
	Results = PollResults;
}

void FMayaLiveLinkPresenceDetector::HandlePongMessage(const FMayaLiveLinkPongMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	FScopeLock Lock(&CriticalSection);

	if (Message.PollRequest == PingId)
	{
		PollResults.Emplace(MakeShared<FProviderPollResult, ESPMode::ThreadSafe>(Context->GetSender(),
																				 Message.ProviderName,
																				 Message.MachineName,
																				 0,
																				 true));
	}
}
