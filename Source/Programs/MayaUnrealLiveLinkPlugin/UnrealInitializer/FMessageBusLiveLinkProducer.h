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

#include "ILiveLinkProducer.h"

class FMessageBusLiveLinkProducer : public ILiveLinkProducer
{
public:
	FMessageBusLiveLinkProducer(const FString& ProviderName)
	: LiveLinkProvider(ILiveLinkProvider::CreateLiveLinkProvider(ProviderName))
	{
	}

	virtual ~FMessageBusLiveLinkProducer()
	{
		LiveLinkProvider.Reset();
	}

	LiveLinkSource GetSourceType() const override final
	{
		return LiveLinkSource::MessageBus;
	}

	/**
	* Send, to UE4, the static data of a subject.
	* @param SubjectName	The name of the subject
	* @param Role			The Live Link role of the subject. The StaticData type should match the role's data.
	* @param StaticData	    The valid static data of the subject. See FUnrealStreamManager::InitializeAndGetStaticData.
	The FLiveLinkStaticDataStruct doesn't have a copy constructor.
	*						The argument is passed by r-value to help the user understand the compiler error message.
	* @return				True if the message was sent or is pending an active connection.
	* @see					UpdateSubjectFrameData, RemoveSubject
	*/
	virtual bool UpdateSubjectStaticData(const FName& SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& StaticData) override
	{
		return LiveLinkProvider->UpdateSubjectStaticData(SubjectName, Role, MoveTemp(StaticData));
	}

	/**
	* Inform UE4 that a subject won't be streamed anymore.
	* @param SubjectName	The name of the subject.
	*/
	virtual void RemoveSubject(const FName& SubjectName) override
	{
		LiveLinkProvider->RemoveSubject(SubjectName);
	}

	/**
	* Send the static data of a subject to UE4.
	* @param SubjectName	The name of the subject
	* @param StaticData	The frame data of the subject. The type should match the role's data send with UpdateSubjectStaticData.
	The FLiveLinkFrameDataStruct doesn't have a copy constructor.
	*						The argument is passed by r-value to help the user understand the compiler error message.
	* @return				True if the message was sent or is pending an active connection.
	* @see					UpdateSubjectStaticData, RemoveSubject
	*/
	virtual bool UpdateSubjectFrameData(const FName& SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkFrameDataStruct&& FrameData) override
	{
		return LiveLinkProvider->UpdateSubjectFrameData(SubjectName, MoveTemp(FrameData));
	}

	/** Is this provider currently connected to something. */
	virtual bool HasConnection() const override
	{
		return LiveLinkProvider->HasConnection();
	}

	/** Function for managing connection status changed delegate. */
	virtual FDelegateHandle RegisterConnStatusChangedHandle(const FLiveLinkProviderConnectionStatusChanged::FDelegate& ConnStatusChanged) override
	{
		return LiveLinkProvider->RegisterConnStatusChangedHandle(ConnStatusChanged);
	}

	/** Function for managing connection status changed delegate. */
	virtual void UnregisterConnStatusChangedHandle(FDelegateHandle Handle) override
	{
		LiveLinkProvider->UnregisterConnStatusChangedHandle(Handle);
	}

	virtual void EnableFileExport(bool Enable, const FString& FilePath = FString()) override final
	{
		/*std::cerr << "Unsupported functionality" << std::endl;*/
	}

private:
	TSharedPtr<ILiveLinkProvider> LiveLinkProvider;
};
