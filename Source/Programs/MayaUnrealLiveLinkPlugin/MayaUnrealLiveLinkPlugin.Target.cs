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

using UnrealBuildTool;
using System;
using System.IO;
#if !UE_5_0_OR_LATER
using Tools.DotNETCommon;
#else
using EpicGames.Core;
#endif
using System.Collections.Generic;
using System.Runtime.CompilerServices;


[SupportedPlatforms("Win64", "Linux")]
public class MayaUnrealLiveLinkPluginTarget : TargetRules
{
	/// <summary>
	/// Finds the innermost parent directory with the provided name. Search is case insensitive.
	/// </summary>
	string InnermostParentDirectoryPathWithName(string ParentName, string CurrentPath)
	{
		DirectoryInfo ParentInfo = Directory.GetParent(CurrentPath);

		if (ParentInfo == null)
		{
			throw new DirectoryNotFoundException("Could not find parent folder '" + ParentName + "'");
		}

		// Case-insensitive check of the parent folder name.
		if (ParentInfo.Name.ToLower() == ParentName.ToLower())
		{
			return ParentInfo.ToString();
		}

		return InnermostParentDirectoryPathWithName(ParentName, ParentInfo.ToString());
	}

	/// <summary>
	/// Returns the path to this .cs file.
	/// </summary>
	string GetCallerFilePath([CallerFilePath] string CallerFilePath = "")
	{
		if (CallerFilePath.Length == 0)
		{
			throw new FileNotFoundException("Could not find the path of our .cs file");
		}

		return CallerFilePath;
	}

	public MayaUnrealLiveLinkPluginTarget(TargetInfo Target) : base(Target)
	{
		Init(Target, "");
	}

	public MayaUnrealLiveLinkPluginTarget(TargetInfo Target, string InMayaVersionString) : base(Target)
	{
		Init(Target, InMayaVersionString);
	}
	
	public void Init(TargetInfo Target, string InMayaVersionString)
	{
		Type = TargetType.Program;

		bShouldCompileAsDLL = true;
		LinkType = TargetLinkType.Monolithic;
		SolutionDirectory = "Programs/LiveLink";
		string MllName = "MayaUnrealLiveLinkPlugin";
		LaunchModuleName = MllName + InMayaVersionString;

		#if UE_5_0_OR_LATER
			string UEVersion = "5_0";
		#else
			string UEVersion = "4_27";
		#endif // UE_5_0_OR_LATER

		// We only need minimal use of the engine for this plugin
		bBuildDeveloperTools = false;
		bUseMallocProfiler = false;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
		bCompileICU = false;
		bHasExports = true;
		

		bBuildInSolutionByDefault = false;

		// This .cs file must be inside the source folder of this Program. We later use this to find other key directories.
		string TargetFilePath = GetCallerFilePath();

		// We need to avoid failing to load DLL due to looking for EngineDir() in non-existent folders.
		// By having it build in the same directory as the engine, it will assume the engine is in the same directory
		// as the program, and because this folder always exists, it will not fail the check inside EngineDir().

		// Because this is a Program, we assume that this target file resides under a "Programs" folder.
		string ProgramsDir = InnermostParentDirectoryPathWithName("Programs", TargetFilePath);

		// We assume this Program resides under a Source folder.
		string SourceDir = InnermostParentDirectoryPathWithName("Source", ProgramsDir);

		// The program is assumed to reside inside the "Engine" folder.
		string EngineDir = InnermostParentDirectoryPathWithName("Engine", SourceDir);

		// The default Binaries path is assumed to be a sibling of "Source" folder.
		string DefaultBinDir = Path.GetFullPath(Path.Combine(SourceDir, "..", "Binaries", Platform.ToString()));

		// We assume that the engine exe resides in Engine/Binaries/[Platform]
		string EngineBinariesDir = Path.Combine(EngineDir, "Binaries", Platform.ToString());

		// Now we calculate the relative path between the default output directory and the engine binaries,
		// in order to force the output of this program to be in the same folder as th engine.
		ExeBinariesSubFolder = (new DirectoryReference(EngineBinariesDir)).MakeRelativeTo(new DirectoryReference(DefaultBinDir));

		// Setting this is necessary since we are creating the binaries outside of Restricted.
		bLegalToDistributeBinary = true;

		// We still need to copy the resources, so at this point we might as well copy the files where the default Binaries folder was meant to be.
		// MayaUnrealLiveLinkPlugin.xml will be unaware of how the files got there.

		// Add a post-build step that copies the output to a file with the .mll extension
		string OutputName = LaunchModuleName;
		if (Target.Configuration != UnrealTargetConfiguration.Development)
		{
			OutputName = string.Format("{0}-{1}-{2}", OutputName, Target.Platform, Target.Configuration);
			MllName = string.Format("{0}-{1}-{2}", MllName, Target.Platform, Target.Configuration);
		}

		string PostBuildBinDir = Path.Combine(DefaultBinDir, "Maya", InMayaVersionString);
		string SrcOutputFileName = string.Format(@"{0}\{1}.dll", PostBuildBinDir, OutputName);
		string DstOutputFileName = string.Format(@"{0}\{1}_{2}.mll", PostBuildBinDir, MllName, UEVersion);

		if (Target.Platform.ToString() == "Linux")
		{
			OutputName = "lib" + OutputName;
			MllName = "lib" + MllName;
			SrcOutputFileName = string.Format(@"{0}\{1}.so", EngineBinariesDir, OutputName);
			DstOutputFileName = string.Format(@"{0}\{1}_{2}.so", PostBuildBinDir, MllName, UEVersion);
		}

		// Copy binaries
		PostBuildSteps.Add(string.Format("echo Copying {0} to {1}...", EngineBinariesDir, PostBuildBinDir));
		PostBuildSteps.Add(string.Format("xcopy /y /i /v \"{0}\\{1}.*\" \"{2}\" 1>nul", EngineBinariesDir, OutputName, PostBuildBinDir));

		// Copy dll as mll
		PostBuildSteps.Add(string.Format("echo Copying {0} to {1}...", SrcOutputFileName, DstOutputFileName));
		PostBuildSteps.Add(string.Format("copy /Y \"{0}\" \"{1}\" 1>nul", SrcOutputFileName, DstOutputFileName));
	}
}
