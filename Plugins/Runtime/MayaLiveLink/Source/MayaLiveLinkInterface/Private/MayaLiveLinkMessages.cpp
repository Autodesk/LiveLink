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

#include "MayaLiveLinkMessages.h"
#include "MayaLiveLinkInterface.h"

FMayaLiveLinkPingMessage::FMayaLiveLinkPingMessage()
: MayaLiveLinkVersion(FMayaLiveLinkInterfaceModule::GetPluginVersion())
, UnrealVersion(FMayaLiveLinkInterfaceModule::GetEngineVersion())
{
}

FMayaLiveLinkPingMessage::FMayaLiveLinkPingMessage(const FGuid& InPollRequest, int32 InLiveLinkVersion)
: FLiveLinkPingMessage(InPollRequest, InLiveLinkVersion)
, MayaLiveLinkVersion(FMayaLiveLinkInterfaceModule::GetPluginVersion())
, UnrealVersion(FMayaLiveLinkInterfaceModule::GetEngineVersion())
{
}

FMayaLiveLinkPongMessage::FMayaLiveLinkPongMessage()
: MayaLiveLinkVersion(FMayaLiveLinkInterfaceModule::GetPluginVersion())
, UnrealVersion(FMayaLiveLinkInterfaceModule::GetEngineVersion())
{
}

FMayaLiveLinkPongMessage::FMayaLiveLinkPongMessage(const FString& InProviderName, const FString& InMachineName, const FGuid& InPollRequest, int32 InLiveLinkVersion)
: FLiveLinkPongMessage(InProviderName, InMachineName, InPollRequest, InLiveLinkVersion)
, MayaLiveLinkVersion(FMayaLiveLinkInterfaceModule::GetPluginVersion())
, UnrealVersion(FMayaLiveLinkInterfaceModule::GetEngineVersion())
{
}
