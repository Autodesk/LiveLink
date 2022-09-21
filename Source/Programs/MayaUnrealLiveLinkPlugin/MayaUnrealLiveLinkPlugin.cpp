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

#include "INetworkMessagingExtension.h"
#include "MayaLiveLinkInterface.h"

#include "Features/IModularFeatures.h"

#include "Interfaces/IPv4/IPv4Endpoint.h"

#include "Shared/UdpMessagingSettings.h"

#include "UnrealInitializer/FUnrealStreamManager.h"
#include "UnrealInitializer/UnrealInitializer.h"

#include <thread>

IMPLEMENT_APPLICATION(MayaUnrealLiveLinkPlugin, "MayaUnrealLiveLinkPlugin");

THIRD_PARTY_INCLUDES_START
#include <maya/MAnimMessage.h>
#include <maya/MArgDatabase.h>
#include <maya/MArgList.h>
#include <maya/MAnimUtil.h>
#include <maya/MCameraMessage.h>
#include <maya/M3dView.h>
#include <maya/MDagMessage.h>
#include <maya/MDagPathArray.h>
#include <maya/MDGMessage.h>
#include <maya/MDistance.h>
#include <maya/MEventMessage.h>
#include <maya/MSceneMessage.h>
#include <maya/MTimerMessage.h>
#include <maya/MUiMessage.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnCamera.h>
#include <maya/MFnKeyframeDelta.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnMotionPath.h>
#include <maya/MObjectArray.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
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

void OnTimeChangedReceived(const FQualifiedFrameTime& Time);
void ClearViewportCallbacks();
MStatus RefreshViewportCallbacks();

MCallbackIdArray myCallbackIds;

MSpace::Space G_TransformSpace = MSpace::kTransform;

static bool PreviousConnectionStatus = false;
static bool ChangeTimeDone = true;
static bool IgnoreAllDagChangesCallback = false;

// PluginVersion definition has been moved to MayaLiveLinkInterface.cpp
static std::string PluginVersion;
const char PluginAppId[] = "3726213941804942083";

static const char OtherUEVersionLoadedEnvVar[] = "OtherUEVersionLoaded";

void RebuildStreamSubjects(void* ClientData)
{
	auto& LiveLinkStreamManager = MayaLiveLinkStreamManager::TheOne();
	if (PreviousConnectionStatus)
	{
		LiveLinkStreamManager.RebuildSubjects(false, true);

		// Wait a bit after rebuilding the subject data before sending the curve data to Unreal.
		// Otherwise, Unreal will ignore it.
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(100ms);

		LiveLinkStreamManager.StreamSubjects();
	}
}

void OnConnectionStatusChanged()
{
	auto LiveLinkProvider = FUnrealStreamManager::TheOne().GetLiveLinkProvider();
	if (LiveLinkProvider.IsValid())
	{
		const bool bHasConnection = LiveLinkProvider->HasConnection();
		if (PreviousConnectionStatus != bHasConnection)
		{
			MGlobal::executeCommand("MayaUnrealLiveLinkRefreshConnectionUI");

			PreviousConnectionStatus = bHasConnection;

			MGlobal::executeTaskOnIdle(RebuildStreamSubjects, nullptr, MGlobal::kVeryLowIdlePriority);
		}
	}		
}

void PrintInfoToMaya(const char* Info, int Severity)
{
	switch (Severity)
	{
		default:
		case 0:
			MGlobal::displayInfo(Info);
			break;
		case 1:
			MGlobal::displayWarning(Info);
			break;
		case 2:
			MGlobal::displayError(Info);
			break;
	}
}

void PutEnv(const char* EnvString, const char* EnvValue)
{
	if (EnvString && EnvValue)
	{
#if PLATFORM_WINDOWS
		_putenv_s(EnvString, EnvValue);
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
		_putenv_s(EnvString, "");
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

inline void FTickerTick(float ElapsedTime)
{
	FTSTicker::GetCoreTicker().Tick(ElapsedTime);
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

const MString LiveLinkSubjectLinkedAssetsCommandName("LiveLinkSubjectLinkedAssets");

class LiveLinkSubjectLinkedAssetsCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkSubjectLinkedAssetsCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MStringArray Assets;
		MayaLiveLinkStreamManager::TheOne().GetSubjectLinkedAssets(Assets);

		const auto SubjectsLen = Assets.length();
		for (unsigned int i = 0; i < SubjectsLen; ++i)
		{
			const auto& Entry = Assets[i];
			appendToResult(Entry);
		}

		return MS::kSuccess;
	}
};

const MString LiveLinkSubjectTargetAssetsCommandName("LiveLinkSubjectTargetAssets");

class LiveLinkSubjectTargetAssetsCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkSubjectTargetAssetsCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MStringArray Assets;
		MayaLiveLinkStreamManager::TheOne().GetSubjectTargetAssets(Assets);

		const auto SubjectsLen = Assets.length();
		for (unsigned int i = 0; i < SubjectsLen; ++i)
		{
			const auto& Entry = Assets[i];
			appendToResult(Entry);
		}
		return MS::kSuccess;
	}
};

const MString LiveLinkSubjectLinkStatusCommandName("LiveLinkSubjectLinkStatus");

class LiveLinkSubjectLinkStatusCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkSubjectLinkStatusCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MStringArray Assets;
		MayaLiveLinkStreamManager::TheOne().GetSubjectLinkStatus(Assets);

		const auto SubjectsLen = Assets.length();
		for (unsigned int i = 0; i < SubjectsLen; ++i)
		{
			const auto& Entry = Assets[i];
			appendToResult(Entry);
		}
		return MS::kSuccess;
	}
};

const MString LiveLinkSubjectClassesCommandName("LiveLinkSubjectClasses");

class LiveLinkSubjectClassesCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkSubjectClassesCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MStringArray Assets;
		MayaLiveLinkStreamManager::TheOne().GetSubjectClasses(Assets);

		const auto SubjectsLen = Assets.length();
		for (unsigned int i = 0; i < SubjectsLen; ++i)
		{
			const auto& Entry = Assets[i];
			appendToResult(Entry);
		}
		return MS::kSuccess;
	}
};

const MString LiveLinkSubjectUnrealNativeClassesCommandName("LiveLinkSubjectUnrealNativeClasses");

class LiveLinkSubjectUnrealNativeClassesCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkSubjectUnrealNativeClassesCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MStringArray Assets;
		MayaLiveLinkStreamManager::TheOne().GetSubjectUnrealNativeClasses(Assets);

		const auto SubjectsLen = Assets.length();
		for (unsigned int i = 0; i < SubjectsLen; ++i)
		{
			const auto& Entry = Assets[i];
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

		auto LiveLinkProvider = FUnrealStreamManager::TheOne().GetLiveLinkProvider();
		if(LiveLinkProvider.IsValid())
		{
			if (LiveLinkProvider->HasConnection())
			{
				ConnectionStatus = "Connected";
				bConnection = true;
			}
			else
			{
				ConnectionStatus = "No Connection";
			}
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

const MString LiveLinkGetAssetsByClassCommandName("LiveLinkGetAssetsByClass");

class LiveLinkGetAssetsByClassCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetAssetsByClassCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		auto LiveLinkProvider = FUnrealStreamManager::TheOne().GetLiveLinkProvider();
		if (!LiveLinkProvider.IsValid() ||
			!LiveLinkProvider->HasConnection())
		{
			displayError("Live Link provider invalid or not connected.");
			appendToResult("");
			return MS::kFailure;
		}

		MSyntax Syntax;
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kBoolean);

		MArgDatabase argData(Syntax, args);

		MString AssetClass;
		argData.getCommandArgument(0, AssetClass);
		bool SearchSubClasses;
		argData.getCommandArgument(1, SearchSubClasses);

		TMap<FString, FStringArray> UnrealAssets;
		if (LiveLinkProvider->GetAssetsByClass(AssetClass.asChar(), SearchSubClasses, UnrealAssets))
		{
			int StartIndex = 0;
			for (auto& Pair : UnrealAssets)
			{
				auto& StringArray = Pair.Value.Array;

				// Class name
				appendToResult(TCHAR_TO_ANSI(*Pair.Key));

				// Object path start index and number of objects for the current class
				appendToResult(StartIndex);
				appendToResult(StringArray.Num());
				StartIndex += StringArray.Num();

				// Object paths
				for (auto& ObjectPath : StringArray)
				{
					appendToResult(TCHAR_TO_ANSI(*ObjectPath));
				}
			}
		}
		else
		{
			appendToResult("Timeout!");
		}

		return MS::kSuccess;
	}
};

const MString LiveLinkGetAssetsByParentClassCommandName("LiveLinkGetAssetsByParentClass");

class LiveLinkGetAssetsByParentClassCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetAssetsByParentClassCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		auto LiveLinkProvider = FUnrealStreamManager::TheOne().GetLiveLinkProvider();
		if (!LiveLinkProvider.IsValid() ||
			!LiveLinkProvider->HasConnection())
		{
			displayError("Live Link provider invalid or not connected.");
			appendToResult("");
			return MS::kFailure;
		}

		MSyntax Syntax;
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kBoolean);
		Syntax.addArg(MSyntax::kString);

		MArgDatabase argData(Syntax, args);

		MString AssetClass;
		argData.getCommandArgument(0, AssetClass);
		bool SearchSubClasses;
		argData.getCommandArgument(1, SearchSubClasses);
		MString ParentClassesString;
		argData.getCommandArgument(2, ParentClassesString);

		TArray<FString> ParentClasses;
		FString ArrayString = ParentClassesString.asChar();
		ArrayString.ParseIntoArray(ParentClasses, TEXT(","));

		FStringArray UnrealAssets;
		FStringArray NativeAssetClasses;
		if (LiveLinkProvider->GetAssetsByParentClass(AssetClass.asChar(), SearchSubClasses, ParentClasses, UnrealAssets, NativeAssetClasses))
		{
			int StartIndex = 0;
			for (FString& Asset : UnrealAssets.Array)
			{
				// Class name
				appendToResult(TCHAR_TO_ANSI(*Asset));
			}
			if (UnrealAssets.Array.Num() == 0)
			{
				appendToResult("");
			}
			else
			{
				appendToResult("|");
				for (FString& Class : NativeAssetClasses.Array)
				{
					// Native class name
					appendToResult(TCHAR_TO_ANSI(*Class));
				}
			}
		}
		else
		{
			appendToResult("Timeout!");
		}

		return MS::kSuccess;
	}
};

const MString LiveLinkGetActorsByClassCommandName("LiveLinkGetActorsByClass");

class LiveLinkGetActorsByClassCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetActorsByClassCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		auto LiveLinkProvider = FUnrealStreamManager::TheOne().GetLiveLinkProvider();
		if (!LiveLinkProvider.IsValid() ||
			!LiveLinkProvider->HasConnection())
		{
			displayError("Live Link provider invalid or not connected.");
			appendToResult("");
			return MS::kFailure;
		}

		MSyntax Syntax;
		Syntax.addArg(MSyntax::kString);

		MArgDatabase argData(Syntax, args);

		MString AssetClass;
		argData.getCommandArgument(0, AssetClass);

		TMap<FString, FStringArray> UnrealAssets;
		if (LiveLinkProvider->GetActorsByClass(AssetClass.asChar(), UnrealAssets))
		{
			int StartIndex = 0;
			for (auto& Pair : UnrealAssets)
			{
				auto& StringArray = Pair.Value.Array;

				// Class name
				appendToResult(TCHAR_TO_ANSI(*Pair.Key));

				// Object path start index and number of objects for the current class
				appendToResult(StartIndex);
				appendToResult(StringArray.Num());
				StartIndex += StringArray.Num();

				// Object paths
				for (auto& ObjectPath : StringArray)
				{
					appendToResult(TCHAR_TO_ANSI(*ObjectPath));
				}
			}
		}
		else
		{
			appendToResult("Timeout!");
		}

		return MS::kSuccess;
	}
};

const MString LiveLinkGetAnimSequencesBySkeletonCommandName("LiveLinkGetAnimSequencesBySkeleton");

class LiveLinkGetAnimSequencesBySkeletonCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkGetAnimSequencesBySkeletonCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		auto LiveLinkProvider = FUnrealStreamManager::TheOne().GetLiveLinkProvider();
		if (!LiveLinkProvider.IsValid() ||
			!LiveLinkProvider->HasConnection())
		{
			displayError("Live Link provider invalid or not connected.");
			appendToResult("");
			return MS::kFailure;
		}

		TMap<FString, FStringArray> UnrealAssets;
		if (LiveLinkProvider->GetAnimSequencesBySkeleton(UnrealAssets))
		{
			int StartIndex = 0;
			for (auto& Pair : UnrealAssets)
			{
				auto& StringArray = Pair.Value.Array;

				// Class name
				appendToResult(TCHAR_TO_ANSI(*Pair.Key));

				// Object path start index and number of objects for the current class
				appendToResult(StartIndex);
				appendToResult(StringArray.Num());
				StartIndex += StringArray.Num();

				// Object paths
				for (auto& ObjectPath : StringArray)
				{
					appendToResult(TCHAR_TO_ANSI(*ObjectPath));
				}
			}
		}
		else
		{
			appendToResult("Timeout!");
		}

		return MS::kSuccess;
	}
};

const MString LiveLinkLinkUnrealAssetCommandName("LiveLinkLinkUnrealAsset");

class LiveLinkLinkUnrealAssetCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkLinkUnrealAssetCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MSyntax Syntax;
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kString);
		Syntax.addArg(MSyntax::kBoolean);

		MArgDatabase argData(Syntax, args);

		MString SubjectPath;
		argData.getCommandArgument(0, SubjectPath);

		IMStreamedEntity::LinkAssetInfo LinkInfo;
		argData.getCommandArgument(1, LinkInfo.UnrealAssetPath);
		argData.getCommandArgument(2, LinkInfo.UnrealAssetClass);
		argData.getCommandArgument(3, LinkInfo.SavedAssetPath);
		argData.getCommandArgument(4, LinkInfo.SavedAssetName);
		argData.getCommandArgument(5, LinkInfo.UnrealNativeClass);
		argData.getCommandArgument(6, LinkInfo.bSetupOnly);

		MayaLiveLinkStreamManager::TheOne().LinkUnrealAsset(SubjectPath, LinkInfo);

		return MS::kSuccess;
	}
};

const MString LiveLinkUnlinkUnrealAssetCommandName("LiveLinkUnlinkUnrealAsset");

class LiveLinkUnlinkUnrealAssetCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkUnlinkUnrealAssetCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		MSyntax Syntax;
		Syntax.addArg(MSyntax::kString);

		MArgDatabase argData(Syntax, args);

		MString SubjectPath;
		argData.getCommandArgument(0, SubjectPath);

		MayaLiveLinkStreamManager::TheOne().UnlinkUnrealAsset(SubjectPath);

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

				UnrealInitializer::TheOne().StartLiveLink(OnConnectionStatusChanged, OnTimeChangedReceived);
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
			UnrealInitializer::TheOne().StartLiveLink(OnConnectionStatusChanged, OnTimeChangedReceived);
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
		MString Version = PluginVersion.c_str();
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
		appendToResult(MString("https://help.autodesk.com/view/MAYAUL/2023/ENU/?guid=UnrealLiveLink_unreal_livelink_landing_html"));

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

const MString LiveLinkPluginUninitializedCommandName("LiveLinkPluginUninitialized");

class LiveLinkPluginUninitializedCommand : public MPxCommand
{
public:
	static void		cleanup() {}
	static void*	creator() { return new LiveLinkPluginUninitializedCommand(); }

	MStatus			doIt(const MArgList& args) override
	{
		PreviousConnectionStatus = false;

		return MS::kSuccess;
	}
};

const MString LiveLinkPlayheadSyncCommandName("LiveLinkPlayheadSync");

class LiveLinkPlayheadSyncCommand : public MPxCommand
{
	static bool bPlayHeadSync;

public:
	static constexpr char EnableFlag[] = "en";
	static constexpr char EnableFlagLong[] = "enable";

	static void		cleanup() {}
	static void*	creator() { return new LiveLinkPlayheadSyncCommand(); }

	static MSyntax CreateSyntax()
	{
		MStatus Status;
		MSyntax Syntax;

		Syntax.enableQuery(true);

		Status = Syntax.addFlag(EnableFlag, EnableFlagLong, MSyntax::kBoolean);
		CHECK_MSTATUS(Status);

		return Syntax;
	}

	MStatus doIt(const MArgList& args) override
	{
		MStatus Status;
		MArgDatabase ArgData(syntax(), args, &Status);
		CHECK_MSTATUS_AND_RETURN_IT(Status);

		const bool bIsPlayheadSyncEnabled = ArgData.isFlagSet(EnableFlagLong, &Status);
		if (ArgData.isQuery())
		{
			setResult(bPlayHeadSync);
		}
		else
		{
			ArgData.getFlagArgument(EnableFlagLong, 0, bPlayHeadSync);
			setResult(true);
		}

		return MS::kSuccess;
	}

	static bool IsEnabled() { return bPlayHeadSync; }
};
constexpr char LiveLinkPlayheadSyncCommand::EnableFlag[];
constexpr char LiveLinkPlayheadSyncCommand::EnableFlagLong[];
bool LiveLinkPlayheadSyncCommand::bPlayHeadSync = true;

const MString LiveLinkPauseAnimSyncCommandName("LiveLinkPauseAnimSync");

class LiveLinkPauseAnimSyncCommand : public MPxCommand
{
	static bool bPausedState;

public:
	static constexpr char EnableFlag[] = "en";
	static constexpr char EnableFlagLong[] = "enable";

	static void		cleanup() {}
	static void* creator() { return new LiveLinkPauseAnimSyncCommand(); }

	static MSyntax CreateSyntax()
	{
		MStatus Status;
		MSyntax Syntax;

		Syntax.enableQuery(true);

		Status = Syntax.addFlag(EnableFlag, EnableFlagLong, MSyntax::kBoolean);
		CHECK_MSTATUS(Status);

		return Syntax;
	}

	MStatus doIt(const MArgList& args) override
	{
		MStatus Status;
		MArgDatabase ArgData(syntax(), args, &Status);
		CHECK_MSTATUS_AND_RETURN_IT(Status);

		const bool bIsPauseEnabled = ArgData.isFlagSet(EnableFlagLong, &Status);
		if (ArgData.isQuery())
		{
			setResult(bPausedState);
		}
		else
		{
			bool NewState = false; 
			ArgData.getFlagArgument(EnableFlagLong, 0, NewState);
			bool RebuildSubjects = bPausedState && !NewState;
			bPausedState = NewState;
			MayaLiveLinkStreamManager::TheOne().PauseAnimSequenceStreaming(bPausedState);
			MayaUnrealLiveLinkUtils::RefreshUI();

			// If we were in paused state, we will need to rebuild stream subjects.
			if (RebuildSubjects)
			{
				MGlobal::executeTaskOnIdle(RebuildStreamSubjects, nullptr, MGlobal::kVeryLowIdlePriority);
			}
			setResult(true);
		}

		return MS::kSuccess;
	}

	static bool IsEnabled() { return bPausedState; }
};
constexpr char LiveLinkPauseAnimSyncCommand::EnableFlag[];
constexpr char LiveLinkPauseAnimSyncCommand::EnableFlagLong[];
bool LiveLinkPauseAnimSyncCommand::bPausedState = false;

void OnMayaExit(void* client)
{
	MayaLiveLinkStreamManager::TheOne().ClearSubjects();
}

void OnScenePreOpen(void* Client)
{
	ClearViewportCallbacks();
	MayaLiveLinkStreamManager::TheOne().Reset();
	MayaUnrealLiveLinkUtils::RefreshUI();
}

void OnScenePreNew(void* Client)
{
	OnScenePreOpen(Client);
}

void OnSceneOpen(void* Client)
{
	MGlobal::executeCommandOnIdle("MayaUnrealLiveLinkOnSceneOpen");
}

bool CameraManipStarted = false;
bool AnimCurveEdited = false;
bool AnimKeyFrameEdited = false;
MTime::Unit CurrentTimeUnit = MTime::kInvalid;
bool gTimeChangedReceived = false;
FQualifiedFrameTime TimeReceived;
std::atomic<bool> SendUpdatedData {false};

// Helper method to send data to unreal when SendUpdatedData is set.
void StreamDataToUnreal()
{
	// Stream data only when this flag is set.
	if (!SendUpdatedData)
	{
		return;
	}

	// Do we need this?
	if (gTimeChangedReceived)
	{
		gTimeChangedReceived = false;
		return;
	}

	auto& StreamManager = MayaLiveLinkStreamManager::TheOne();
	auto TimeUnit = MAnimControl::currentTime().unit();
	if (TimeUnit != CurrentTimeUnit)
	{
		CurrentTimeUnit = TimeUnit;
		StreamManager.OnTimeUnitChanged();
	}

	if (!CameraManipStarted && !AnimCurveEdited && !AnimKeyFrameEdited)
	{
		auto LiveLinkProvider = FUnrealStreamManager::TheOne().GetLiveLinkProvider();
		if (LiveLinkPlayheadSyncCommand::IsEnabled() &&
			LiveLinkProvider.IsValid() &&
			LiveLinkProvider->HasConnection())
		{
			LiveLinkProvider->OnTimeChanged(MayaUnrealLiveLinkUtils::GetMayaFrameTimeAsUnrealTime());

			// Need to sleep the thread so that the OnTimeChanged message is sent to Unreal
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(20ms);
		}

		StreamManager.StreamSubjects();

	}
	else
	{
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

	// Set the streaming flag to false.
	SendUpdatedData = false;
}

void StreamOnIdleTask(void* ClientData)
{
	if (!ClientData)
	{
		return;
	}

	std::shared_ptr<IMStreamedEntity> Subject = *static_cast<std::shared_ptr<IMStreamedEntity>*>(ClientData);
	double StreamTime = FPlatformTime::Seconds();
	auto FrameNumber = MAnimControl::currentTime().value();
	Subject->OnStream(StreamTime, FrameNumber);
}

void StreamOnIdle(std::shared_ptr<IMStreamedEntity>& Subject, MGlobal::MIdleTaskPriority Priority)
{
	MGlobal::executeTaskOnIdle(StreamOnIdleTask, &Subject, Priority);
}

void OnTimeChanged(MTime& Time, void* ClientData)
{
	SendUpdatedData = true;
	AnimCurveEdited  = false;
	AnimKeyFrameEdited = false;
}

int FindMatchingDagPath(const MString& DagPathName, const MFnDagNode& DagNode, const MStringArray& SubjectPaths, MDagPath& SubjectDagPath)
{
	int PathIndex = -1;
	const auto SubjectPathLen = SubjectPaths.length();
	for (unsigned int path = 0; path < SubjectPathLen; ++path)
	{
		const MString& SubjectPath = SubjectPaths[path];

		MSelectionList SelectionList;
		SelectionList.add(SubjectPath);
		MObject SubjectObj;
		SelectionList.getDependNode(0, SubjectObj);
		SelectionList.getDagPath(0, SubjectDagPath);

		if (SubjectPath == DagPathName)
		{
			PathIndex = path;
			break;
		}
		else
		{
			MDagPath ShapeDagPath = SubjectDagPath;
			ShapeDagPath.extendToShape();
			if (ShapeDagPath.fullPathName() == DagPathName)
			{
				PathIndex = path;
				break;
			}
			else if (PathIndex == -1 && DagNode.isChildOf(SubjectObj))
			{
				PathIndex = path;
			}
		}
	}

	return PathIndex;
}

void OnAnimCurveEdited(MObjectArray& Objects, void* ClientData)
{
	auto& MayaStreamManager = MayaLiveLinkStreamManager::TheOne();

	bool bInternalUpdate = ClientData && (*reinterpret_cast<bool*>(ClientData));

	// Get the list of tracked subjects
	MStringArray SubjectPaths;
	MayaStreamManager.GetSubjectPaths(SubjectPaths);
	const auto SubjectPathLen = SubjectPaths.length();
	if (SubjectPathLen == 0)
	{
		return;
	}

	MayaStreamManager.OnPreAnimCurvesEdited();

	struct UnrealTrackInfo
	{
		const char* Name;
		double ConversionFactor = 1.0;
	};

	static const std::map<std::string, const char*> CommonNames =
	{
		{ "translateX", "LocationX" },
		{ "translateY", "LocationZ" },
		{ "translateZ", "LocationY" },
		{ "rotateX", "RotationY" },
		{ "rotateY", "RotationZ" },
		{ "rotateZ", "RotationX" },
		{ "scaleX", "ScaleX" },
		{ "scaleY", "ScaleY" },
		{ "scaleZ", "ScaleZ" },
		{ "visibility", "bHidden" }
	};

	static const std::map<std::string, const char*> CommonNamesZup =
	{
		{ "translateX", "LocationX" },
		{ "translateY", "LocationY" },
		{ "translateZ", "LocationZ" },
		{ "rotateX", "RotationX" },
		{ "rotateY", "RotationY" },
		{ "rotateZ", "RotationZ" },
		{ "scaleX", "ScaleX" },
		{ "scaleY", "ScaleY" },
		{ "scaleZ", "ScaleZ" },
		{ "visibility", "bHidden" }
	};

	const double InchToMM = MDistance(1.0, MDistance::kInches).asMillimeters();
	static const std::map<std::string, UnrealTrackInfo> CameraNames =
	{
		{ "fStop", { "CurrentAperture" } },
		{ "focalLength", { "CurrentFocalLength" } },
		{ "horizontalFilmAperture", { "Filmback.SensorWidth", InchToMM } },
		{ "verticalFilmAperture", { "Filmback.SensorHeight", InchToMM } },
		{ "focusDistance", { "FocusSettings.ManualFocusDistance" } },
	};

	static const std::map<std::string, const char*> LightNames =
	{
		{ "colorR", "LightColorR" },
		{ "colorG", "LightColorG" },
		{ "colorB", "LightColorB" },
		{ "intensity", "Intensity" },
//		{ "coneAngle","OuterConeAngle"}, // Will be supported by MAYA-121680
	};

	auto& CurveCommonNames = MGlobal::isYAxisUp() ? CommonNames : CommonNamesZup;

	auto MatchName = [&](const std::string& MayaName,
						 double& ConversionFactor) -> const char*
	{
		auto Found = CurveCommonNames.find(MayaName);
		if (Found != CurveCommonNames.end())
		{
			return Found->second;
		}
		Found = LightNames.find(MayaName);
		if (Found != LightNames.end())
		{
			return Found->second;
		}
		auto CineCameraNameFound = CameraNames.find(MayaName);
		if (CineCameraNameFound != CameraNames.end())
		{
			ConversionFactor = CineCameraNameFound->second.ConversionFactor;
			return CineCameraNameFound->second.Name;
		}
		return nullptr;
	};

	MDagPathArray DagPathArray;
	MStatus Status;
	auto Length = Objects.length();
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

				MObjectArray Targets;
				MPlugArray SrcPlugArray;
				Connection.connectedTo(SrcPlugArray, false, true);
				for (unsigned int src = 0; src < SrcPlugArray.length(); ++src)
				{
					MPlug Plug = SrcPlugArray[src];
					MObject Node = Plug.node();

					// Check for a motion path
					if (Node.hasFn(MFn::kMotionPath))
					{
						MFnMotionPath Path(Node);
						MDagPathArray AnimatedObjects;
						Path.getAnimatedObjects(AnimatedObjects);
						bool bFound = false;
						for (unsigned int Parent = 0; Parent < AnimatedObjects.length() && !bFound; ++Parent)
						{
							MFnDagNode DagNode(AnimatedObjects[Parent]);
							
							MDagPath SubjectDagPath;
							int PathIndex = FindMatchingDagPath(AnimatedObjects[Parent].fullPathName(), DagNode, SubjectPaths, SubjectDagPath);
							if (PathIndex >= 0 && SubjectDagPath.isValid())
							{
								MPlugArray AnimatedPlugs;
								MAnimUtil::findAnimatedPlugs(SubjectDagPath, AnimatedPlugs, true);
								for (unsigned int AnimPlugIdx = 0; AnimPlugIdx < AnimatedPlugs.length() && !bFound; ++AnimPlugIdx)
								{
									Plug = AnimatedPlugs[AnimPlugIdx];

									MPlugArray SrcAnimatedPlugs;
									AnimatedPlugs[AnimPlugIdx].connectedTo(SrcAnimatedPlugs, true, false);
									for (unsigned int Anim = 0; Anim < SrcAnimatedPlugs.length(); ++Anim)
									{
										MObject SrcAnimatedObject = SrcAnimatedPlugs[Anim].node();
										if (SrcAnimatedObject.hasFn(MFn::kMotionPath) &&
											SrcAnimatedObject == Node)
										{
											Node = AnimatedObjects[Parent].node();
											bFound = true;
											break;
										}
									}
								}
							}
						}
					}
					// Check for a constraint
					else if (Node.hasFn(MFn::kTransform))
					{
						bool bNodeAffectedByConstraintFound = false;
						MFnDependencyNode DependNode(Node);
						MObject Constraint;
						MPlug ParentMatrixPlugArray = DependNode.findPlug("parentMatrix", false);
						if (!ParentMatrixPlugArray.isNull() && ParentMatrixPlugArray.isArray())
						{
							for (unsigned int ParentMatrixIndex = 0; ParentMatrixIndex < ParentMatrixPlugArray.numElements() && !bNodeAffectedByConstraintFound; ++ParentMatrixIndex)
							{
								MPlug DependPlug = ParentMatrixPlugArray[ParentMatrixIndex];
								MPlugArray DependConnections;
								DependPlug.connectedTo(DependConnections, false, true);
								for (unsigned int DependIdx = 0; DependIdx < DependConnections.length() && !bNodeAffectedByConstraintFound; ++DependIdx)
								{
									MPlug DependConnection = DependConnections[DependIdx];
									MObject DependObject = DependConnection.node();
									if (DependObject.hasFn(MFn::kConstraint))
									{
										for (unsigned int path = 0; path < SubjectPathLen; ++path)
										{
											MString& SubjectPath = SubjectPaths[path];

											MSelectionList SelectionList;
											SelectionList.add(SubjectPath);
											MDagPath SubjectDagPath;
											MObject SubjectObj;
											SelectionList.getDependNode(0, SubjectObj);
											SelectionList.getDagPath(0, SubjectDagPath);
											MFnDagNode DagNode(SubjectDagPath);
											if (DagNode.isParentOf(DependObject))
											{
												Node = SubjectDagPath.node();
												bNodeAffectedByConstraintFound = true;
												break;
											}
										}
									}
								}
							}
						}
					}

					if (Node.hasFn(MFn::kDagNode))
					{
						MFnDagNode DagNode(Node);

						MDagPath DagPath;
						if (DagNode.getPath(DagPath))
						{
							MString DagPathName;
							DagPathName = DagPath.fullPathName();

							// Check if the AnimCurve is linked to a blendshape outside the Subject hierarchy
							auto FindBlendShapeOwner = [&MayaStreamManager](const MPlug& Plug,
																			auto& FindBlendShapeOwnerRef) -> IMStreamedEntity*
							{
								IMStreamedEntity* Owner = nullptr;

								MPlugArray PlugArray;
								Plug.connectedTo(PlugArray, false, true);
								for (unsigned int PlugIndex = 0; PlugIndex < PlugArray.length() && !Owner; ++PlugIndex)
								{
									MObject BlendShapeObject = PlugArray[PlugIndex].node();
									if (BlendShapeObject.hasFn(MFn::kBlendShape))
									{
										MFnBlendShapeDeformer BlendShape(BlendShapeObject);
										MPlug WeightPlug = BlendShape.findPlug("weight", false);
										if (!WeightPlug.isNull())
										{
											Owner = MayaStreamManager.GetSubjectOwningBlendShape(BlendShape.name());
											if (Owner)
											{
												break;
											}
										}
									}
									else if (BlendShapeObject.hasFn(MFn::kTransform))
									{
										MPlugArray TransformConnections;
										PlugArray[PlugIndex].connectedTo(TransformConnections, false, true);
										for (unsigned int Src = 0; Src < TransformConnections.length() && !Owner; ++Src)
										{
											Owner = FindBlendShapeOwnerRef(TransformConnections[Src], FindBlendShapeOwnerRef);
										}
									}
								}
								return Owner;
							};

							IMStreamedEntity* SubjectOwningBlendshape = FindBlendShapeOwner(Plug, FindBlendShapeOwner);

							auto SubjectAnimCurveEdited = [&Plug, &CurveCommonNames, &MatchName, &bInternalUpdate,
														   &Obj, &DagPathArray](IMStreamedEntity* Subject,
																				const MDagPath& SubjectDagPath)
							{
								auto Attribute = Plug.attribute();
								MFnAttribute Attrib(Attribute);

								double ConversionFactor = 1.0;
								const char* AttribName = Attrib.name().asChar();
								const char* UnrealName = MatchName(AttribName, ConversionFactor);

								// If UnrealName is null, assume it's a custom attribute to be used in a blueprint.
								Subject->OnAnimCurveEdited(UnrealName ? UnrealName : AttribName, Obj, Plug, ConversionFactor);
								MayaUnrealLiveLinkUtils::AddUnique(SubjectDagPath, DagPathArray);
								if (!bInternalUpdate)
								{
									AnimCurveEdited = true;
								}
							};

							if (SubjectOwningBlendshape)
							{
								SubjectAnimCurveEdited(SubjectOwningBlendshape, SubjectOwningBlendshape->GetDagPath());
							}
							else
							{
								MDagPath SubjectDagPath;
								int PathIndex = FindMatchingDagPath(DagPathName, DagNode, SubjectPaths, SubjectDagPath);
								if (PathIndex >= 0 && SubjectDagPath.isValid())
								{
									IMStreamedEntity* Subject = MayaStreamManager.GetSubjectByDagPath(SubjectPaths[PathIndex]);
									if (Subject)
									{
										SubjectAnimCurveEdited(Subject, SubjectDagPath);
									}
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

	if (!bInternalUpdate)
	{
		SendUpdatedData = true;
	}
}

void OnAnimKeyframeEdited(MObjectArray& Objects, void* ClientData)
{
	auto& MayaStreamManager = MayaLiveLinkStreamManager::TheOne();

	AnimKeyFrameEdited = true;

	// Get the list of tracked subjects
	MStringArray SubjectPaths;
	MayaStreamManager.GetSubjectPaths(SubjectPaths);

	const auto NumSubjectPaths = SubjectPaths.length();
	if (NumSubjectPaths == 0)
	{
		return;
	}

	auto Length = Objects.length();
	MDagPathArray DagPathArray;

	for (unsigned int index = 0; index < Length; ++index)
	{
		MObject& KeyFrameObj = Objects[index];
		if (KeyFrameObj.hasFn(MFn::kKeyframeDelta))
		{
			MStatus Status;
			MFnKeyframeDelta KeyFrameDelta(KeyFrameObj);
			auto Obj = KeyFrameDelta.paramCurve(&Status);

			if (Status && !Obj.isNull() && Obj.hasFn(MFn::kAnimCurve))
			{
				MFnAnimCurve AnimCurve(Obj);
				MPlugArray Connections;
				AnimCurve.getConnections(Connections);

				for (unsigned int i = 0; i < Connections.length(); ++i)
				{
					const MPlug& Connection = Connections[i];
					MPlugArray SrcPlugArray;
					Connection.connectedTo(SrcPlugArray, false, true);
					for (unsigned int src = 0; src < SrcPlugArray.length(); ++src)
					{
						MPlug Plug = SrcPlugArray[src];
						MObject Node = Plug.node();
						IMStreamedEntity* SubjectOwningBlendShape = nullptr;

						if (Node.hasFn(MFn::kIkHandle))
						{
							MFnDependencyNode DependNode(Node);
							MPlug StartJointPlug = DependNode.findPlug("startJoint", false);
							if (!StartJointPlug.isNull())
							{
								MPlugArray PlugArray;
								StartJointPlug.connectedTo(PlugArray, true, false);
								for (unsigned int PlugIdx = 0; PlugIdx < PlugArray.length(); ++PlugIdx)
								{
									MPlug& DstPlug = PlugArray[PlugIdx];
									MObject DstObject = DstPlug.node();
									if (DstObject.hasFn(MFn::kJoint))
									{
										Node = DstObject;
										break;
									}
								}
							}
						}
						else if (Node.hasFn(MFn::kTransform))
						{
							auto UpdateNodeAndPlugForBlendShape = [&MayaStreamManager](const MPlug& Plug,
																					   auto& UpdateNodeAndPlugForBlendShapeRef,
																					   MObject& OutNode,
																					   MPlug& OutPlug) -> IMStreamedEntity*
							{
								IMStreamedEntity* Owner = nullptr;
								MPlugArray TransformSrcPlugs;
								Plug.connectedTo(TransformSrcPlugs, false, true);
								for (unsigned int Src = 0; Src < TransformSrcPlugs.length(); ++Src)
								{
									const MPlug& SrcPlug = TransformSrcPlugs[Src];
									MObject SrcObject = SrcPlug.node();
									if (SrcObject.hasFn(MFn::kBlendShape))
									{
										MFnBlendShapeDeformer BlendShape(SrcObject);
										Owner = MayaStreamManager.GetSubjectOwningBlendShape(BlendShape.name());
										if (Owner)
										{
											OutNode = SrcObject;
											OutPlug = SrcPlug;
											return Owner;
										}
									}
									else if (SrcObject.hasFn(MFn::kTransform))
									{
										Owner = UpdateNodeAndPlugForBlendShapeRef(SrcPlug,
																				  UpdateNodeAndPlugForBlendShapeRef,
																				  OutNode,
																				  OutPlug);
										if (Owner)
										{
											return Owner;
										}
									}
								}

								return Owner;
							};
							
							SubjectOwningBlendShape = UpdateNodeAndPlugForBlendShape(Plug, UpdateNodeAndPlugForBlendShape, Node, Plug);
						}

						if (Node.hasFn(MFn::kHikIKEffector))
						{
							// Try to match the InputCharacterDefinition from the effector to the one of this subject
							IMStreamedEntity* Subject = MayaLiveLinkStreamManager::TheOne().GetSubjectByHikIKEffector(Node);
							if (Subject)
							{
								Subject->OnAnimKeyframeEdited(AnimCurve.name(), Obj, Plug);
								MayaUnrealLiveLinkUtils::AddUnique(Subject->GetDagPath(), DagPathArray);
							}
						}
						else if (Node.hasFn(MFn::kDagNode))
						{
							MFnDagNode DagNode(Node);

							MDagPath DagPath;
							if (DagNode.getPath(DagPath))
							{
								MString DagPathName;
								DagPathName = DagPath.fullPathName();

								MDagPath SubjectDagPath;
								int PathIndex = FindMatchingDagPath(DagPathName, DagNode, SubjectPaths, SubjectDagPath);
								if (PathIndex >= 0 && SubjectDagPath.isValid())
								{
									if (IMStreamedEntity* Subject = MayaStreamManager.GetSubjectByDagPath(SubjectPaths[PathIndex]))
									{
										Subject->OnAnimKeyframeEdited(AnimCurve.name(), Obj, Plug);
										MayaUnrealLiveLinkUtils::AddUnique(SubjectDagPath, DagPathArray);
									}
								}
							}
						}
						else if (Node.hasFn(MFn::kBlendShape))
						{
							MFnBlendShapeDeformer BlendShape(Node);
							IMStreamedEntity* Subject = SubjectOwningBlendShape ? SubjectOwningBlendShape :
																				  MayaStreamManager.GetSubjectOwningBlendShape(BlendShape.name());
							if (Subject)
							{
								Subject->OnAnimKeyframeEdited(MayaUnrealLiveLinkUtils::GetPlugAliasName(Plug), Obj, Plug);
								MayaUnrealLiveLinkUtils::AddUnique(Subject->GetDagPath(), DagPathArray);
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

void ChangeTime(void* ClientData)
{
	gTimeChangedReceived = true;

	FFrameRate MayaFrameRate = MayaUnrealLiveLinkUtils::GetMayaFrameRateAsUnrealFrameRate();
	double FrameTime = 0.0;
	if (MayaFrameRate != TimeReceived.Rate)
	{
		FrameTime = ConvertFrameTime(TimeReceived.Time, TimeReceived.Rate, MayaFrameRate).AsDecimal();
	}
	else
	{
		FrameTime = TimeReceived.Time.AsDecimal();
	}

	MAnimControl::setCurrentTime(MTime(FMath::FloorToDouble(FrameTime + KINDA_SMALL_NUMBER), MAnimControl::currentTime().unit()));
	ChangeTimeDone = true;
}

void OnTimeChangedReceived(const FQualifiedFrameTime& Time)
{
	if (!LiveLinkPlayheadSyncCommand::IsEnabled())
	{
		return;
	}

	// Make sure to only queue 1 ChangeTime event since TimeReceived could be modified
	// while a previous ChangeTime is currently happening
	if (ChangeTimeDone)
	{
		TimeReceived = Time;

		MGlobal::executeTaskOnIdle(ChangeTime, nullptr, MGlobal::kLowIdlePriority);
		ChangeTimeDone = false;
	}
}

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
}

void OnCameraEndManip(MObject& Node, void* ClientData)
{
	if (!FUnrealStreamManager::TheOne().GetLiveLinkProvider().IsValid())
	{
		return;
	}

	CameraManipStarted = false;
}

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

	auto& MayaStreamManager = MayaLiveLinkStreamManager::TheOne();

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

	StreamDataToUnreal();
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

std::atomic<std::chrono::time_point<std::chrono::steady_clock>> PlaybackRangeChangedId;
std::atomic<bool> PlaybackRangeThreadStarted(false);
TUniquePtr<class FDetectIdleEvent> DetectIdleEvent = nullptr;

class FDetectIdleEvent : public FRunnable
{
public:
	FDetectIdleEvent();

	virtual ~FDetectIdleEvent();

	virtual bool Init() override { return true; }
	uint32 Run() override;
	void Stop() override;

private:
	FRunnableThread* Thread;
	bool bRunThread;
};

FDetectIdleEvent::FDetectIdleEvent()
{
	Thread = FRunnableThread::Create(this, TEXT("DetectIdleEvent"));
}

FDetectIdleEvent::~FDetectIdleEvent()
{
	if (Thread)
	{
		Thread->Kill();
		delete Thread;
	}
}

uint32 FDetectIdleEvent::Run()
{
	using namespace std::chrono_literals;

	// Pause the thread until we're sure that no other playback range change occurs
	while (std::chrono::duration<double>(std::chrono::steady_clock::now() - PlaybackRangeChangedId.load()).count() < 2.0 &&
		   bRunThread)
	{
		FPlatformProcess::Sleep(0.01);
	}

	// Rebuild the subjects
	MGlobal::executeTaskOnIdle(RebuildStreamSubjects, nullptr, MGlobal::kHighIdlePriority);

	// End the thread
	PlaybackRangeThreadStarted.store(false);

	return 0;
}

void FDetectIdleEvent::Stop()
{
	bRunThread = false;
}

void OnPlaybackRangeChanged(void* ClientData)
{
	if (MayaLiveLinkStreamManager::TheOne().GetNumberOfSubjects() == 0)
	{
		return;
	}

	// On playback range changed, we are starting a worker thread that will wait
	// to see if another playback range changed happens shortly after.
	// If it does, the wait timer will be reset.
	// If it doesn't, we will rebuild the subjects with the new playback range.

	// Update the timer, which will reset how long the thread will wait
	PlaybackRangeChangedId.store(std::chrono::steady_clock::now());
	if (!PlaybackRangeThreadStarted.load())
	{
		if (DetectIdleEvent.IsValid())
		{
			DetectIdleEvent->Stop();
			DetectIdleEvent.Release();
		}

		// Start the worker thread that will wait for additional user input before rebuilding the subjects
		PlaybackRangeThreadStarted.store(true);
		DetectIdleEvent = MakeUnique<FDetectIdleEvent>();
		DetectIdleEvent->Init();
	}
}

void BeforeSaveCallback(void* ClientData)
{
	IgnoreAllDagChangesCallback = true;

	MGlobal::executeCommand("MayaUnrealLiveLinkOnScenePreSave");
}

void AfterSaveCallback(void* ClientData)
{
	IgnoreAllDagChangesCallback = false;
}

void AllDagChangesCallback(MDagMessage::DagMessage MsgType,
						   MDagPath& Child,
						   MDagPath& Parent,
						   void* ClientData)
{
	// Update the UI when a parent is added/removed to update the dag paths
	if (!IgnoreAllDagChangesCallback &&
		(MsgType == MDagMessage::kParentAdded ||
		 MsgType == MDagMessage::kParentRemoved))
	{
		// Verify if we need to register a callback for the new parent
		if (MsgType == MDagMessage::kParentAdded && Child.isValid() && Parent.isValid() && Parent.length() != 0)
		{
			std::vector<IMStreamedEntity*> Subjects;
			MayaLiveLinkStreamManager::TheOne().GetSubjectsFromParentPath(Child, Subjects);

			MFnDagNode ChildDagNode(Child);
			for (IMStreamedEntity* Subject : Subjects)
			{
				const MDagPath& SubjectPath = Subject->GetDagPath();
				bool bRegisterNode = SubjectPath == Child;
				if (!bRegisterNode)
				{
					MObject DagNode = SubjectPath.node();
					bRegisterNode = ChildDagNode.isParentOf(DagNode);
				}
				if (bRegisterNode)
				{
					MObject ParentObject = Parent.node();
					Subject->RegisterParentNode(ParentObject);
				}
				
			}
		}

		MayaUnrealLiveLinkUtils::RefreshUI();
	}
}

/**
* This function is called by Maya when the plugin becomes loaded
*
* @param	MayaPluginObject	The Maya object that represents our plugin
*
* @return	MS::kSuccess if everything went OK and the plugin is ready to use
*/
MStatus initializePlugin(MObject MayaPluginObject)
{
	PluginVersion = TCHAR_TO_ANSI(*FMayaLiveLinkInterfaceModule::GetPluginVersion());

	// Tell Maya about our plugin
	MFnPlugin MayaPlugin(
		MayaPluginObject,
		"Autodesk, Inc.",
		PluginVersion.c_str());

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
			MGlobal::displayWarning("Unable to load Unreal 5.x Live Link for Maya plug-in, because the Unreal 4.27 version of the same plug-in is/was already loaded.");

			// Execute the command that will check for the auto-load status, change it for the current plugin and tell the user
			// that Maya needs to be restarted for the change to take effect.
			MGlobal::executeCommandOnIdle("MayaLiveLinkNotifyAndQuit \"" + MayaPlugin.name() + "\" \"" + PrevPluginName + "\"", false);

			MayaUnrealLiveLinkUtils::RefreshUI();

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
	UnrealInitializer::TheOne().StartLiveLink(OnConnectionStatusChanged, OnTimeChangedReceived);

	// We do not tick the core engine but we need to tick the ticker to make sure the message
	// bus endpoint in LiveLinkProvider is up to date.
	FTickerTick(1.0f);
	MayaLiveLinkStreamManager::TheOne().Reset();

	MCallbackId MayaExitingCallbackId = MSceneMessage::addCallback(MSceneMessage::kMayaExiting, (MMessage::MBasicFunction)OnMayaExit);
	myCallbackIds.append(MayaExitingCallbackId);

	MCallbackId ScenePreOpenedCallbackId = MSceneMessage::addCallback(MSceneMessage::kBeforeOpen, (MMessage::MBasicFunction)OnScenePreOpen);
	myCallbackIds.append(ScenePreOpenedCallbackId);
	MCallbackId SceneOpenedCallbackId = MSceneMessage::addCallback(MSceneMessage::kAfterOpen, (MMessage::MBasicFunction)OnSceneOpen);
	myCallbackIds.append(SceneOpenedCallbackId);

	MCallbackId ScenePreNewCallbackId = MSceneMessage::addCallback(MSceneMessage::kBeforeNew, (MMessage::MBasicFunction)OnScenePreNew);
	myCallbackIds.append(ScenePreNewCallbackId);

	myCallbackIds.append(MSceneMessage::addCallback(MSceneMessage::kBeforeSave, BeforeSaveCallback));
	myCallbackIds.append(MSceneMessage::addCallback(MSceneMessage::kAfterSave, AfterSaveCallback));

	MCallbackId TimeChangedCallbackId = MDGMessage::addTimeChangeCallback(OnTimeChanged);
	myCallbackIds.append(TimeChangedCallbackId);

	MCallbackId	PlaybackRangeChangeCallbackId = MEventMessage::addEventCallback("playbackRangeChanged", OnPlaybackRangeChanged);
	myCallbackIds.append(PlaybackRangeChangeCallbackId);

	MCallbackId AnimCurveEditedCallbackId = MAnimMessage::addAnimCurveEditedCallback(OnAnimCurveEdited);
	myCallbackIds.append(AnimCurveEditedCallbackId);
	MCallbackId AnimKeyframeEditedCallbackId = MAnimMessage::addAnimKeyframeEditedCallback(OnAnimKeyframeEdited);
	myCallbackIds.append(AnimKeyframeEditedCallbackId);

	// Update function every 5 seconds
	MCallbackId timerCallback = MTimerMessage::addTimerCallback(5.f, (MMessage::MElapsedTimeFunction)OnInterval);
	myCallbackIds.append(timerCallback);

	MCallbackId	CallbackId = MDagMessage::addAllDagChangesCallback(AllDagChangesCallback);
	myCallbackIds.append(CallbackId);

	MayaPlugin.registerCommand(LiveLinkSubjectNamesCommandName, LiveLinkSubjectNamesCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectPathsCommandName, LiveLinkSubjectPathsCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectRolesCommandName, LiveLinkSubjectRolesCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectTypesCommandName, LiveLinkSubjectTypesCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectLinkedAssetsCommandName, LiveLinkSubjectLinkedAssetsCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectTargetAssetsCommandName, LiveLinkSubjectTargetAssetsCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectLinkStatusCommandName, LiveLinkSubjectLinkStatusCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectClassesCommandName, LiveLinkSubjectClassesCommand::creator);
	MayaPlugin.registerCommand(LiveLinkSubjectUnrealNativeClassesCommandName, LiveLinkSubjectUnrealNativeClassesCommand::creator);
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
	MayaPlugin.registerCommand(LiveLinkGetAssetsByClassCommandName, LiveLinkGetAssetsByClassCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetAssetsByParentClassCommandName, LiveLinkGetAssetsByParentClassCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetActorsByClassCommandName, LiveLinkGetActorsByClassCommand::creator);
	MayaPlugin.registerCommand(LiveLinkGetAnimSequencesBySkeletonCommandName, LiveLinkGetAnimSequencesBySkeletonCommand::creator);
	MayaPlugin.registerCommand(LiveLinkLinkUnrealAssetCommandName, LiveLinkLinkUnrealAssetCommand::creator);
	MayaPlugin.registerCommand(LiveLinkUnlinkUnrealAssetCommandName, LiveLinkUnlinkUnrealAssetCommand::creator);
	MayaPlugin.registerCommand(LiveLinkPluginUninitializedCommandName, LiveLinkPluginUninitializedCommand::creator);
	MayaPlugin.registerCommand(LiveLinkPlayheadSyncCommandName,
							   LiveLinkPlayheadSyncCommand::creator,
							   LiveLinkPlayheadSyncCommand::CreateSyntax);
	MayaPlugin.registerCommand(LiveLinkPauseAnimSyncCommandName, LiveLinkPauseAnimSyncCommand::creator,
							   LiveLinkPauseAnimSyncCommand::CreateSyntax);

	MGlobal::executeCommandOnIdle("MayaUnrealLiveLinkInitialized");

	// Print to Maya's output window, too!
	MGlobal::displayInfo("MayaUnrealLiveLinkPlugin initialized");

	MGlobal::executeCommandOnIdle("SetCommandCategory");

	RefreshViewportCallbacks();

	MayaUnrealLiveLinkUtils::RefreshUI();

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
MStatus uninitializePlugin(MObject MayaPluginObject)
{
	DetectIdleEvent.Release();

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
	MayaPlugin.deregisterCommand(LiveLinkSubjectLinkedAssetsCommandName);
	MayaPlugin.deregisterCommand(LiveLinkSubjectTargetAssetsCommandName);
	MayaPlugin.deregisterCommand(LiveLinkSubjectLinkStatusCommandName);
	MayaPlugin.deregisterCommand(LiveLinkSubjectClassesCommandName);
	MayaPlugin.deregisterCommand(LiveLinkSubjectUnrealNativeClassesCommandName);
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
	MayaPlugin.deregisterCommand(LiveLinkGetAssetsByClassCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetAssetsByParentClassCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetActorsByClassCommandName);
	MayaPlugin.deregisterCommand(LiveLinkGetAnimSequencesBySkeletonCommandName);
	MayaPlugin.deregisterCommand(LiveLinkLinkUnrealAssetCommandName);
	MayaPlugin.deregisterCommand(LiveLinkUnlinkUnrealAssetCommandName);
	MayaPlugin.deregisterCommand(LiveLinkPluginUninitializedCommandName);
	MayaPlugin.deregisterCommand(LiveLinkPlayheadSyncCommandName);
	MayaPlugin.deregisterCommand(LiveLinkPauseAnimSyncCommandName);

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

	MayaUnrealLiveLinkUtils::RefreshUI();

	const MStatus MayaStatusResult = MS::kSuccess;
	return MayaStatusResult;
}
