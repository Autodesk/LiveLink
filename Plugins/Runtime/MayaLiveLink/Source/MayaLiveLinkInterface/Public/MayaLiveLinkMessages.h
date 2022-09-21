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

#include "LiveLinkMessages.h"

#include "UObject/Object.h"

#include "MayaLiveLinkMessages.generated.h"

USTRUCT()
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkPingMessage : public FLiveLinkPingMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString MayaLiveLinkVersion;

	UPROPERTY()
	FString UnrealVersion;

	// default constructor for the receiver
	FMayaLiveLinkPingMessage();

	FMayaLiveLinkPingMessage(const FGuid& InPollRequest, int32 InLiveLinkVersion);
};

USTRUCT()
struct MAYALIVELINKINTERFACE_API FMayaLiveLinkPongMessage : public FLiveLinkPongMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString MayaLiveLinkVersion;

	UPROPERTY()
	FString UnrealVersion;

	// default constructor for the receiver
	FMayaLiveLinkPongMessage();

	FMayaLiveLinkPongMessage(const FString& InProviderName, const FString& InMachineName, const FGuid& InPollRequest, int32 InLiveLinkVersion);
};

USTRUCT()
struct FMayaLiveLinkConnectMessage
{
	GENERATED_BODY()

	UPROPERTY()
	int32 LiveLinkVersion = 1;

	UPROPERTY()
	FString MayaLiveLinkVersion;

	UPROPERTY()
	FString UnrealVersion;
};

USTRUCT()
struct FMayaLiveLinkHeartbeatMessage
{
	GENERATED_BODY()
};

USTRUCT()
struct FMayaLiveLinkClearSubject
{
	GENERATED_BODY()

	// Name of the subject to clear
	UPROPERTY()
	FName SubjectName;

	FMayaLiveLinkClearSubject() {}
	FMayaLiveLinkClearSubject(const FName& InSubjectName) : SubjectName(InSubjectName) {}
};
// #endif

USTRUCT()
struct FMayaLiveLinkSourceShutdownMessage
{
	GENERATED_BODY()
};

USTRUCT()
struct FStringArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> Array;
};

USTRUCT()
struct FMayaLiveLinkListAssetsRequestMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString AssetClass;

	UPROPERTY()
	bool SearchSubClasses = false;
};

USTRUCT()
struct FMayaLiveLinkListAssetsReturnMessage
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FString, FStringArray> AssetsByClass;
};

USTRUCT()
struct FMayaLiveLinkListAnimSequenceSkeletonRequestMessage
{
	GENERATED_BODY()
};

USTRUCT()
struct FMayaLiveLinkListAnimSequenceSkeletonReturnMessage
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FString, FStringArray> AnimSequencesBySkeleton;
};

USTRUCT()
struct FMayaLiveLinkListAssetsByParentClassRequestMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString AssetClass;

	UPROPERTY()
	bool SearchSubClasses = false;

	UPROPERTY()
	TArray<FString> ParentClasses;

	UPROPERTY()
	TArray<FString> NativeClasses;
};

USTRUCT()
struct FMayaLiveLinkListAssetsByParentClassReturnMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FStringArray Assets;

	UPROPERTY()
	FStringArray NativeAssetClasses;
};

USTRUCT()
struct FMayaLiveLinkListActorsRequestMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString ActorClass;
};

USTRUCT()
struct FMayaLiveLinkListActorsReturnMessage
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FString, FStringArray> ActorsByClass;
};

USTRUCT()
struct FMayaLiveLinkTimeChangeRequestMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FQualifiedFrameTime Time;
};

USTRUCT()
struct FMayaLiveLinkTimeChangeReturnMessage : public FMayaLiveLinkTimeChangeRequestMessage
{
	GENERATED_BODY()
};