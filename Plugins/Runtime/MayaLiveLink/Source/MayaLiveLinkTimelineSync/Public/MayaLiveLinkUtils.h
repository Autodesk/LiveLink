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

#include "CoreMinimal.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/UnrealString.h"
#include "Misc/PackageName.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogMayaLiveLink, Log, All);

namespace FMayaLiveLinkUtils
{
	template<typename T>
	T* FindAsset(const FString& Path, const FString& Name = FString())
	{
		if (Path.IsEmpty())
		{
			return nullptr;
		}

		// If we found a package, try and get the primary asset from it
		if (UPackage* FoundPackage = FindPackage(nullptr, *Path))
		{
			if (auto PotentialAsset = StaticFindObject(UObject::StaticClass(),
													   FoundPackage,
													   Name.IsEmpty() ?
													   (*FPackageName::GetShortName(FoundPackage)) :
													   (*Name)))
			{
				return Cast<T>(PotentialAsset);
			}
		}

		return nullptr;
	}

	template<typename T>
	T* FindAssetInRegistry(const FString& PackagePath, const FString& AssetName)
	{
		TArray<FAssetData> OutAssetData;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		auto& AssetRegistry = AssetRegistryModule.Get();

		// Find the asset by its name and package path
		AssetRegistry.GetAssetsByPath(FName(PackagePath), OutAssetData);
		for (auto& Asset : OutAssetData)
		{
			if (Asset.AssetName.ToString() == AssetName)
			{
				return Cast<T>(Asset.GetAsset());
			}
		}
		return nullptr;
	}

	void RefreshContentBrowser(const UObject& Object);

	template<typename T>
	T* FindObject(const FString& ObjectName)
	{
		UObject* Object = FindFirstObject<UField>(*ObjectName, EFindFirstObjectOptions::EnsureIfAmbiguous);
		if (Object != nullptr)
		{
			return Cast<T>(Object);
		}

		FCoreRedirectObjectName CoreRedirectObjectName;
		if (!FCoreRedirects::RedirectNameAndValues(ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Type_Struct | ECoreRedirectFlags::Type_Enum,
												   FCoreRedirectObjectName(ObjectName),
												   CoreRedirectObjectName,
												   nullptr,
												   ECoreRedirectMatchFlags::None))
		{
			return nullptr;
		}

		const FString RedirectedObjectName = CoreRedirectObjectName.ObjectName.ToString();
		UPackage* Package = nullptr;
		if (!CoreRedirectObjectName.PackageName.IsNone())
		{
			Package = FindPackage(nullptr, *CoreRedirectObjectName.PackageName.ToString());
		}

		if (Package != nullptr)
		{
			Object = FindObject<UField>(Package, *RedirectedObjectName);
		}

		if (Package == nullptr || Object == nullptr)
		{
			Object = FindFirstObject<UField>(*RedirectedObjectName, EFindFirstObjectOptions::EnsureIfAmbiguous);
		}
		return Cast<T>(Object);
	}
}
