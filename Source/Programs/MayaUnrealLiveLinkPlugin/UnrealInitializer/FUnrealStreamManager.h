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

#include "ILiveLinkProducer.h"
#include "LiveLinkTypes.h"

/*! \class	FUnrealStreamManager
*		\brief  This class acts as stream manager to interact with UE.
				This is a singleton and should be accessed by TheOne() function.
				We do not use any Maya objects here. This class should exclusively
				use Unreal's API. Do not use STL in this class and module. 
*/
class FUnrealStreamManager
{
private:

	//! LiveLink source that is being used to stream from Maya to UE. Currently, it can
	//! be either our JSON provider or built in MesssageBus.
	TSharedPtr<class ILiveLinkProducer> LiveLinkProvider;

	//! Member working data structs that can be used by the RebuildSubjectData or
	//! OnStreamSubject function to send the data to LiveLink providers. We give access
	//! to these members to subjects in MayaUnrealLiveLink to set the data that needs to
	//! be sent to UE.
	FLiveLinkStaticDataStruct WorkingStaticData;
	FLiveLinkFrameDataStruct  WorkingFrameData;

	bool bUpdateWhenDisconnected;

public:

	//! Singleton object. Use this function to access it.
	static FUnrealStreamManager& TheOne();

	TSharedPtr<class ILiveLinkProducer> GetLiveLinkProvider();
	bool SetLiveLinkProvider(LiveLinkSource Producer);

	void UpdateWhenDisconnected(bool bUpdate) { bUpdateWhenDisconnected = bUpdate; }
	bool IsUpdateWhenDisconnected() const { return bUpdateWhenDisconnected; }

private:

	//! Private constructor and destructor for this singleton object
	FUnrealStreamManager();
	~FUnrealStreamManager();

	bool HasConnection() const;

public:

	//! Initialize and get the reference to working static data
	template<typename T>
	T& InitializeAndGetStaticData();

	//! Initialize and get the reference to working frame data
	template<typename T>
	T& InitializeAndGetFrameData();

	//! LiveLink functions for Prop subject
	bool RebuildPropSubjectData(const FName& SubjectName, const FString& StreamMode);
	void OnStreamPropSubject(const FName& SubjectName, const FString& StreamMode);

	//! LiveLink functions for Light subject
	bool RebuildLightSubjectData(const FName& SubjectName, const FString& StreamMode);
	void OnStreamLightSubject(const FName& SubjectName, const FString& StreamMode);

	//! LiveLink functions for Base Camera Subject
	bool RebuildBaseCameraSubjectData(const FName& SubjectName, const FString& StreamMode);
	void StreamCamera(const FName& SubjectName, const FString& StreamMode);

	//! LiveLink functions for Camera Subject
	bool RebuildCameraSubjectData(const FName& SubjectName, const FString& StreamMode);

	//! LiveLink functions for Joint Hierarchy Subject
	bool RebuildJointHierarchySubjectData(const FName& SubjectName, const FString& StreamMode);
	void OnStreamJointHierarchySubject(const FName& SubjectName, const FString& StreamMode);

	void RebuildAnimSequence(const FName& SubjectName);
	void OnStreamAnimSequence(const FName& SubjectName);

	void RebuildLevelSequence(const FName& SubjectName);
	void OnStreamLevelSequence(const FName& SubjectName);
};
