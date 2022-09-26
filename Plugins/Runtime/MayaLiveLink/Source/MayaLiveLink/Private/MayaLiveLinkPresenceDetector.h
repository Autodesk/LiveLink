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

#include "LiveLinkMessageBusFinder.h"
#include "MayaLiveLinkMessages.h"
#include "MessageEndpoint.h"

#include "HAL/CriticalSection.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"

#include "Templates/Atomic.h"

// A class to detect Maya live link sources
class FMayaLiveLinkPresenceDetector : public FRunnable
{
public:
	FMayaLiveLinkPresenceDetector();
	virtual ~FMayaLiveLinkPresenceDetector();

	virtual uint32 Run() override;
	virtual void Stop() override;

	bool IsRunning() const { return bIsRunning; }

	void AddPresenceRequest();
	void RemovePresenceRequest();
	void GetResults(TArray<FProviderPollResultPtr>& Results);

private:
	void HandlePongMessage(const struct FMayaLiveLinkPongMessage& Message,
						   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

private:
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	class FRunnableThread* Thread;
	FThreadSafeBool bIsRunning;

	FCriticalSection CriticalSection;
	TAtomic<int32> NumberRequests;

	TArray<FProviderPollResultPtr> PollResults;

	double PingRequestFrequency;
	FGuid PingId;
};