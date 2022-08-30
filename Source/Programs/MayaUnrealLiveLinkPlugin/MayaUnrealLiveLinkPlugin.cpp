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


#include "RequiredProgramMainCPPInclude.h"
#include "Misc/CommandLine.h"
#include "Async/TaskGraphInterfaces.h"
#include "Features/IModularFeatures.h"
#include "INetworkMessagingExtension.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Modules/ModuleManager.h"
#include "Shared/UdpMessagingSettings.h"
#include "UObject/Object.h"
#include "Misc/ConfigCacheIni.h"

#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkAnimationTypes.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkCameraTypes.h"
#include "Roles/LiveLinkLightRole.h"
#include "Roles/LiveLinkLightTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "LiveLinkProvider.h"
#include "LiveLinkRefSkeleton.h"
#include "LiveLinkTypes.h"
#include "Misc/OutputDevice.h"

#include "UnrealInitializer/UnrealInitializer.h"
#include "CoreMinimal.h"
#include "UnrealInitializer/FUnrealStreamManager.h"

#include <cstdlib>
#include <set>

IMPLEMENT_APPLICATION(MayaUnrealLiveLinkPlugin, "MayaUnrealLiveLinkPlugin");

#include "MayaCommonIncludes.h"
#include "Subjects/IMStreamedEntity.h"
#include "Subjects/MStreamHierarchy.h"
#include "Subjects/MStreamedEntity.h"
#include "MayaUnrealLiveLinkUtils.h"
#include "MayaLiveLinkStreamManager.h"

THIRD_PARTY_INCLUDES_START
#include <maya/MAnimMessage.h>
#ifndef MAYA_OLD_PLUGIN
#include <maya/MCameraMessage.h>
#endif // MAYA_OLD_PLUGIN
THIRD_PARTY_INCLUDES_END

#define MCHECKERROR(STAT,MSG)	\
	if (!STAT) {				\
		perror(MSG);			\
		return MS::kFailure;	\
	}

#define MREPORTERROR(STAT,MSG)	\
	if (!STAT) {				\
		perror(MSG);			\
	}

class FLiveLinkStreamedSubjectManager;

void ClearViewportCallbacks();
MStatus RefreshViewportCallbacks();

MCallbackIdArray myCallbackIds;

MSpace::Space G_TransformSpace = MSpace::kTransform;

bool bUEInitialized = false;

const char PluginVersion[] = "v1.1.2";
const char PluginAppId[] = "3726213941804942083";

static const char OtherUEVersionLoadedEnvVar[] = "OtherUEVersionLoaded";

// Execute the python command to refresh our UI
void RefreshUI()
{
	MGlobal::executeCommand("MayaUnrealLiveLinkRefreshUI");
}

void OnConnectionStatusChanged()
{
	MGlobal::executeCommand("MayaUnrealLiveLinkRefreshConnectionUI");
}

void PrintInfoToMaya(const char* Info)
{
	MGlobal::displayInfo(Info);
}

inline void FTickerTick(float ElapsedTime)
{
#if !UE_5_0_OR_LATER
	FTicker::GetCoreTicker().Tick(ElapsedTime);
#else
	FTSTicker::GetCoreTicker().Tick(ElapsedTime);
#endif // UE_5_0_OR_LATER
}

void PutEnv(const char* EnvString, const char* EnvValue)
{
	if (EnvString && EnvValue)
	{
#if PLATFORM_WINDOWS
		auto CombinedString = std::string(EnvString);
		CombinedString += "=";
		CombinedString += EnvValue;
		_putenv(CombinedString.c_str());
#else
		setenv(EnvString, EnvValue, 1);
#endif // PLATFORM_WINDOWS
	}
}

void DelEnv(const char* EnvString)
{
	if (EnvString)
	{
#if PLATFORM_WINDOWS
		PutEnv(EnvString, "");
#else
		unsetenv(EnvString);
#endif // PLATFORM_WINDOWS
	}
}

std::string GetEnv(const char* EnvString)
{
	std::string EnvValue;
	if (EnvString)
	{
#if PLATFORM_WINDOWS
		size_t Len = 0;
		char Value[256] = "";
		getenv_s(&Len, Value, sizeof(Value), EnvString);
		EnvValue = Value;
#else
		if (auto GetEnvValue = secure_getenv(EnvString))
		{
			EnvValue = GetEnvValue;
		}
#endif // PLATFORM_WINDOWS
	}
	return EnvValue;
}

const MString LiveLinkSubjectNamesCommandName("LiveLinkSubjectNames");

class LiveLinkSubjectNamesCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkSubjectNamesCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MStringArray SubjectNames;
		MayaLiveLinkStreamManager::TheOne().GetSubjectNames(SubjectNames);

		const auto SubjectNamesLen = SubjectNames.length();
		for (unsigned int i = 0; i < SubjectNamesLen; ++i)
		{
			const auto& Entry = SubjectNames[i];
			appendToResult(Entry);
		}

		return MS::kSuccess;
	}
};

const MString LiveLinkSubjectPathsCommandName("LiveLinkSubjectPaths");

class LiveLinkSubjectPathsCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkSubjectPathsCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MStringArray SubjectPaths;
		MayaLiveLinkStreamManager::TheOne().GetSubjectPaths(SubjectPaths);

		const auto SubjectPathsLen = SubjectPaths.length();
		for (unsigned int i = 0; i < SubjectPathsLen; ++i)
		{
			const auto& Entry = SubjectPaths[i];
			appendToResult(Entry);
		}

		return MS::kSuccess;
	}
};

const MString LiveLinkSubjectRolesCommandName("LiveLinkSubjectRoles");

class LiveLinkSubjectRolesCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkSubjectRolesCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MStringArray SubjectRoles;
		MayaLiveLinkStreamManager::TheOne().GetSubjectRoles(SubjectRoles);

		const auto SubjectRolesLen = SubjectRoles.length();
		for (unsigned int i = 0; i < SubjectRolesLen; ++i)
		{
			const auto& Entry = SubjectRoles[i];
			appendToResult(Entry);
		}

		return MS::kSuccess;
	}
};

const MString LiveLinkSubjectTypesCommandName("LiveLinkSubjectTypes");

class LiveLinkSubjectTypesCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkSubjectTypesCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MStringArray SubjectTypes;
		MayaLiveLinkStreamManager::TheOne().GetSubjectTypes(SubjectTypes);

		const auto SubjectTypesLen = SubjectTypes.length();
		for (unsigned int i = 0; i < SubjectTypesLen; ++i)
		{
			const auto& Entry = SubjectTypes[i];
			appendToResult(Entry);
		}

		return MS::kSuccess;
	}
};

const MString LiveLinkAddSelectionCommandName("LiveLinkAddSelection");

class LiveLinkAddSelectionCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkAddSelectionCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MSelectionList SelectedItems;
		MGlobal::getActiveSelectionList(SelectedItems);

		bool AlreadyInTheList = false;
		for (unsigned int i = 0; i < SelectedItems.length(); ++i)
		{
			MObject SelectedRoot;
			SelectedItems.getDependNode(i, SelectedRoot);

			// Check whether the selected node is a DAG node first. If it's not, resetting DagIterator
			// with a non-DAG node will cause us to iterate from the scene root, which could result in
			// arbitrary nodes outside the selection being added (often the "|persp" camera).
			if (!SelectedRoot.hasFn(MFn::kDagNode)) {
				continue;
			}

			MItDag DagIterator;
			DagIterator.reset(SelectedRoot);

			AlreadyInTheList |= MayaLiveLinkStreamManager::TheOne().AddSubject(DagIterator);
		}
		appendToResult(AlreadyInTheList);
		return MS::kSuccess;
	}
};

const MString LiveLinkRemoveSubjectCommandName("LiveLinkRemoveSubject");

class LiveLinkRemoveSubjectCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkRemoveSubjectCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MSyntax Syntax;
		Syntax.addArg(MSyntax::kString);

		MArgDatabase argData(Syntax, args);

		MString SubjectToRemove;
		argData.getCommandArgument(0, SubjectToRemove);

		MayaLiveLinkStreamManager::TheOne().RemoveSubject(SubjectToRemove);

		return MS::kSuccess;
	}
};

const MString LiveLinkChangeSubjectNameCommandName("LiveLinkChangeSubjectName");

class LiveLinkChangeSubjectNameCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkChangeSubjectNameCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MSyntax Syntax;
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kString);

		MArgDatabase argData(Syntax, args);

		MString SubjectDagPath;
		MString NewName;
		argData.getCommandArgument(0, SubjectDagPath);
		argData.getCommandArgument(1, NewName);

		MayaLiveLinkStreamManager::TheOne().ChangeSubjectName(SubjectDagPath, NewName);

		return MS::kSuccess;
	}
};

const MString LiveLinkConnectionStatusCommandName("LiveLinkConnectionStatus");

class LiveLinkConnectionStatusCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkConnectionStatusCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MString ConnectionStatus("No Provider (internal error)");
		bool bConnection = false;

		if(FUnrealStreamManager::TheOne().GetLiveLinkProvider().IsValid())
		{
			if (FUnrealStreamManager::TheOne().GetLiveLinkProvider()->HasConnection())
			{
				ConnectionStatus = "Connected";
				bConnection = true;
			}
			else
			{
				ConnectionStatus = "No Connection";
			}
		}

		static bool PreviousConnectionStatus = false;
		if (PreviousConnectionStatus != bConnection)
		{
			if (bConnection)
			{
				MayaLiveLinkStreamManager::TheOne().RebuildSubjects(false);
			}
			PreviousConnectionStatus = bConnection;
		}

		appendToResult(ConnectionStatus);
		appendToResult(bConnection);

		return MS::kSuccess;
	}
};

const MString LiveLinkChangeSubjectStreamTypeCommandName("LiveLinkChangeSubjectStreamType");

class LiveLinkChangeSubjectStreamTypeCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkChangeSubjectStreamTypeCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MSyntax Syntax;
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kString);

		MArgDatabase argData(Syntax, args);

		MString SubjectPath;
		argData.getCommandArgument(0, SubjectPath);
		MString StreamType;
		argData.getCommandArgument(1, StreamType);

		MayaLiveLinkStreamManager::TheOne().ChangeStreamType(SubjectPath, StreamType);

		return MS::kSuccess;
	}
};

class LiveLinkMessagingSettingsCommand : public MPxCommand
{
public:
	static constexpr char CommandName[] = "LiveLinkMessagingSettings";

	static constexpr char UnicastEndpointFlag[] = "ue";
	static constexpr char UnicastEndpointFlagLong[] = "unicastEndpoint";
	static constexpr char StaticEndpointsFlag[] = "se";
	static constexpr char StaticEndpointsFlagLong[] = "staticEndpoints";
	static constexpr char AddEndpointFlag[] = "a";
	// Long names must be at least four characters, so it can't be just "add".
	static constexpr char AddEndpointFlagLong[] = "addEndpoint";
	static constexpr char RemoveEndpointFlag[] = "r";
	static constexpr char RemoveEndpointFlagLong[] = "removeEndpoint";

	static void* Creator() { return new LiveLinkMessagingSettingsCommand(); }

	static MSyntax CreateSyntax()
	{
		MStatus Status;
		MSyntax Syntax;

		Syntax.enableQuery(true);

		Status = Syntax.setObjectType(MSyntax::kStringObjects);
		CHECK_MSTATUS(Status);

		Status = Syntax.addFlag(UnicastEndpointFlag, UnicastEndpointFlagLong, MSyntax::kString);
		CHECK_MSTATUS(Status);
		Status = Syntax.addFlag(StaticEndpointsFlag, StaticEndpointsFlagLong, MSyntax::kString);
		CHECK_MSTATUS(Status);
		Status = Syntax.addFlag(AddEndpointFlag, AddEndpointFlagLong, MSyntax::kString);
		CHECK_MSTATUS(Status);
		Status = Syntax.addFlag(RemoveEndpointFlag, RemoveEndpointFlagLong, MSyntax::kString);
		CHECK_MSTATUS(Status);

		return Syntax;
	}

	MStatus doIt(const MArgList& args) override
	{
		MStatus Status;
		MArgDatabase ArgData(syntax(), args, &Status);
		CHECK_MSTATUS_AND_RETURN_IT(Status);

		MStringArray EndpointStrings;
		Status = ArgData.getObjects(EndpointStrings);
		CHECK_MSTATUS_AND_RETURN_IT(Status);

		const bool bIsUnicast = ArgData.isFlagSet(UnicastEndpointFlag, &Status);
		CHECK_MSTATUS_AND_RETURN_IT(Status);
		const bool bIsStatic = ArgData.isFlagSet(StaticEndpointsFlag, &Status);
		CHECK_MSTATUS_AND_RETURN_IT(Status);

		const bool bAddStatic = ArgData.isFlagSet(AddEndpointFlag, &Status);
		CHECK_MSTATUS_AND_RETURN_IT(Status);
		const bool bRemoveStatic = ArgData.isFlagSet(RemoveEndpointFlag, &Status);
		CHECK_MSTATUS_AND_RETURN_IT(Status);

		if ((static_cast<int>(bIsUnicast) + static_cast<int>(bIsStatic)) != 1)
		{
			MString ErrorMsg;
			ErrorMsg.format(
				"Must specify exactly one of -^1s or -^2s",
				UnicastEndpointFlagLong, StaticEndpointsFlagLong);
			displayError(ErrorMsg);
			return MS::kFailure;
		}

		if (!IModularFeatures::Get().IsModularFeatureAvailable(INetworkMessagingExtension::ModularFeatureName))
		{
			return MS::kFailure;
		}

		INetworkMessagingExtension& NetworkExtension =
			IModularFeatures::Get().GetModularFeature<INetworkMessagingExtension>(INetworkMessagingExtension::ModularFeatureName);

		UUdpMessagingSettings* Settings = GetMutableDefault<UUdpMessagingSettings>();
		if (!Settings)
		{
			return MS::kFailure;
		}

		if (ArgData.isQuery())
		{
			if (bIsUnicast)
			{
				setResult(MString(TCHAR_TO_ANSI(*Settings->UnicastEndpoint)));
			}
			else
			{
				for (const FString& StaticEndpoint : Settings->StaticEndpoints)
				{
					appendToResult(MString(TCHAR_TO_ANSI(*StaticEndpoint)));
				}

				if (!isCurrentResultArray())
				{
					// Make sure we return an empty string array rather than nothing if
					// there were no static endpoints.
					static const MStringArray EmptyStringArray;
					setResult(EmptyStringArray);
				}
			}

			return MS::kSuccess;
		}

		// Code below this point will (potentially) modify the settings and might
		// restart LiveLink. The return value will indicate whether the settings were
		// changed, so mark it as unchanged initially.
		setResult(false);

		const unsigned int NumEndpointStrings = EndpointStrings.length();
		if (NumEndpointStrings < 1u)
		{
			displayError("Must specify endpoint(s) when editing");
			return MS::kFailure;
		}

		// Validate the endpoint strings.
		for (unsigned int Index = 0u; Index < NumEndpointStrings; ++Index)
		{
			const MString& EndpointString = EndpointStrings[Index];
			FIPv4Endpoint Endpoint;
			if (!FIPv4Endpoint::FromHostAndPort(EndpointString.asChar(), Endpoint))
			{
				MString ErrorMsg;
				ErrorMsg.format(
					"The string \"^1s\" is not a valid endpoint string",
					EndpointString);
				displayError(ErrorMsg);
				return MS::kFailure;
			}
		}

		if (bIsUnicast)
		{
			if (EndpointStrings.length() != 1u)
			{
				displayError("Must specify exactly one endpoint when editing the unicast endpoint");
				return MS::kFailure;
			}

			if (bAddStatic || bRemoveStatic)
			{
				MString ErrorMsg;
				ErrorMsg.format(
					"The -^1s and -^2s flags are not valid when editing the unicast endpoint",
					AddEndpointFlagLong, RemoveEndpointFlagLong);
				displayError(ErrorMsg);
				return MS::kFailure;
			}

			if (Settings->UnicastEndpoint != EndpointStrings[0].asChar())
			{
				UnrealInitializer::TheOne().StopLiveLink();

				Settings->UnicastEndpoint = EndpointStrings[0].asChar();
				NetworkExtension.RestartServices();

				UnrealInitializer::TheOne().StartLiveLink(OnConnectionStatusChanged);
				MayaLiveLinkStreamManager::TheOne().Reset();
				MayaLiveLinkStreamManager::TheOne().RebuildSubjects();

				setResult(true);
			}
		}
		else
		{
			// Editing static endpoints.
			if ((static_cast<int>(bAddStatic) + static_cast<int>(bRemoveStatic)) != 1)
			{
				MString ErrorMsg;
				ErrorMsg.format(
					"Must specify exactly one of -^1s or -^2s when editing static endpoints",
					AddEndpointFlagLong, RemoveEndpointFlagLong);
				displayError(ErrorMsg);
				return MS::kFailure;
			}

			for (unsigned int Index = 0u; Index < NumEndpointStrings; ++Index)
			{
				const MString& EndpointString = EndpointStrings[Index];
				const TArray<FString>::SizeType SettingsIndex = Settings->StaticEndpoints.Find(EndpointString.asChar());
				if (bAddStatic && SettingsIndex == INDEX_NONE)
				{
					Settings->StaticEndpoints.Add(EndpointString.asChar());
					NetworkExtension.AddEndpoint(EndpointString.asChar());
					setResult(true);
				}
				else if (bRemoveStatic && SettingsIndex != INDEX_NONE)
				{
					Settings->StaticEndpoints.RemoveAt(SettingsIndex);
					NetworkExtension.RemoveEndpoint(EndpointString.asChar());
					setResult(true);
				}
			}
		}

		return MS::kSuccess;
	}
};

constexpr char LiveLinkMessagingSettingsCommand::CommandName[];
constexpr char LiveLinkMessagingSettingsCommand::UnicastEndpointFlag[];
constexpr char LiveLinkMessagingSettingsCommand::UnicastEndpointFlagLong[];
constexpr char LiveLinkMessagingSettingsCommand::StaticEndpointsFlag[];
constexpr char LiveLinkMessagingSettingsCommand::StaticEndpointsFlagLong[];
constexpr char LiveLinkMessagingSettingsCommand::AddEndpointFlag[];
constexpr char LiveLinkMessagingSettingsCommand::AddEndpointFlagLong[];
constexpr char LiveLinkMessagingSettingsCommand::RemoveEndpointFlag[];
constexpr char LiveLinkMessagingSettingsCommand::RemoveEndpointFlagLong[];

const MString LiveLinkChangeSourceCommandName("LiveLinkChangeSource");

class LiveLinkChangeSourceCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkChangeSourceCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		if (args.length() == 0)
		{
			return MS::kFailure;
		}

		MStatus status;
		auto SourceIndex = args.asInt(0, &status);
		if (status == MS::kSuccess &&
			SourceIndex > 0 && (SourceIndex - 1) != FUnrealStreamManager::TheOne().GetLiveLinkProvider()->GetSourceType())
		{
			MayaLiveLinkStreamManager::TheOne().ClearSubjects();
			UnrealInitializer::TheOne().StopLiveLink();

			FUnrealStreamManager::TheOne().SetLiveLinkProvider(static_cast<LiveLinkSource>(SourceIndex - 1));
			UnrealInitializer::TheOne().StartLiveLink(OnConnectionStatusChanged);
			MayaLiveLinkStreamManager::TheOne().Reset();
			MayaLiveLinkStreamManager::TheOne().RebuildSubjects();
		}

		return MS::kSuccess;
	}
};

const MString LiveLinkGetSourceNamesCommandName("LiveLinkGetSourceNames");

class LiveLinkGetSourceNamesCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetSourceNamesCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MStringArray SourceNames;
		for (auto& sourceName : LiveLinkSourceNames)
		{
			SourceNames.append(sourceName);
		}
		appendToResult(SourceNames);

		return MS::kSuccess;
	}
};

const MString LiveLinkGetSelectedSourceCommandName("LiveLinkGetSelectedSource");

class LiveLinkGetSelectedSourceCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetSelectedSourceCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		appendToResult(FUnrealStreamManager::TheOne().GetLiveLinkProvider()->GetSourceType() + 1);

		return MS::kSuccess;
	}
};

const MString LiveLinkSendSubjectListCommandName("LiveLinkSendSubjectList");

class LiveLinkSendSubjectListCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkSendSubjectListCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MayaLiveLinkStreamManager::TheOne().RebuildSubjects();

		return MS::kSuccess;
	}
};

const MString LiveLinkExportStaticDataCommandName("LiveLinkExportStaticData");

class LiveLinkExportStaticDataCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkExportStaticDataCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		if (args.length() != 1)
		{
			return MS::kFailure;
		}

		MStatus status;
		auto FilePath = args.asString(0, &status);
		bool Success = status == MS::kSuccess;
		if (Success &&
			FilePath.length() > 0)
		{
			MDagPath DagPath;
			if (MayaUnrealLiveLinkUtils::GetSelectedSubjectDagPath(DagPath) == MS::kSuccess &&
				MayaLiveLinkStreamManager::TheOne().ExportSubjectStaticDataToJSON(DagPath.fullPathName(), FilePath))
			{
				return MS::kSuccess;
			}
		}

		return MS::kFailure;
	}
};

const MString LiveLinkExportFrameDataCommandName("LiveLinkExportFrameData");

class LiveLinkExportFrameDataCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkExportFrameDataCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		if (args.length() != 2)
		{
			return MS::kFailure;
		}

		MStatus status;
		auto FilePath = args.asString(0, &status);
		bool Success = status == MS::kSuccess;
		auto FrameTime = args.asDouble(1, &status);
		Success = Success && status == MS::kSuccess;
		if (Success &&
			FilePath.length() > 0 &&
			FrameTime >= 0.0)
		{
			MDagPath DagPath;
			if (MayaUnrealLiveLinkUtils::GetSelectedSubjectDagPath(DagPath) == MS::kSuccess &&
				MayaLiveLinkStreamManager::TheOne().ExportSubjectFrameDataToJSON(DagPath.fullPathName(), FilePath, FrameTime))
			{
				return MS::kSuccess;
			}
		}

		return MS::kFailure;
	}
};

const MString LiveLinkGetPluginVersionCommandName("LiveLinkGetPluginVersion");

class LiveLinkGetPluginVersionCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetPluginVersionCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MString Version = PluginVersion;
		Version.substitute("v", "");
		appendToResult(Version);

		return MS::kSuccess;
	}
};

const MString LiveLinkGetUnrealVersionCommandName("LiveLinkGetUnrealVersion");

class LiveLinkGetUnrealVersionCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetUnrealVersionCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MString Version;
		Version += ENGINE_MAJOR_VERSION;
		Version += ".";
		Version += ENGINE_MINOR_VERSION;
		Version += ".";
		Version += ENGINE_PATCH_VERSION;
		appendToResult(Version);

		return MS::kSuccess;
	}
};

const MString LiveLinkGetPluginAppIdCommandName("LiveLinkGetPluginAppId");

class LiveLinkGetPluginAppIdCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetPluginAppIdCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		appendToResult(PluginAppId);

		return MS::kSuccess;
	}
};

const MString LiveLinkGetPluginRequestUrlCommandName("LiveLinkGetPluginRequestUrl");

class LiveLinkGetPluginRequestUrlCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetPluginRequestUrlCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		appendToResult("https://apps.autodesk.com/api/v1/apps?store=MAYA&isDetail=true&isLive=true");

		return MS::kSuccess;
	}
};

const MString LiveLinkGetPluginUpdateUrlCommandName("LiveLinkGetPluginUpdateUrl");

class LiveLinkGetPluginUpdateUrlCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetPluginUpdateUrlCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		appendToResult(MString("https://apps.autodesk.com/MAYA/en/Detail/Index?id=") + MString(PluginAppId));

		return MS::kSuccess;
	}
};

const MString LiveLinkGetPluginDocumentationUrlCommandName("LiveLinkGetPluginDocumentationUrl");

class LiveLinkGetPluginDocumentationUrlCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetPluginDocumentationUrlCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		appendToResult(MString("https://www.autodesk.com/unreal-livelink-docs"));

		return MS::kSuccess;
	}
};

const MString LiveLinkOnQuitCommandName("LiveLinkOnQuit");

class LiveLinkOnQuitCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkOnQuitCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MSyntax Syntax;
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kString);

		MArgDatabase argData(Syntax, args);

		DelEnv(OtherUEVersionLoadedEnvVar);

		return MS::kSuccess;
	}
};


void OnMayaExit(void* client)
{
	MayaLiveLinkStreamManager::TheOne().ClearSubjects();
}

void OnScenePreOpen(void* client)
{
	ClearViewportCallbacks();
	MayaLiveLinkStreamManager::TheOne().Reset();
	RefreshUI();
}

void OnSceneOpen(void* client)
{
	//BuildStreamHierarchyData();
}

bool CameraManipStarted = false;
bool CameraManipEnded = false;
bool AnimCurveEdited = false;
bool AnimKeyFrameEdited = false;
std::atomic<bool> SendUpdatedData {false};

// Helper method to send data to unreal when SendUpdatedData is set.
void StreamDataToUnreal()
{
	// Stream data only when this flag is set.
	if (!SendUpdatedData) return;

	if (!CameraManipEnded && !AnimCurveEdited && !AnimKeyFrameEdited)
	{
		MayaLiveLinkStreamManager::TheOne().StreamSubjects();
	}
	else
	{
		CameraManipEnded = false;
		if (!AnimCurveEdited)
		{
			// If anim curve edited was clear, we can then clear the anim key framed edited flag
			// That's because Maya sends 2 OnTimeChanged events that we want to ignore.
			AnimKeyFrameEdited = false;
		}
		else
		{
			AnimCurveEdited = false;
		}
	}
	CameraManipStarted = false;

	// Set the streaming flag to false.
	SendUpdatedData = false;
}

void OnTimeChanged(MTime& Time, void* ClientData)
{
	SendUpdatedData = true;
}

void OnAnimCurveEdited(MObjectArray& Objects, void* ClientData)
{
	auto& MayaStreamManager = MayaLiveLinkStreamManager::TheOne();

	// Get the list of tracked subjects
	MStringArray SubjectPaths;
	MayaStreamManager.GetSubjectPaths(SubjectPaths);
	const auto SubjectPathLen = SubjectPaths.length();
	if (SubjectPathLen == 0)
	{
		return;
	}

	MDagPathArray DagPathArray;
	MStatus Status;
	auto Length = Objects.length();
	std::set<unsigned int> DagPathsToStream;
	for (unsigned int index = 0; index < Length; ++index)
	{
		MObject& Obj = Objects[index];
		if (Obj.hasFn(MFn::kAnimCurve))
		{
			MFnAnimCurve AnimCurve(Obj);
			MPlugArray Connections;
			AnimCurve.getConnections(Connections);

			for (unsigned int i = 0; i < Connections.length(); ++i)
			{
				auto& Connection = Connections[i];
				MPlugArray SrcPlugArray;
				Connection.connectedTo(SrcPlugArray, false, true);
				for (unsigned int src = 0; src < SrcPlugArray.length(); ++src)
				{
					MPlug& Plug = SrcPlugArray[src];
					MObject Node = Plug.node();
					if (Node.hasFn(MFn::kDagNode))
					{
						MFnDagNode DagNode(Node);
						MDagPath DagPath;

						if (DagNode.getPath(DagPath))
						{
							MString DagPathName;
							DagPathName = DagPath.fullPathName();

							for (unsigned int path = 0; path < SubjectPathLen; ++path)
							{
								MString& SubjectPath = SubjectPaths[path];

								MSelectionList SelectionList;
								SelectionList.add(SubjectPath);
								MDagPath SubjectDagPath;
								MObject SubjectObj;
								SelectionList.getDependNode(0, SubjectObj);
								SelectionList.getDagPath(0, SubjectDagPath);

								if (SubjectPath == DagPathName ||
									DagNode.isChildOf(SubjectObj))
								{
									if (DagPathsToStream.find(path) == DagPathsToStream.end())
									{
										DagPathsToStream.insert(path);
										DagPathArray.append(SubjectDagPath);
									}
									AnimCurveEdited = true;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	for (unsigned int Index = 0; Index < DagPathArray.length(); ++Index)
	{
		MayaStreamManager.StreamSubject(DagPathArray[Index]);
	}

}

void OnAnimKeyframeEdited(MObjectArray &objects,void *clientData)
{
	AnimKeyFrameEdited = true;
}

#ifndef MAYA_OLD_PLUGIN
bool IsActiveCameraSubject(const MObject& Node)
{
	if (Node.hasFn(MFn::kCamera))
	{
		MStatus Status;
		MFnDagNode Camera(Node, &Status);
		if (Status)
		{
			MDagPath DagPath;
			return Camera.getPath(DagPath) &&
				DagPath == MayaLiveLinkStreamManager::TheOne().GetActiveCameraSubjectPath();
		}
	}
	return false;
}

void OnCameraBeginManip(MObject& Node, void* ClientData)
{
	if (!FUnrealStreamManager::TheOne().GetLiveLinkProvider().IsValid())
	{
		return;
	}

	M3dView ActiveView = M3dView::active3dView();
	MDagPath ActiveCameraDagPath;
	ActiveView.getCamera(ActiveCameraDagPath);

	auto& MayaStreamManager = MayaLiveLinkStreamManager::TheOne();
	MDagPath SubjectDagPath = MayaStreamManager.GetActiveCameraSubjectPath();
	if (!SubjectDagPath.isValid() || !(ActiveCameraDagPath == SubjectDagPath))
	{
		MayaStreamManager.SetActiveCameraDagPath(ActiveCameraDagPath);
		CameraManipStarted = true;
	}
	else
	{
		CameraManipStarted = IsActiveCameraSubject(Node);
	}
	CameraManipEnded = false;
}

void OnCameraEndManip(MObject& Node, void* ClientData)
{
	if (!FUnrealStreamManager::TheOne().GetLiveLinkProvider().IsValid())
	{
		return;
	}

	CameraManipStarted = false;
	CameraManipEnded = IsActiveCameraSubject(Node);
}
#endif // MAYA_OLD_PLUGIN

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::map<uintptr_t, MCallbackId> PostRenderCallbackIds;
std::map<uintptr_t, MCallbackId> ViewportDeletedCallbackIds;
std::map<uintptr_t, MCallbackId> CameraBeginManipCallbackIds;
std::map<uintptr_t, MCallbackId> CameraEndManipCallbackIds;
std::map<uintptr_t, MCallbackId> CameraChangedCallbackIds;

void OnPostRenderViewport(const MString &str, void* ClientData)
{
	if (!FUnrealStreamManager::TheOne().GetLiveLinkProvider().IsValid())
	{
		return;
	}

	StreamDataToUnreal();

	auto& MayaStreamManager = MayaLiveLinkStreamManager::TheOne();

#ifdef MAYA_OLD_PLUGIN
	auto CameraDagPath = MayaStreamManager.GetActiveCameraSubjectPath();
	if (CameraDagPath.isValid())
	{
		MayaStreamManager.StreamSubject(CameraDagPath);
	}
#else
	if (CameraManipStarted)
	{
		M3dView View;
		M3dView::getM3dViewFromModelPanel(str, View);
		MDagPath CameraDagPath;
		View.getCamera(CameraDagPath);
		if (CameraDagPath == MayaStreamManager.GetActiveCameraSubjectPath())
		{
			MayaStreamManager.StreamSubject(CameraDagPath);
		}
	}
#endif // MAYA_OLD_PLUGIN
}

void OnCameraChanged(const MString& String, MObject& Node, void* ClientData)
{
	ClearViewportCallbacks();
	RefreshViewportCallbacks();
}

void OnViewportClosed(void* ClientData)
{
	uintptr_t ViewIndex = reinterpret_cast<uintptr_t>(ClientData);

	auto removeCallback = [ViewIndex](std::map<uintptr_t, MCallbackId>& CallbackIds)
	{
		MMessage::removeCallback(CallbackIds[ViewIndex]);
		CallbackIds.erase(ViewIndex);
	};

	if (CameraBeginManipCallbackIds.size() == PostRenderCallbackIds.size())
	{
		removeCallback(CameraBeginManipCallbackIds);
	}
	if (CameraEndManipCallbackIds.size() == PostRenderCallbackIds.size())
	{
		removeCallback(CameraEndManipCallbackIds);
	}

	removeCallback(PostRenderCallbackIds);
	removeCallback(ViewportDeletedCallbackIds);
	removeCallback(CameraChangedCallbackIds);
}

void ClearViewportCallbacks()
{
	auto removeCallbacks = [](std::map<uintptr_t, MCallbackId>& CallbackIds)
	{
		for (std::pair<const uintptr_t, MCallbackId>& Pair : CallbackIds)
		{
			MMessage::removeCallback(Pair.second);
		}
		CallbackIds.clear();
	};

	removeCallbacks(PostRenderCallbackIds);
	removeCallbacks(ViewportDeletedCallbackIds);
	removeCallbacks(CameraBeginManipCallbackIds);
	removeCallbacks(CameraEndManipCallbackIds);
	removeCallbacks(CameraChangedCallbackIds);
}

MStatus RefreshViewportCallbacks()
{
	MStatus ExitStatus;

	if (M3dView::numberOf3dViews() != PostRenderCallbackIds.size())
	{
		ClearViewportCallbacks();

		static MString ListEditorPanelsCmd = "gpuCacheListModelEditorPanels";

		MStringArray EditorPanels;
		ExitStatus = MGlobal::executeCommand(ListEditorPanelsCmd, EditorPanels);
		MCHECKERROR(ExitStatus, "gpuCacheListModelEditorPanels");

		if (ExitStatus == MStatus::kSuccess)
		{
			for (uintptr_t i = 0; i < EditorPanels.length(); ++i)
			{
				MStatus Status;
				MCallbackId CallbackId = MUiMessage::add3dViewPostRenderMsgCallback(EditorPanels[i], OnPostRenderViewport, NULL, &Status);

				MREPORTERROR(Status, "MUiMessage::add3dViewPostRenderMsgCallback()");

				if (Status != MStatus::kSuccess)
				{
					ExitStatus = MStatus::kFailure;
					continue;
				}

				PostRenderCallbackIds.emplace(i, CallbackId);

#ifndef MAYA_OLD_PLUGIN
				M3dView View;
				MString EditorPanel = EditorPanels[i];
				int LastIndex = EditorPanel.rindex('|');
				if (LastIndex >= 0)
				{
					EditorPanel = EditorPanel.substring(LastIndex + 1, EditorPanel.length());
				}
				if (M3dView::getM3dViewFromModelPanel(EditorPanel, View))
				{
					// Callback to detect when a viewport gets assign to a new camera
					CallbackId = MUiMessage::addCameraChangedCallback(EditorPanel, OnCameraChanged, nullptr, &Status);
					if (Status != MStatus::kSuccess)
					{
						ExitStatus = MStatus::kFailure;
						continue;
					}
					CameraChangedCallbackIds.emplace(i, CallbackId);

					MDagPath CameraDagPath;
					if (View.getCamera(CameraDagPath))
					{
						MFnCamera Camera(CameraDagPath, &Status);
						if (Status)
						{
							CallbackId = MCameraMessage::addBeginManipulationCallback(Camera.object(), OnCameraBeginManip, nullptr, &Status);
							MREPORTERROR(Status, "MCameraMessage::addBeginManipulationCallback()");
							if (Status != MStatus::kSuccess)
							{
								ExitStatus = MStatus::kFailure;
								continue;
							}
							CameraBeginManipCallbackIds.emplace(i, CallbackId);

							CallbackId = MCameraMessage::addEndManipulationCallback(Camera.object(), OnCameraEndManip, nullptr, &Status);
							MREPORTERROR(Status, "MCameraMessage::addEndManipulationCallback()");
							if (Status != MStatus::kSuccess)
							{
								ExitStatus = MStatus::kFailure;
								continue;
							}
							CameraEndManipCallbackIds.emplace(i, CallbackId);
						}
					}
				}
#endif // MAYA_OLD_PLUGIN

				CallbackId = MUiMessage::addUiDeletedCallback(EditorPanels[i], OnViewportClosed, reinterpret_cast<void*>(i), &Status);

				MREPORTERROR(Status, "MUiMessage::addUiDeletedCallback()");

				if (Status != MStatus::kSuccess)
				{
					ExitStatus = MStatus::kFailure;
					continue;
				}
				ViewportDeletedCallbackIds.emplace(i, CallbackId);
			}
		}
	}

	return ExitStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OnInterval(float ElapsedTime, float LastTime, void* ClientData)
{
	//No good way to check for new views being created, so just periodically refresh our list
	RefreshViewportCallbacks();

	OnConnectionStatusChanged();

	FTickerTick(ElapsedTime);
}

#if PLATFORM_WINDOWS
#define EXTERN_DECL __declspec( dllexport )
#else
#define EXTERN_DECL extern
#endif

/**
* This function is called by Maya when the plugin becomes loaded
*
* @param	MayaPluginObject	The Maya object that represents our plugin
*
* @return	MS::kSuccess if everything went OK and the plugin is ready to use
*/
EXTERN_DECL MStatus initializePlugin(MObject MayaPluginObject)
{
	// Tell Maya about our plugin
	MFnPlugin MayaPlugin(
		MayaPluginObject,
		"MayaUnrealLiveLinkPlugin",
		PluginVersion);

	// Check if another UE version of the plugin is already loaded
	auto LoadedString = GetEnv(OtherUEVersionLoadedEnvVar);
	if (LoadedString.empty())
	{
		// Create an environment variable telling that the current plugin is loaded
		PutEnv(OtherUEVersionLoadedEnvVar, MayaPlugin.name().asChar());
	}
	else
	{
		// We're loading another UE version of the plugin, make sure it's different
		// If it is, we're going to disable the auto-load for the previous plugin and
		// activate the auto-load for the current plugin.
		// We will also quit Maya to completely initialize Unreal.
		auto PrevPluginName = MString(LoadedString.c_str());
		if (MayaPlugin.name() != PrevPluginName)
		{
#if UE_5_0_OR_LATER
			MGlobal::displayWarning("Unable to load Unreal 5.x Live Link for Maya plug-in, because the Unreal 4.27 version of the same plug-in is/was already loaded.");
#else
			MGlobal::displayWarning("Unable to load Unreal 4.27 Live Link for Maya plug-in, because the Unreal 5.x version of the same plug-in is/was already loaded.");
#endif // UE_5_0_OR_LATER

			// Execute the command that will check for the auto-load status, change it for the current plugin and tell the user
			// that Maya needs to be restarted for the change to take effect.
			MGlobal::executeCommandOnIdle("MayaLiveLinkNotifyAndQuit \"" + MayaPlugin.name() + "\" \"" + PrevPluginName + "\"", false);

			MGlobal::executeCommandOnIdle("MayaUnrealLiveLinkRefreshUI");

			// Must return success, otherwise we won't be able to set the autoload flag since Maya throws an exception if the plugin is not loaded
			return MS::kSuccess;
		}
	}

	if (UnrealInitializer::TheOne().HasInitializedOnce())
	{
		MGlobal::displayWarning("Unreal Live Link plug-in is unable to reload after unloading in same session. Please restart Maya to reload the plug-in again.");
		const MStatus MayaStatusResult = MS::kFailure;
		return MayaStatusResult;
	}

	UnrealInitializer::TheOne().InitializeUnreal();
	UnrealInitializer::TheOne().AddMayaOutput(PrintInfoToMaya);
	UnrealInitializer::TheOne().StartLiveLink(OnConnectionStatusChanged);

	// We do not tick the core engine but we need to tick the ticker to make sure the message
	// bus endpoint in LiveLinkProvider is up to date.
	FTickerTick(1.0f);
	MayaLiveLinkStreamManager::TheOne().Reset();

	MCallbackId MayaExitingCallbackId = MSceneMessage::addCallback(MSceneMessage::kMayaExiting, (MMessage::MBasicFunction)OnMayaExit);
	myCallbackIds.append(MayaExitingCallbackId);

	MCallbackId ScenePreOpenedCallbackID = MSceneMessage::addCallback(MSceneMessage::kBeforeOpen, (MMessage::MBasicFunction)OnScenePreOpen);
	myCallbackIds.append(ScenePreOpenedCallbackID);

	MCallbackId SceneOpenedCallbackId = MSceneMessage::addCallback(MSceneMessage::kAfterOpen, (MMessage::MBasicFunction)OnSceneOpen);
	myCallbackIds.append(SceneOpenedCallbackId);

	MCallbackId TimeChangedCallbackId = MDGMessage::addTimeChangeCallback(OnTimeChanged);
	myCallbackIds.append(TimeChangedCallbackId);

	MCallbackId AnimCurveEditedCallbackId = MAnimMessage::addAnimCurveEditedCallback(OnAnimCurveEdited);
	myCallbackIds.append(AnimCurveEditedCallbackId);
	MCallbackId AnimKeyframeEditedCallbackId = MAnimMessage::addAnimKeyframeEditedCallback(OnAnimKeyframeEdited);
	myCallbackIds.append(AnimKeyframeEditedCallbackId);

	// Update function every 5 seconds
	MCallbackId timerCallback = MTimerMessage::addTimerCallback(5.f, (MMessage::MElapsedTimeFunction)OnInterval);
	myCallbackIds.append(timerCallback);

	MayaPlugin.registerCommand(LiveLinkSubjectNamesCommandName, LiveLinkSubjectNamesCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectPathsCommandName, LiveLinkSubjectPathsCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectRolesCommandName, LiveLinkSubjectRolesCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectTypesCommandName, LiveLinkSubjectTypesCommand::creator);
	MayaPlugin.registerCommand(LiveLinkAddSelectionCommandName, LiveLinkAddSelectionCommand::creator);
	MayaPlugin.registerCommand(LiveLinkRemoveSubjectCommandName, LiveLinkRemoveSubjectCommand::creator);
	MayaPlugin.registerCommand(LiveLinkChangeSubjectNameCommandName, LiveLinkChangeSubjectNameCommand::creator);
	MayaPlugin.registerCommand(LiveLinkConnectionStatusCommandName, LiveLinkConnectionStatusCommand::creator);
	MayaPlugin.registerCommand(LiveLinkChangeSubjectStreamTypeCommandName, LiveLinkChangeSubjectStreamTypeCommand::creator);
	MayaPlugin.registerCommand(LiveLinkMessagingSettingsCommand::CommandName, LiveLinkMessagingSettingsCommand::Creator,
							   LiveLinkMessagingSettingsCommand::CreateSyntax);
	MayaPlugin.registerCommand(LiveLinkChangeSourceCommandName, LiveLinkChangeSourceCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSendSubjectListCommandName, LiveLinkSendSubjectListCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetSourceNamesCommandName, LiveLinkGetSourceNamesCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetSelectedSourceCommandName, LiveLinkGetSelectedSourceCommand::creator);
	MayaPlugin.registerCommand(LiveLinkExportStaticDataCommandName, LiveLinkExportStaticDataCommand::creator);
	MayaPlugin.registerCommand(LiveLinkExportFrameDataCommandName, LiveLinkExportFrameDataCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetPluginVersionCommandName, LiveLinkGetPluginVersionCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetUnrealVersionCommandName, LiveLinkGetUnrealVersionCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetPluginAppIdCommandName, LiveLinkGetPluginAppIdCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetPluginRequestUrlCommandName, LiveLinkGetPluginRequestUrlCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetPluginUpdateUrlCommandName, LiveLinkGetPluginUpdateUrlCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetPluginDocumentationUrlCommandName, LiveLinkGetPluginDocumentationUrlCommand::creator);
	MayaPlugin.registerCommand(LiveLinkOnQuitCommandName, LiveLinkOnQuitCommand::creator);

	MGlobal::executeCommandOnIdle("MayaUnrealLiveLinkInitialized");

	// Print to Maya's output window, too!
	MGlobal::displayInfo("MayaUnrealLiveLinkPlugin initialized");

	MGlobal::executeCommandOnIdle("SetCommandCategory");

	RefreshViewportCallbacks();

	MGlobal::executeCommandOnIdle("MayaUnrealLiveLinkRefreshUI");

	const MStatus MayaStatusResult = MS::kSuccess;
	return MayaStatusResult;
}


/**
* Called by Maya either at shutdown, or when the user opts to unload the plugin through the Plugin Manager
*
* @param	MayaPluginObject	The Maya object that represents our plugin
*
* @return	MS::kSuccess if everything went OK and the plugin was fully shut down
*/
EXTERN_DECL MStatus uninitializePlugin(MObject MayaPluginObject)
{
	// Get the plugin API for the plugin object
	MFnPlugin MayaPlugin(MayaPluginObject);

	// Reset the environment variable if it's for this plugin
	auto LoadedString = GetEnv(OtherUEVersionLoadedEnvVar);
	if (!LoadedString.empty())
	{
		auto PluginName = MString(LoadedString.c_str());
		if (MayaPlugin.name() == PluginName)
		{
			DelEnv(OtherUEVersionLoadedEnvVar);
		}
	}

	// ... do stuff here ...

	MayaPlugin.deregisterCommand(LiveLinkSubjectNamesCommandName);
	MayaPlugin.deregisterCommand(LiveLinkSubjectPathsCommandName);
	MayaPlugin.deregisterCommand(LiveLinkSubjectRolesCommandName);
	MayaPlugin.deregisterCommand(LiveLinkSubjectTypesCommandName);
	MayaPlugin.deregisterCommand(LiveLinkAddSelectionCommandName);
	MayaPlugin.deregisterCommand(LiveLinkRemoveSubjectCommandName);
	MayaPlugin.deregisterCommand(LiveLinkChangeSubjectNameCommandName);
	MayaPlugin.deregisterCommand(LiveLinkConnectionStatusCommandName);
	MayaPlugin.deregisterCommand(LiveLinkChangeSubjectStreamTypeCommandName);
	MayaPlugin.deregisterCommand(LiveLinkMessagingSettingsCommand::CommandName);
	MayaPlugin.deregisterCommand(LiveLinkChangeSourceCommandName);
	MayaPlugin.deregisterCommand(LiveLinkSendSubjectListCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetSourceNamesCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetSelectedSourceCommandName);
	MayaPlugin.deregisterCommand(LiveLinkExportStaticDataCommandName);
	MayaPlugin.deregisterCommand(LiveLinkExportFrameDataCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetPluginVersionCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetUnrealVersionCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetPluginAppIdCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetPluginRequestUrlCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetPluginUpdateUrlCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetPluginDocumentationUrlCommandName);
	MayaPlugin.deregisterCommand(LiveLinkOnQuitCommandName);


	ClearViewportCallbacks();
	if (myCallbackIds.length() != 0)
	{
		// Make sure we remove all the callbacks we added
		MMessage::removeCallbacks(myCallbackIds);
	}

	MayaLiveLinkStreamManager::TheOne().ClearSubjects();
	UnrealInitializer::TheOne().StopLiveLink();

	// Make sure the Garbage Collector does not try to remove Delete Listeners on shutdown as those will be invalid causing a crash
	RequestEngineExit(TEXT("MayaUnrealLiveLink uninitializePlugin"));

	UnrealInitializer::TheOne().UninitializeUnreal();

	FTickerTick(1.f);

	MGlobal::executeCommandOnIdle("MayaUnrealLiveLinkRefreshUI");

	const MStatus MayaStatusResult = MS::kSuccess;
	return MayaStatusResult;
}
