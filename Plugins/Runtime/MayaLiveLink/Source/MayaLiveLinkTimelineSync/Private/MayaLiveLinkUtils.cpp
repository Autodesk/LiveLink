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

#include "MayaLiveLinkUtils.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

#include "UObject/UObjectGlobals.h"

void FMayaLiveLinkUtils::RefreshContentBrowser(const UObject& Object)
{
	// Need to refresh the content browser if the current selected folder is the one for the LevelSequence
	// Modifying the LevelSequence doesn't seem to show the dirty "star" flag.
	// Do the update only once per second to avoid freezing the content browser.
	static int32 Time = 0;
	const int32 NewSeconds = FDateTime::Now().GetTimeOfDay().GetTotalSeconds();
	if (NewSeconds != Time && Object.GetPackage())
	{
		Time = NewSeconds;
		UPackage* Package = Object.GetPackage();

		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
		TArray<FString> CurrentSelectedFolders;
		ContentBrowser.GetSelectedPathViewFolders(CurrentSelectedFolders);
		if (CurrentSelectedFolders.Num() == 1)
		{
			const FPackagePath& Path = Package->GetLoadedPath();
			if (!Path.IsEmpty())
			{
				const FString PackageName = FPackageName::FilenameToLongPackageName(Path.GetLocalFullPath());
				const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
				const int32 Index = CurrentSelectedFolders[0].Find(PackagePath);

				// Make sure the end of the string is the full package path.
				// GetSelectedPathViewFolders returns paths with some extra parent folder like "/All/".
				if (Index > 0 && (Index + PackagePath.Len()) == CurrentSelectedFolders[0].Len())
				{
					ContentBrowser.SetSelectedPaths({ PackageName }, true);
				}
			}
		}
	}
}
