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

#include "SMayaLiveLinkSourceFactory.h"

#include "LiveLinkMessageBusFinder.h"
#include "MayaLiveLinkModule.h"
#include "MayaLiveLinkPresenceDetector.h"

#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "MayaLiveLinkSourceFactory"

struct FMayaLiveLinkSource
{
	FProviderPollResultPtr Result;
	double Time = 0.0;

	FMayaLiveLinkSource(FProviderPollResultPtr InResult)
	: Result(MoveTemp(InResult))
	, Time(FApp::GetCurrentTime()) 
	{
	}
};

namespace
{
	bool operator==(const FProviderPollResult& LHS, const FProviderPollResult& RHS)
	{
		return LHS.Name == RHS.Name && LHS.MachineName == RHS.MachineName;
	}

	const FName TypeColumnName(TEXT("Type"));
	const FName MachineColumnName(TEXT("Machine"));
	const double UIRefreshRate = 0.5;
}

class SMayaLiveLinkProviderRow : public SMultiColumnTableRow<FProviderPollResultPtr>
{
public:
	SLATE_BEGIN_ARGS(SMayaLiveLinkProviderRow) {}
		SLATE_ARGUMENT(FProviderPollResultPtr, ResultPtr)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& TableView)
	{
		ResultPtr = Args._ResultPtr;

		SMultiColumnTableRow<FProviderPollResultPtr>::Construct(FSuperRowType::FArguments()
																	.Padding(1.0f),
																TableView);

		if (!ResultPtr->bIsValidProvider)
		{
			SetToolTipText(LOCTEXT("InvalidProvider", "Invalid provider, please make sure you are using the latest Unreal Engine Version"));
			SetEnabled(false);
		}
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == TypeColumnName)
		{
			return SNew(STextBlock)
						.Text(FText::FromString(ResultPtr->Name));
		}
		else if (ColumnName == MachineColumnName)
		{
			return SNew(STextBlock)
						.Text(FText::FromString(ResultPtr->MachineName));
		}

		return SNullWidget::NullWidget;
	}

private:
	FProviderPollResultPtr ResultPtr;
};

SMayaLiveLinkSourceFactory::~SMayaLiveLinkSourceFactory()
{
	if (FMayaLiveLinkModule* ModulePtr = FModuleManager::GetModulePtr<FMayaLiveLinkModule>("MayaLiveLink"))
	{
		ModulePtr->GetPresenceDetector().RemovePresenceRequest();
	}
}

void SMayaLiveLinkSourceFactory::Construct(const FArguments& Args)
{
	OnSourceSelected = Args._OnSourceSelected;

	FMayaLiveLinkModule::Get().GetPresenceDetector().AddPresenceRequest();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SBox)
			.HeightOverride(200)
			.WidthOverride(200)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FMayaLiveLinkSource>>)
				.OnSelectionChanged(this, &SMayaLiveLinkSourceFactory::OnSelectionChanged)
				.OnGenerateRow(this, &SMayaLiveLinkSourceFactory::CreateListView)
				.SelectionMode(ESelectionMode::SingleToggle)
				.ListItemsSource(&Sources)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(TypeColumnName)
						.FillWidth(50.0f)
						.DefaultLabel(LOCTEXT("MayaLiveLinkTypeName", "Type"))
					+ SHeaderRow::Column(MachineColumnName)
						.FillWidth(50.0f)
						.DefaultLabel(LOCTEXT("MayaLiveLinkMachineName", "Machine Name"))
				)
			]
		]
	];
}

void SMayaLiveLinkSourceFactory::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (FApp::GetCurrentTime() - LastUpdateTime > UIRefreshRate)
	{
		// Get the sources
		Results.Reset();
		FMayaLiveLinkModule::Get().GetPresenceDetector().GetResults(Results);
		
		// Remove all sources that took too long to send their presence
		Sources.RemoveAllSwap([this](TSharedPtr<FMayaLiveLinkSource> Source)
		{
			return FApp::GetCurrentTime() - Source->Time > 2.0;
		});
		
		for (const FProviderPollResultPtr& Result : Results)
		{
			TSharedPtr<FMayaLiveLinkSource>* FoundSource = Sources.FindByPredicate([&Result](const TSharedPtr<FMayaLiveLinkSource> Source)
			{
				return Source && Result && *Source->Result == *Result;
			});

			if (FoundSource && *FoundSource)
			{
				(*FoundSource)->Time = FApp::GetCurrentTime();
			}
			else
			{
				Sources.Add(MakeShared<FMayaLiveLinkSource>(Result));
			}
		}

		Sources.StableSort([](const TSharedPtr<FMayaLiveLinkSource>& LHS, const TSharedPtr<FMayaLiveLinkSource>& RHS)
		{
			if (LHS.IsValid() && RHS.IsValid() &&
				LHS->Result.IsValid() && RHS->Result.IsValid())
			{
				const int32 CompareTest = LHS->Result->Name.Compare(RHS->Result->Name);
				return CompareTest == 0 ?
					LHS->Result->MachineName.Compare(RHS->Result->MachineName) <= 0 :
					CompareTest <= 0;
			}
			return true;
		});

		// Refresh the list view with the new sources
		ListView->RequestListRefresh();
		LastUpdateTime = FApp::GetCurrentTime();
	}
}

void SMayaLiveLinkSourceFactory::OnSelectionChanged(TSharedPtr<FMayaLiveLinkSource> Source,
															  ESelectInfo::Type Type)
{
	SelectedResult = Source->Result;
	OnSourceSelected.ExecuteIfBound(SelectedResult);
}

TSharedRef<ITableRow> SMayaLiveLinkSourceFactory::CreateListView(TSharedPtr<FMayaLiveLinkSource> Source,
																		   const TSharedRef<STableViewBase>& TableView) const
{
	return SNew(SMayaLiveLinkProviderRow, TableView).ResultPtr(Source->Result);
}

#undef LOCTEXT_NAMESPACE
