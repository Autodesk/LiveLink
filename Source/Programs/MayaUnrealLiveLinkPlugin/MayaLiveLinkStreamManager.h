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
//Subjects (Note: Do not change the order of includes)
#include "Subjects/MLiveLinkLightSubject.h"
#include "Subjects/MLiveLinkPropSubject.h"
#include "Subjects/MLiveLinkCameraSubject.h"
#include "Subjects/MLiveLinkJointHierarchySubject.h"
#include "Subjects/IMStreamedEntity.h"

//Unreal Includes
#include "CoreMinimal.h"

#include <vector>
#include <memory>

// Boilerplate for importing OpenMaya
#if defined PLATFORM_WINDOWS
#include "Windows/WindowsPlatformCompilerPreSetup.h"
#else
#include "Unix/UnixPlatformCompilerPreSetup.h"
#endif

// Import OpenMaya headers
THIRD_PARTY_INCLUDES_START
#include <maya/MStringArray.h>
THIRD_PARTY_INCLUDES_END

// Forward declarations
struct FQualifiedFrameTime;
struct FLiveLinkLightFrameData;
struct FLiveLinkCameraFrameData;
struct FLiveLinkSkeletonStaticData;
struct FLiveLinkAnimationFrameData;

/*! \class	MayaLiveLinkStreamManager
*		\brief  This class facilitates streaming Maya objects to Unreal Engine via a singleton.
				It provides an interface for Subjects to be streamed to Unreal Live Link provider.
				It also keeps track of subjects that are being streamed to UE.
*/
class MayaLiveLinkStreamManager
{
public:

	//! Singleton object. Use this function to access it.
	static MayaLiveLinkStreamManager& TheOne();

	//! Getter functions for Subjects and their attributes
	void GetSubjectNames(MStringArray& Entries) const;
	void GetSubjectPaths(MStringArray& Entries) const;
	void GetSubjectRoles(MStringArray& Entries) const;
	void GetSubjectTypes(MStringArray& Entries) const;
	IMStreamedEntity* GetSubjectByDagPath(const MString& Path);
	int GetStreamTypeByDagPath(const MString& Path);

	//! AddSubject functions. 
	bool AddSubject(MItDag& DagIterator, const MString& Name = MString(), uint16_t StreamType = -1, int32_t Index = -1);

	template<class SubjectType, typename... ArgsType>
	bool AddSubjectOfType(int Index, ArgsType&&... Args);
	
	void AddPropSubject(const MString& SubjectName, const MDagPath& RootPath, 
		MLiveLinkPropSubject::MPropStreamMode StreamType, int32_t Index);

	void  AddLightSubject(const MString& SubjectName, const MDagPath& RootPath,
		MLiveLinkLightSubject::FLightStreamMode StreamType, int32_t Index);
	
	void  AddCameraSubject(const MString& SubjectName, const MDagPath& RootPath,
		MLiveLinkCameraSubject::MCameraStreamMode StreamType, int32_t Index);

	bool  AddJointHierarchySubject(const MString& SubjectName, const MDagPath& RootPath,
		MLiveLinkJointHierarchySubject::MCharacterStreamMode StreamType, int32_t Index);

	//! Functions check and manipulate subjects or subject data. 
	void ValidateSubjects(bool NeedToRefreshUI);
	bool IsInSubjectList(const MString& DagPath) const;
	int  RemoveSubject(const MString& PathOfSubjectToRemove);
	bool ChangeSubjectName(const MString& SubjectDagPath, const MString& NewName);
	void ChangeStreamType(const MString& SubjectPathIn, const MString& StreamTypeIn);

	//! Operations on SubjectList
	void ClearSubjects();
	void Reset();
	void RebuildSubjects(bool NeedToRefreshUI = true);

	//! Callback listeners
	void OnConnectionStatusChanged();
	void OnAttributeChanged(const MDagPath& DagPath, const MObject& Object, const MPlug& Plug, const MPlug& OtherPlug);

	//! Export static and frame(animated) data to JSON
	bool ExportSubjectStaticDataToJSON(const MString& SubjectDagPath,
									   const MString& FilePath);

	bool ExportSubjectFrameDataToJSON(const MString& SubjectDagPath,
									  const MString& FilePath,
									  double FrameTime);

	//! Make a unique name for a subject being added
	MString MakeUniqueName(const MString& SubjectName);

	//! Stream all subjects to LL provider
	void StreamSubjects() const;

	void StreamSubject(const MDagPath& DagPath) const;

	//! Remove a subject from LL provider
	void RemoveSubjectFromLiveLink(const MString& SubjectName);

	//! Prop Subject static and frame data streaming
	bool RebuildPropSubjectData(const MString& SubjectName, const MString& StreamMode);
	void OnStreamPropSubject(const MString& SubjectName, const MString& StreamMode);

	//! Light Subject static and frame data streaming
	bool RebuildLightSubjectData(const MString& SubjectName, const MString& StreamMode);
	void OnStreamLightSubject(const MString& SubjectName, const MString& StreamMode);

	//! Base Camera Subject static and frame data streaming
	bool RebuildBaseCameraSubjectData(const MString& SubjectName, const MString& StreamMode);
	void StreamCamera(const MString& SubjectName, const MString& StreamMode);

	//! Camera Subject static and frame data streaming
	bool RebuildCameraSubjectData(const MString& SubjectName, const MString& StreamMode);

	//! Joint Hierarchy Subject static and frame data streaming
	bool RebuildJointHierarchySubject(const MString& SubjectName, const MString& StreamMode);
	void OnStreamJointHierarchySubject(const MString& SubjectName, const MString& StreamMode);

	//! Active camera
	MDagPath GetActiveCameraSubjectPath() const;
	void SetActiveCameraDagPath(const MDagPath& DagPath);

	//! Initialize and get the reference to working static data used by UnrealStreamManger that would be sent to UE
	template<typename T>
	T& InitializeAndGetStaticDataFromUnreal();

	//! Initialize and get the reference to working frame data used by UnrealStreamManger that would be sent to UE
	template<typename T>
	T& InitializeAndGetFrameDataFromUnreal();

private:
	//! Private constructor. Access to the members is provided by TheOne()
	MayaLiveLinkStreamManager();

	//! List of streamed subjects.
	std::vector<std::shared_ptr<IMStreamedEntity>> StreamedSubjects;
};
