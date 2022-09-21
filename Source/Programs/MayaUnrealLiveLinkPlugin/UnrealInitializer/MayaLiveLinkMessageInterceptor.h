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

#include "IMessageInterceptor.h"

class FMayaLiveLinkMessageInterceptor : public TSharedFromThis<FMayaLiveLinkMessageInterceptor, ESPMode::ThreadSafe>
									  , public IMessageInterceptor
									  , public IMessageSender
{
public:
	FMayaLiveLinkMessageInterceptor();

	// IMessageInterceptor interface
	virtual FName GetDebugName() const override;
	virtual const FGuid& GetInterceptorId() const override;
	virtual bool InterceptMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context) override;

	// IMessageSender interface
	virtual FMessageAddress GetSenderAddress() override;
	virtual void NotifyMessageError(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, const FString& Error) override {}

	void EnableIntercepting(bool bIntercept) { bIsIntercepting = bIntercept; }

private:
	bool bIsIntercepting;

	FGuid InterceptorId;
	FMessageAddress Address;

	TSharedPtr<IMessageBus, ESPMode::ThreadSafe> InterceptedBus;
};
