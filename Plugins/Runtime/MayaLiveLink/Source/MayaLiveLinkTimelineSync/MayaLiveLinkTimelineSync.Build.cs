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

namespace UnrealBuildTool.Rules
{
	public class MayaLiveLinkTimelineSync : ModuleRules
	{
		public MayaLiveLinkTimelineSync(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
				});

			PrivateIncludePaths.AddRange(
				new string[] {
				});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"Core",
					"LiveLinkInterface"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"UnrealEd",
					"AnimGraph",
					"BlueprintGraph",
					"LiveLink",
					"LiveLinkGraphNode",
					"MayaLiveLinkInterface",
					"LevelSequence",
					"MovieScene",
					"MovieSceneTracks",
					"LevelSequenceEditor",
					"EditorScriptingUtilities",
					"LiveLinkAnimationCore",
					"Sequencer",
					"CinematicCamera",
					"Persona"
				});

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
				});
		}
	}
}