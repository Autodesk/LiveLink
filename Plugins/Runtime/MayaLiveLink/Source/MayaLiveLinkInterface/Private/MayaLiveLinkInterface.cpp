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

#include "MayaLiveLinkInterface.h"

#include "UObject/ObjectMacros.h"

#include "Runtime/Launch/Resources/Version.h"

IMPLEMENT_MODULE(FMayaLiveLinkInterfaceModule, MayaLiveLinkInterface)

#define LOCTEXT_NAMESPACE "FMayaLiveLinkInterfaceModule"

namespace
{
	static const FString PluginVersion = "v2.0.1";
	static const FString UnrealEngineVersion = VERSION_STRINGIFY(ENGINE_MAJOR_VERSION) TEXT(".") VERSION_STRINGIFY(ENGINE_MINOR_VERSION);
}

void FMayaLiveLinkInterfaceModule::StartupModule()
{
}

void FMayaLiveLinkInterfaceModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

const FString& FMayaLiveLinkInterfaceModule::GetPluginVersion()
{
	return PluginVersion;
}

const FString& FMayaLiveLinkInterfaceModule::GetEngineVersion()
{
	return UnrealEngineVersion;
}

#undef LOCTEXT_NAMESPACE
