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

#include "Roles/MayaLiveLinkAnimSequenceRole.h"
#include "Roles/MayaLiveLinkTimelineTypes.h"

#include "LiveLinkTypes.h"

#define LOCTEXT_NAMESPACE "LiveLinkRole"

/**
* UMayaLiveLinkAnimSequenceRole
*/
UScriptStruct* UMayaLiveLinkAnimSequenceRole::GetStaticDataStruct() const
{
	return FMayaLiveLinkAnimSequenceStaticData::StaticStruct();
}

UScriptStruct* UMayaLiveLinkAnimSequenceRole::GetFrameDataStruct() const
{
	return FMayaLiveLinkAnimSequenceFrameData::StaticStruct();
}

FText UMayaLiveLinkAnimSequenceRole::GetDisplayName() const
{
	return LOCTEXT("AnimSequenceRole", "AnimSequence");
}

bool UMayaLiveLinkAnimSequenceRole::IsStaticDataValid(const FLiveLinkStaticDataStruct& InStaticData,
												  bool& bOutShouldLogWarning) const
{
	bool bResult = Super::IsStaticDataValid(InStaticData, bOutShouldLogWarning);
	if (bResult)
	{
		const FMayaLiveLinkAnimSequenceStaticData* StaticData = InStaticData.Cast<FMayaLiveLinkAnimSequenceStaticData>();
		bResult = StaticData && StaticData->BoneParents.Num() == StaticData->BoneNames.Num();
	}
	return bResult;
}

bool UMayaLiveLinkAnimSequenceRole::IsFrameDataValid(const FLiveLinkStaticDataStruct& InStaticData,
												 const FLiveLinkFrameDataStruct& InFrameData,
												 bool& bOutShouldLogWarning) const
{
	return Super::IsFrameDataValid(InStaticData, InFrameData, bOutShouldLogWarning);
}

#undef LOCTEXT_NAMESPACE
