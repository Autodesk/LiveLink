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

#include "MayaLiveLinkSourceFactory.h"

#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "MayaLiveLinkMessageBusSource.h"
#include "Misc/MessageDialog.h"
#include "SMayaLiveLinkSourceFactory.h"

#define LOCTEXT_NAMESPACE "MayaLiveLinkSourceFactory"

FText UMayaLiveLinkSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "Maya Live Link");
}

FText UMayaLiveLinkSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "Creates a connection to Maya for syncing animation");
}

TSharedPtr<SWidget> UMayaLiveLinkSourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
	return SNew(SMayaLiveLinkSourceFactory)
		.OnSourceSelected(FOnMayaLiveLinkSourceSelected::CreateUObject(this, &UMayaLiveLinkSourceFactory::OnSourceSelected, InOnLiveLinkSourceCreated));
}

TSharedPtr<FLiveLinkMessageBusSource> UMayaLiveLinkSourceFactory::MakeSource(const FText& Name,
																					   const FText& MachineName,
																					   const FMessageAddress& Address,
																					   double TimeOffset) const
{
	return MakeShared<FMayaLiveLinkMessageBusSource>(Name, MachineName, Address, TimeOffset);
}

void UMayaLiveLinkSourceFactory::OnSourceSelected(FProviderPollResultPtr SelectedSource, FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
	if (SelectedSource.IsValid())
	{
#if WITH_EDITOR
		bool bDoesAlreadyExist = false;
		{
			ILiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			TArray<FGuid> Sources = LiveLinkClient.GetSources();
			for (FGuid SourceGuid : Sources)
			{
				if (LiveLinkClient.GetSourceType(SourceGuid).ToString() == SelectedSource->Name)
				{
					bDoesAlreadyExist = true;
					break;
				}
			}
		}

		if (bDoesAlreadyExist)
		{
			if (EAppReturnType::No == FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::Yes, LOCTEXT("AddSourceWithSameName", "This provider name already exist. Are you sure you want to add a new one?")))
			{
				return;
			}
		}
#endif

		TSharedPtr<FLiveLinkMessageBusSource> SharedPtr = MakeSource(FText::FromString(SelectedSource->Name), FText::FromString(SelectedSource->MachineName), SelectedSource->Address, SelectedSource->MachineTimeOffset);
		if (SharedPtr)
		{
			FString ConnectionString = FString::Printf(TEXT("Name=\"%s\""), *(SelectedSource->Name));
			InOnLiveLinkSourceCreated.ExecuteIfBound(SharedPtr, MoveTemp(ConnectionString));
		}
	}
}

#undef LOCTEXT_NAMESPACE