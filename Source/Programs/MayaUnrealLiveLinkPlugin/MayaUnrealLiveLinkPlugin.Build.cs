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
using System.IO;
#if !UE_5_0_OR_LATER
using Tools.DotNETCommon;
#else
using EpicGames.Core;
#endif

public class MayaUnrealLiveLinkPlugin : ModuleRules
{
	string MayaVersion;
	public MayaUnrealLiveLinkPlugin(ReadOnlyTargetRules Target, string pMayaVersion="") : base(Target)
	{
		MayaVersion = pMayaVersion;

		// Skip initialization if not building for a specific version
		// This class is used as the base class for the other versions,
        // but must still be instiate by the Unreal build system
		if (GetMayaVersion() == string.Empty)
        {
			return;
        }

		// For LaunchEngineLoop.cpp include.  You shouldn't need to add anything else to this line.
		PrivateIncludePaths.AddRange(new string[]
		{
			"Runtime/Launch/Public",
			"Runtime/Launch/Private"
		});

		// Unreal dependency modules
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"ApplicationCore",
			"Messaging",
			"Networking",
			"Projects",
			"UdpMessaging",
			"LiveLinkInterface",
			"LiveLinkMessageBusFramework",
		});


		//
		// Maya SDK setup
		//
		string MayaInstallFolder = GetMayaInstallFolderPath();
		string MayaIncludePath   = GetMayaIncludePath();
		string MayaLibDir        = GetMayaLibraryPath();

		// Make sure this version of Maya is actually installed
		if (Directory.Exists(MayaInstallFolder))
		{
			PrivateIncludePaths.Add(MayaIncludePath);
//			RapidJSON is now optional, can be added as a submodule in ThirdParty if prouved to be necessary.
//			PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Restricted","NotForLicensees","ThirdParty","rapidjson","include"));

			string MayaLibPrefix = "";
			string MayaLibExtension = "";

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicDefinitions.Add("NT_PLUGIN=1");
				MayaLibExtension = ".lib";
			} else if (Target.Platform == UnrealTargetPlatform.Linux) {
				bool IsLinux = System.Environment.OSVersion.Platform.ToString() == "Unix";
				PublicDefinitions.Add("LINUX=1");
				// For GL/gl.h
				if(IsLinux)
				{
					PrivateIncludePaths.Add("/usr/include");
				} 
				else
				{
					// Build machine don't have Open GL headers installed so let's use a copy of it for cross-compile
					PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Restricted","NotForLicensees","build-scripts"));
				}

				MayaLibPrefix = "lib";
				MayaLibExtension = ".so";
			}

			// Maya libraries we're depending on
			string[] MayaLibs = new string[]
			{
				"Foundation",
				"OpenMaya",
				"OpenMayaAnim",
				"OpenMayaUI"
			};
			foreach(string MayaLib in MayaLibs)
			{
				PublicAdditionalLibraries.Add(Path.Combine(MayaLibDir, MayaLibPrefix + MayaLib + MayaLibExtension));
			}

			PublicDefinitions.Add(
			#if UE_5_0_OR_LATER
				"UE_5_0_OR_LATER=1"
			#else
				"UE_5_0_OR_LATER=0"
			#endif // UE_5_0_OR_LATER
				);

		}
		//else
		//{
		//	throw new BuildException("Couldn't find Autodesk Maya " + MayaVersionString + " in folder '" + MayaInstallFolder + "'.  This version of Maya must be installed for us to find the Maya SDK files.");
		//}
	}

	public string GetMayaVersion() { return MayaVersion; }
	public virtual string GetMayaInstallFolderPath()
	{
		// Try with standard setup
		string Location = System.Environment.GetEnvironmentVariable("MAYA_WIN_DIR_" + GetMayaVersion());

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			Location = System.Environment.GetEnvironmentVariable("MAYA_LNX_DIR_" + GetMayaVersion());
		}
		
		if (Location == null)
		{
			Location = string.Empty;
		}

		if (!Directory.Exists(Location))
		{
			// Try with build machine setup
			string SDKRootEnvVar = System.Environment.GetEnvironmentVariable("UE_SDKS_ROOT");
			if (SDKRootEnvVar != null && SDKRootEnvVar != "")
			{
				Location = Path.Combine(SDKRootEnvVar, "HostWin64", "Win64", "Maya", GetMayaVersion());
			}
		}

		return Location;
	}
	public virtual string GetMayaIncludePath() { return Path.Combine(GetMayaInstallFolderPath(), "include"); }
	public virtual string GetMayaLibraryPath() { return Path.Combine(GetMayaInstallFolderPath(), "lib"); }
}
