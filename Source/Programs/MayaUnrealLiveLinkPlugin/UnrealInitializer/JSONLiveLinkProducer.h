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

// Network/socket
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#elif PLATFORM_LINUX
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <memory.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <errno.h>
#include <stdlib.h>
#endif

// Try to include RapidJSON headers otherwise include JSon from Unreal.
#ifndef __has_include
    #define __has_include(F) 0
    #define JSON__has_include_DEFINED
#endif
#if __has_include(<rapidjson/stringbuffer.h>)
	#pragma message("Using RapidJSON")
	THIRD_PARTY_INCLUDES_START
	#include <rapidjson/stringbuffer.h>
	#include <rapidjson/writer.h>
	THIRD_PARTY_INCLUDES_END
#else
	#include "Json.h"
#endif
#ifdef JSON__has_include_DEFINED
    #undef JSON__has_include_DEFINED
#endif

struct FIPv4Endpoint;
struct FLiveLinkSkeletonStaticData;
struct FLiveLinkAnimationFrameData;
struct FLiveLinkCameraStaticData;
struct FLiveLinkCameraFrameData;
struct FLiveLinkLightStaticData;
struct FLiveLinkLightFrameData;
struct FLiveLinkTransformStaticData;
struct FLiveLinkTransformFrameData;

// Static Subject data that the application has told us about
struct FTrackedStaticData
{
	FTrackedStaticData()
		: SubjectName(NAME_None)
	{}
	FTrackedStaticData(FName InSubjectName, TWeakObjectPtr<UClass> InRoleClass, FLiveLinkStaticDataStruct InStaticData)
		: SubjectName(InSubjectName), RoleClass(InRoleClass), StaticData(MoveTemp(InStaticData))
	{}
	FName SubjectName;
	TWeakObjectPtr<UClass> RoleClass;
	FLiveLinkStaticDataStruct StaticData;
	bool operator==(FName InSubjectName) const { return SubjectName == InSubjectName; }
};


class FJSONLiveLinkProducer : public ILiveLinkProducer
{
public:
	FJSONLiveLinkProducer(const FString& ProviderName);

	virtual ~FJSONLiveLinkProducer();

	LiveLinkSource GetSourceType() const override final
	{
		return LiveLinkSource::JSON;
	}

	bool Connect(const FIPv4Endpoint& InEndpoint);

	void CloseConnection();

	/**
	* Send, to UE4, the static data of a subject.
	* @param SubjectName	The name of the subject
	* @param Role			The Live Link role of the subject. The StaticData type should match the role's data.
	* @param StaticData	The static data of the subject.
	The FLiveLinkStaticDataStruct doesn't have a copy constructor.
	*						The argument is passed by r-value to help the user understand the compiler error message.
	* @return				True if the message was sent or is pending an active connection.
	* @see					UpdateSubjectFrameData, RemoveSubject
	*/
	virtual bool UpdateSubjectStaticData(const FName& SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& StaticData) override;

	/**
	* Inform UE4 that a subject won't be streamed anymore.
	* @param SubjectName	The name of the subject.
	*/
	virtual void RemoveSubject(const FName& SubjectName) override;

	/**
	* Send the static data of a subject to UE4.
	* @param SubjectName	The name of the subject
	* @param StaticData	The frame data of the subject. The type should match the role's data send with UpdateSubjectStaticData.
	The FLiveLinkFrameDataStruct doesn't have a copy constructor.
	*						The argument is passed by r-value to help the user understand the compiler error message.
	* @return				True if the message was sent or is pending an active connection.
	* @see					UpdateSubjectStaticData, RemoveSubject
	*/
	virtual bool UpdateSubjectFrameData(const FName& SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkFrameDataStruct&& FrameData) override;

	/** Is this provider currently connected to something. */
	virtual bool HasConnection() const override;

	/** Function for managing connection status changed delegate. */
	virtual FDelegateHandle RegisterConnStatusChangedHandle(const FMayaLiveLinkProviderConnectionStatusChanged::FDelegate& ConnStatusChanged) override;

	/** Function for managing connection status changed delegate. */
	void UnregisterConnStatusChangedHandle(FDelegateHandle Handle);

	// Get the cached data for the named subject
	template<typename T>
	T* GetLastSubjectStaticData(const FName& SubjectName);

	virtual void EnableFileExport(bool Enable, const FString& FilePath = FString()) override final;

private:
	void ResetWriter();

	bool SendStringBuffer();

	void ClearTrackedSubject(const FName& SubjectName);
	void SetLastSubjectStaticData(const FName& SubjectName, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& StaticData);

	void UpdateSubjectStaticData(const FName& SubjectName, FLiveLinkSkeletonStaticData& StaticData);
	void UpdateSubjectFrameData(const FName& SubjectName, const FLiveLinkAnimationFrameData& FrameData);

	void UpdateSubjectStaticData(const FName& SubjectName, FLiveLinkCameraStaticData& StaticData);
	void UpdateSubjectFrameData(const FName& SubjectName, const FLiveLinkCameraFrameData& FrameData);

	void UpdateSubjectStaticData(const FName& SubjectName, FLiveLinkLightStaticData& StaticData);
	void UpdateSubjectFrameData(const FName& SubjectName, const FLiveLinkLightFrameData& FrameData);

	void UpdateSubjectStaticData(const FName& SubjectName, FLiveLinkTransformStaticData& StaticData);
	void UpdateSubjectFrameData(const FName& SubjectName, const FLiveLinkTransformFrameData& FrameData);

	void UpdateSubjectStaticData(const FName& SubjectName, struct FMayaLiveLinkAnimSequenceStaticData& StaticData);
	void UpdateSubjectFrameData(const FName& SubjectName, const struct FMayaLiveLinkAnimSequenceFrameData& FrameData);

	// JSON Writer helper functions
	void WriteKey(const char* KeyName, bool Supported, bool Value);
	void WriteKey(const char* KeyName, bool Supported, int Value);
	void WriteKey(const char* KeyName, bool Supported, double Value);
	void WriteKey(const char* KeyName, bool Supported, const FString& Value);
	void WriteKey(const char* KeyName, bool Supported, const FVector& Value);
	void WriteKey(const char* KeyName, bool Supported, const FQuat& Value);
	void WriteKey(const char* KeyName, bool Supported, const FColor& Value);
	void WriteKey(const char* KeyName, bool Supported, const TArray<FName>& Value);
	void WriteKey(const char* KeyName, bool Supported, const TArray<float>& Value);

	void WriteLocation(const FVector& Location, bool isLocationSupported = true);
	void WriteRotation(const FQuat& Rotation, bool isRotationSupported = true);
	void WriteScale(const FVector& Scale, bool isScaleSupported = false);
	void WriteTransform(const FTransform& transform,
						bool isLocationSupported = true,
						bool isRotationSupported = true,
						bool isScaleSupported = false);

	void WriteTransform(const FTransform& transform, const FLiveLinkTransformStaticData& StaticData);

	void WriteTransformStaticDataKeys(const FLiveLinkTransformStaticData& StaticData);
	void WriteTransformFrameDataKeys(const FLiveLinkTransformFrameData& FrameData,
									 const FLiveLinkTransformStaticData& StaticData);

	void StartWriterStaticData(const FName& SubjectName, const char* RoleName);
	void EndWriterStaticData();

	void StartWriterFrameData(const FName& SubjectName, const char* RoleName, double SceneTimeInSeconds);
	void EndWriterFrameData();

private:
	// Delegate to notify interested parties when the client sources have changed
	FMayaLiveLinkProviderConnectionStatusChanged OnConnectionStatusChanged;

#if PLATFORM_WINDOWS
	SOCKET Socket;
#else
	int Socket;
#endif
	sockaddr_storage AddrDest = {};
	bool SendSuccess = true;

#ifdef RAPIDJSON_VERSION_STRING
	rapidjson::StringBuffer StringBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> Writer;
#else
	template <class PrintPolicy = TPrettyJsonPrintPolicy<TCHAR>>
	class TJsonBytesWriter
		: public TJsonWriter<TCHAR, PrintPolicy>
	{
	public:

		virtual ~TJsonBytesWriter()
		{
			check(this->Stream->Close());
			delete this->Stream;
		}

		void Reset(TArray<uint8>& InBytes)
		{
			this->Stream->Reset();
			this->Stream->Seek(0);
			this->Stack.Reset();
			this->PreviousTokenWritten = EJsonToken::None;
			this->IndentLevel = 0;
			Bytes.Reset();
		}

		TJsonBytesWriter( TArray<uint8>& InBytes, int32 InitialIndent=0 )
			: TJsonWriter<TCHAR, PrintPolicy>(new FMemoryWriter(InBytes), InitialIndent)
			, Bytes(InBytes)
		{ }

	private:

		TArray<uint8>& Bytes;
	};

	TArray<uint8> StringBuffer;
	TJsonBytesWriter<TCondensedJsonPrintPolicy<TCHAR>> Writer;
#endif

	// Cache of our current subject state
 	TArray<FTrackedStaticData> StaticDatas;

	bool FileExport = false;
	FString FileExportPath;
};
