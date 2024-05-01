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

#include "UnrealInitializer.h"
#include "LaunchEngineLoop.h"
#include "Async/TaskGraphInterfaces.h"
#include "Features/IModularFeatures.h"
#include "INetworkMessagingExtension.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Modules/ModuleManager.h"
#include "Shared/UdpMessagingSettings.h"
#include "UObject/Object.h"
#include "Misc/ConfigCacheIni.h"

#include "FMessageBusLiveLinkProducer.h"
#include "JSONLiveLinkProducer.h"
#include "FUnrealStreamManager.h"
#include "FMayaOutputDevice.h"

//======================================================================
/*!	\brief Get the singleton Unreal Initializer object

	\return The singleton Unreal Initializer object
*/
UnrealInitializer& UnrealInitializer::TheOne()
{
	static UnrealInitializer SUnrealInitializer;
	return SUnrealInitializer;
}

//======================================================================
/*!	\brief Check if Live Link was initialized before.

	\return True if Live Link has been initialized before at least once.
*/
bool UnrealInitializer::HasInitializedOnce() const
{
	return InitializedOnce;
}

//======================================================================
/*!	\brief Default private constructor
*/
UnrealInitializer::UnrealInitializer()
: InitializedOnce(false)
{
}

//======================================================================
/*!	\brief Initialize Unreal and set it up for live link
*/
void UnrealInitializer::InitializeUnreal()
{
	GEngineLoop.PreInit(TEXT("MayaUnrealLiveLinkPlugin -Messaging -stdout"));
	ProcessNewlyLoadedUObjects();

	// Tell the module manager is may now process newly-loaded UObjects when new C++ modules are loaded
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	// Load UdpMessaging module needed by message bus.
	FModuleManager::Get().LoadModule(TEXT("UdpMessaging"));

	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault);
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::Default);
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PostDefault);

	InitializedOnce = true;
}

//======================================================================
/*!	\brief	Creates and adds a new output device in UE to print logs.

	\param[in] Callback Function pointer to function that prints to Maya.
*/
void UnrealInitializer::AddMayaOutput(PrintToMayaCb Callback)
{
	if (GLog)
	{
		GLog->SetCurrentThreadAsPrimaryThread();
		GLog->TearDown(); //clean up existing output devices
		GLog->AddOutputDevice(new FMayaOutputDevice(Callback)); //Add Maya output device
	}
}

//======================================================================
/*!	\brief	Start live link with MessageBus as default provider.

	\param[in] OnChangedCbFp Function pointer to function that refreshes Maya when live link provider changes.
*/
void UnrealInitializer::StartLiveLink(void (*OnChangedCbFp)(), void(*OnTimeChangedCbFp)(const struct FQualifiedFrameTime&))
{
	auto LiveLinkProvider = FUnrealStreamManager::TheOne().GetLiveLinkProvider();
	if (LiveLinkProvider.IsValid())
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("Live Link Provider already started!\n"));
	}
	else
	{
		// We start with message bus as our default provider
		FUnrealStreamManager::TheOne().SetLiveLinkProvider(LiveLinkSource::MessageBus); // ToDo: Maybe we need a create function instead of set?
		LiveLinkProvider = FUnrealStreamManager::TheOne().GetLiveLinkProvider();
	}

	ConnectionStatusChangedHandle = LiveLinkProvider->
		RegisterConnStatusChangedHandle(FMayaLiveLinkProviderConnectionStatusChanged::FDelegate::CreateStatic(OnChangedCbFp));

	TimeChangedReceivedHandle = LiveLinkProvider->
		RegisterTimeChangedReceived(FMayaLiveLinkProviderTimeChangedReceived::FDelegate::CreateStatic(OnTimeChangedCbFp));


	FPlatformMisc::LowLevelOutputDebugString(TEXT("Live Link Provider started!\n"));
}

//======================================================================
/*!	\brief Stop live link and completes necessary cleanup.
*/
void UnrealInitializer::StopLiveLink()
{
	auto LiveLinkProvider = FUnrealStreamManager::TheOne().GetLiveLinkProvider();

	if (LiveLinkProvider.IsValid())
	{
		if (ConnectionStatusChangedHandle.IsValid())
		{
			LiveLinkProvider->UnregisterConnStatusChangedHandle(ConnectionStatusChangedHandle);
		}

		if (TimeChangedReceivedHandle.IsValid())
		{
			LiveLinkProvider->UnregisterTimeChangedReceived(TimeChangedReceivedHandle);
		}

		FPlatformMisc::LowLevelOutputDebugString(TEXT("Deleting Live Link\n"));
		
		LiveLinkProvider.Reset();
	}

	ConnectionStatusChangedHandle.Reset();
	TimeChangedReceivedHandle.Reset();
	FPlatformMisc::LowLevelOutputDebugString(TEXT("Live Link Provider stopped!\n"));
}

//======================================================================
/*!	\brief Uninitialize Unreal modules and call app exit.
*/
void UnrealInitializer::UninitializeUnreal()
{
	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppExit();
}