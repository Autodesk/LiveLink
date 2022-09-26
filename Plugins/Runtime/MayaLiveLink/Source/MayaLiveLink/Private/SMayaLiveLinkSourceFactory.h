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

#include "LiveLinkMessageBusFinder.h"

#include "Misc/App.h"

#include "Widgets/SCompoundWidget.h"

#include "Widgets/Views/SListView.h"

DECLARE_DELEGATE_OneParam(FOnMayaLiveLinkSourceSelected, FProviderPollResultPtr);

class SMayaLiveLinkSourceFactory : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SMayaLiveLinkSourceFactory) {}
		SLATE_EVENT(FOnMayaLiveLinkSourceSelected, OnSourceSelected)
	SLATE_END_ARGS()

	virtual ~SMayaLiveLinkSourceFactory();

	void Construct(const FArguments& Args);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);

	FProviderPollResultPtr GetSelectedSource() const { return SelectedResult; }

private:
	void OnSelectionChanged(TSharedPtr<struct FMayaLiveLinkSource> Source,
							ESelectInfo::Type Type);

	TSharedRef<class ITableRow> CreateListView(TSharedPtr<struct FMayaLiveLinkSource> Source,
											   const TSharedRef<class STableViewBase>& TableView) const;

private:
	TArray<TSharedPtr<struct FMayaLiveLinkSource>> Sources;

	TSharedPtr<SListView<TSharedPtr<struct FMayaLiveLinkSource>>> ListView;

	FOnMayaLiveLinkSourceSelected OnSourceSelected;

	TArray<FProviderPollResultPtr> Results;
	FProviderPollResultPtr SelectedResult;

	double LastUpdateTime = 0;
};
