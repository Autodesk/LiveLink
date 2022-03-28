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

#include "LiveLinkProvider.h"
#include "LiveLinkTypes.h"

enum LiveLinkSource
{
	MessageBus,
	JSON,

	NumberOfSources
};

const char* const LiveLinkSourceNames[LiveLinkSource::NumberOfSources] =
{
	"MessageBus",
	"JSON",
};

class ILiveLinkProducer
{
public:
	virtual LiveLinkSource GetSourceType() const = 0;

	const char* GetSourceName() const
	{
		static_assert(LiveLinkSource::NumberOfSources == sizeof(LiveLinkSourceNames) / sizeof(LiveLinkSourceNames[0]),
					  "LiveLinkSourceNames size doesn't match the number of sources in LiveLinkSource");
		return LiveLinkSourceNames[GetSourceType()];
	}

	/**
	* Send, to UE4, the static data of a subject.
	* @param SubjectName	The name of the subject
	* @param Role			The Live Link role of the subject. The StaticData type should match the role's data.
	* @param StaticData		The static data of the subject.
							The FLiveLinkStaticDataStruct doesn't have a copy constructor.
	*						The argument is passed by r-value to help the user understand the compiler error message.
	* @return				True if the message was sent or is pending an active connection.
	* @see					UpdateSubjectFrameData, RemoveSubject
	*/
	virtual bool UpdateSubjectStaticData(const FName& SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& StaticData) = 0;

	/**
	* Inform UE4 that a subject won't be streamed anymore.
	* @param SubjectName	The name of the subject.
	*/
	virtual void RemoveSubject(const FName& SubjectName) = 0;

	/**
	* Send the static data of a subject to UE4.
	* @param SubjectName	The name of the subject
	* @param StaticData		The frame data of the subject. The type should match the role's data send with UpdateSubjectStaticData.
	*						The FLiveLinkFrameDataStruct doesn't have a copy constructor.
	*						The argument is passed by r-value to help the user understand the compiler error message.
	* @return				True if the message was sent or is pending an active connection.
	* @see					UpdateSubjectStaticData, RemoveSubject
	*/
	virtual bool UpdateSubjectFrameData(const FName& SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkFrameDataStruct&& FrameData) = 0;

	/** Is this provider currently connected to something. */
	virtual bool HasConnection() const = 0;

	/** Function for managing connection status changed delegate. */
	virtual FDelegateHandle RegisterConnStatusChangedHandle(const FLiveLinkProviderConnectionStatusChanged::FDelegate& ConnStatusChanged) = 0;

	/** Function for managing connection status changed delegate. */
	virtual void UnregisterConnStatusChangedHandle(FDelegateHandle Handle) = 0;

	virtual void EnableFileExport(bool Enable, const FString& FilePath = FString()) = 0;
};