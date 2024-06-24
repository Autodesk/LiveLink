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

#include "MayaLiveLinkStreamManager.h"
#include "MayaUnrealLiveLinkUtils.h"
#include "Subjects/MLiveLinkActiveCamera.h"

#include "UnrealInitializer/FUnrealStreamManager.h"

#include <algorithm>

//! Unreal Includes
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkLightTypes.h"
#include "Roles/LiveLinkCameraTypes.h"
#include "Roles/LiveLinkAnimationTypes.h"

THIRD_PARTY_INCLUDES_START
#include <maya/MAnimControl.h>
#include <maya/MFnDagNode.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MSelectionList.h>
THIRD_PARTY_INCLUDES_END

extern void StreamOnIdle(std::shared_ptr<IMStreamedEntity>& Subject, MGlobal::MIdleTaskPriority Priority);

//======================================================================
//
/*!	\brief	Private constructor. Access the singleton by TheOne().
*/
MayaLiveLinkStreamManager::MayaLiveLinkStreamManager()
{ 
	StreamedSubjects.clear();
	AnimSequenceStreamingPaused = false;
}

//======================================================================
//
/*!	\brief	Access to this singleton object.
*/
MayaLiveLinkStreamManager& MayaLiveLinkStreamManager::TheOne()
{
	static MayaLiveLinkStreamManager SMayaLiveLinkStreamManager;
	return SMayaLiveLinkStreamManager;
}

//======================================================================
//
/*!	\brief	Listener for OnConnectionChange callback. Currently we update the UI.
*/
void MayaLiveLinkStreamManager::OnConnectionStatusChanged()
{
	MGlobal::executeCommand("MayaUnrealLiveLinkRefreshConnectionUI");
}

//======================================================================
//
/*!	\brief	Add a subject to be streamed.
	
	\param[in] DagIterator Maya DAG iterator to traverse DAG
	\param[in] Name		   Subject name
	\param[in] StreamType  Stream type of the subject (default -1)
	\param[in] Index       Index at which subject will be added in StreamedSubjects list (default -1)

	\return	True when subject was added successfully.

*/
bool MayaLiveLinkStreamManager::AddSubject(MItDag& DagIterator, const MString& Name, uint16_t StreamType, int32_t Index)
{
	bool ItemAdded = false;

	MDagPath DagPathBackup;
	DagIterator.getPath(DagPathBackup);

	// first try to find a specific subject item under the selected root item.
	// we iterate through the DAG to find items in groups/sets and to be able to find the Shape compoments which
	// hold the interesting properties.
	bool SubjectNotInList = false;
	while (!DagIterator.isDone() && !ItemAdded)
	{
		MDagPath CurrentItemPath;
		if (!DagIterator.getPath(CurrentItemPath))
			continue;

		MFnDagNode CurrentNode(CurrentItemPath);
		MString SubjectName = Name.length() != 0 ? Name : CurrentNode.name();

		if (CurrentItemPath.hasFn(MFn::kJoint))
		{
			MFnDagNode RootNode(DagPathBackup);

			SubjectName = Name.length() != 0 ? Name : RootNode.name();
			SubjectName = MakeUniqueName(SubjectName);
			if (!IsInSubjectList(DagPathBackup.fullPathName()))
			{

				ItemAdded = AddJointHierarchySubject(SubjectName.asChar(),
					DagPathBackup,
					static_cast<MLiveLinkJointHierarchySubject::MCharacterStreamMode>(StreamType),
					Index);
				SubjectNotInList = true;
			}
			else
			{
				ItemAdded = true;
			}
		}
		else if (CurrentItemPath.hasFn(MFn::kCamera))
		{
			if (!IsInSubjectList(CurrentItemPath.fullPathName()))
			{
				SubjectName = MakeUniqueName(SubjectName);
				AddCameraSubject(SubjectName.asChar(),
					CurrentItemPath,
					static_cast<MLiveLinkCameraSubject::MCameraStreamMode>(StreamType),
					Index);
				SubjectNotInList = true;
			}
			ItemAdded = true;
		}
		else if (CurrentItemPath.hasFn(MFn::kLight))
		{
			if (!IsInSubjectList(CurrentItemPath.fullPathName()))
			{
				SubjectName = MakeUniqueName(SubjectName);
				AddLightSubject(SubjectName.asChar(),
					CurrentItemPath,
					static_cast<MLiveLinkLightSubject::MLightStreamMode>(StreamType),
					Index);
				SubjectNotInList = true;
			}
			ItemAdded = true;
		}

		if (ItemAdded && SubjectNotInList)
		{
			MGlobal::displayInfo(MString("LiveLinkAddSubjectCommand ") + SubjectName);
		}

		DagIterator.next();
	}

	// if there was no specific item, we assume that the selected item is a prop.
	// the props are handled differently because almost everything has a kTransform function set, so if a subject
	// is under a group node or in a set, the group would be added as a prop otherwise.
	if (!ItemAdded)
	{
		if (DagPathBackup.hasFn(MFn::kTransform))
		{
			MFnDagNode DagNode(DagPathBackup);

			MDagPath SubjectPath;
			DagNode.getPath(SubjectPath);

			MString SubjectName = Name.length() != 0 ? Name : DagNode.name();
			if (!IsInSubjectList(SubjectPath.fullPathName()))
			{
				SubjectName = MakeUniqueName(SubjectName);
				AddPropSubject(SubjectName.asChar(),
					SubjectPath,
					static_cast<MLiveLinkPropSubject::MPropStreamMode>(StreamType),
					Index);

				MGlobal::displayInfo(MString("LiveLinkAddSubjectCommand ") + SubjectName);
				SubjectNotInList = true;
			}
		}
	}

	return !SubjectNotInList;
}

//======================================================================
//
/*!	\brief	Check if an object is in the StreamedSubject list.

	\param[in] DagPath	DagPath of the subject as MString
	
	\return	True when subject was found in StreamedSubject list.

*/
bool MayaLiveLinkStreamManager::IsInSubjectList(const MString& DagPath) const
{
	for (auto& Subject : StreamedSubjects)
	{
		if (DagPath == Subject->GetDagPath().fullPathName())
		{
			return true;
		}
	}
	return false;
}

//======================================================================
//
/*!	\brief	Check if an object is in the StreamedSubject list.

\param[in] DagPath	DagPath of the subject as MString

\return	True when subject was found in StreamedSubject list.

*/
bool MayaLiveLinkStreamManager::IsInSubjectList(const MDagPath& DagPath) const
{
	for (auto& Subject : StreamedSubjects)
	{
		if (DagPath == Subject->GetDagPath())
		{
			return true;
		}
	}
	return false;
}
//======================================================================
//
/*!	\brief	Create a unique name for a subject being added, if a duplicate already exists in
			StreamedSubject list.

	\param[in] SubjectName	Subject name in Maya used as base string to create a unique name

	\return	Unique name that is not in StreamedSubject list.

*/
MString MayaLiveLinkStreamManager::MakeUniqueName(const MString& SubjectName)
{
	if (SubjectName.length() == 0)
	{
		return SubjectName;
	}

	MString UniqueName = SubjectName;

	bool SearchIdenticalMatch = true;
	while (SearchIdenticalMatch)
	{
		SearchIdenticalMatch = false;
		MStringArray SubjectsNames;
		GetSubjectNames(SubjectsNames);
		const auto SubjectsNamesLen = SubjectsNames.length();
		for (unsigned int i = 0; i < SubjectsNamesLen; ++i)
		{
			auto& Subject = SubjectsNames[i];
			if (Subject == UniqueName)
			{
				const char* UniqueNamePtr = UniqueName.asUTF8();

				// Check if the name ends with a number
				int Index = UniqueName.length() - 1;
				while (Index >= 0)
				{
					if (isdigit(UniqueNamePtr[Index]))
					{
						Index--;
					}
					else
					{
						break;
					}
				}

				SearchIdenticalMatch = true;

				if (UniqueName.length() == 1 || Index == -1)
				{
					// The name doesn't end with a number, so add 1 to the name to make it unique
					UniqueName += 1;
				}
				else
				{
					MString NamePrefix = UniqueName.substring(0, Index);
					int number = atoi(UniqueName.substring(Index + 1, UniqueName.length() - 1).asChar());

					// Increment the number by 1
					UniqueName = NamePrefix;
					UniqueName += (number + 1);
				}
				break;
			}
		}
	}

	return UniqueName;
}

//======================================================================
//
/*!	\brief	Variadic function to add a subject to be streamed of a specific type.

	\param[in] Index Index at which the subject should be added
	\param[in] Args  Variable arguments related to subject type

	\return	True when subject was added to StreamedSubject list successfully.

*/
template<class SubjectType, typename... ArgsType>
bool  MayaLiveLinkStreamManager::AddSubjectOfType(int Index, ArgsType&&... Args)
{
	auto Subject = std::make_shared<SubjectType>(Args...);

	bool RebuildSubjectStatus = Subject->RebuildSubjectData();

	Subject->OnStream(FPlatformTime::Seconds(), MAnimControl::currentTime().value());

	if (Index >= 0 && Index < StreamedSubjects.size())
		StreamedSubjects.insert(StreamedSubjects.begin() + Index, Subject);
	else
		StreamedSubjects.emplace_back(Subject);

	return RebuildSubjectStatus;
}

//======================================================================
//
/*!	\brief	Function to add a prop subject.

	\param[in] SubjectName Name of the subject
	\param[in] RootPath    DagPath associated with the subject
	\param[in] StreamType  Stream type of this prop subject
	\param[in] Index       Index at which this subject should be added

*/
void  MayaLiveLinkStreamManager::AddPropSubject(const MString& SubjectName, const MDagPath& RootPath,
	MLiveLinkPropSubject::MPropStreamMode StreamType, int32_t Index)
{
	AddSubjectOfType<MLiveLinkPropSubject>(Index, SubjectName, RootPath, StreamType);
}

//======================================================================
//
/*!	\brief	Function to add a light subject.

	\param[in] SubjectName Name of the subject
	\param[in] RootPath    DagPath associated with the subject
	\param[in] StreamType  Stream type of this prop subject
	\param[in] Index       Index at which this subject should be added

*/
void  MayaLiveLinkStreamManager::AddLightSubject(const MString& SubjectName, const MDagPath& RootPath,
	MLiveLinkLightSubject::MLightStreamMode StreamType, int32_t Index)
{
	AddSubjectOfType<MLiveLinkLightSubject>(Index, SubjectName, RootPath, StreamType);
}

//======================================================================
//
/*!	\brief	Function to add a camera subject.

	\param[in] SubjectName Name of the subject
	\param[in] RootPath    DagPath associated with the subject
	\param[in] StreamType  Stream type of this prop subject
	\param[in] Index       Index at which this subject should be added

*/
void  MayaLiveLinkStreamManager::AddCameraSubject(const MString& SubjectName, const MDagPath& RootPath,
	MLiveLinkCameraSubject::MCameraStreamMode StreamType, int32_t Index)
{
	AddSubjectOfType<MLiveLinkCameraSubject>(Index, SubjectName, RootPath, StreamType);
}

//======================================================================
//
/*!	\brief	Function to add a joint hierarchy subject.

	\param[in] SubjectName Name of the subject
	\param[in] RootPath    DagPath associated with the subject
	\param[in] StreamType  Stream type of this prop subject
	\param[in] Index       Index at which this subject should be added

	\return True if subject was added

*/
bool  MayaLiveLinkStreamManager::AddJointHierarchySubject(const MString& SubjectName, const MDagPath& RootPath,
	MLiveLinkJointHierarchySubject::MCharacterStreamMode StreamType, int32_t Index)
{
	return AddSubjectOfType<MLiveLinkJointHierarchySubject>(Index, SubjectName, RootPath, StreamType);
}

//======================================================================
//
/*!	\brief	Get all the subject names

	\param[in] Entries Reference to string array to store the subject names

*/
void MayaLiveLinkStreamManager::GetSubjectNames(MStringArray& Entries) const
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Entries.append(Subject->GetNameDisplayText());
		}
	}
}

//======================================================================
//
/*!	\brief	Get all the subject's DAG paths

	\param[in] Entries Reference to string array to store the subject DAG paths

*/
void MayaLiveLinkStreamManager::GetSubjectPaths(MStringArray& Entries) const
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Entries.append(Subject->GetDagPath().fullPathName());
		}
	}
}

//======================================================================
//
/*!	\brief	Get all the subject's roles

	\param[in] Entries Reference to string array to store the subject roles

*/
void MayaLiveLinkStreamManager::GetSubjectRoles(MStringArray& Entries) const
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Entries.append(Subject->GetRoleDisplayText());
		}
	}
}

//======================================================================
//
/*!	\brief	Get all the subject's roles

	\param[in] Entries Reference to string array to store the subject roles

*/
void MayaLiveLinkStreamManager::GetSubjectTypes(MStringArray& Entries) const
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Entries.append(Subject->GetSubjectTypeDisplayText());
		}
	}
}

//======================================================================
//
/*!	\brief	Get all the subject's linked assets

\param[in] Entries Reference to string array to store the subject linked assets

*/
void MayaLiveLinkStreamManager::GetSubjectLinkedAssets(MStringArray& Entries) const
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Entries.append(Subject->GetLinkedAsset());
		}
	}
}

//======================================================================
//
/*!	\brief	Get all the subject's target assets

\param[in] Entries Reference to string array to store the subject target assets

*/
void MayaLiveLinkStreamManager::GetSubjectTargetAssets(MStringArray& Entries) const
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Entries.append(Subject->GetTargetAsset());
		}
	}
}

//======================================================================
//
/*!	\brief	Get all the subject's link status

\param[in] Entries Reference to string array to store the subject link status

*/
void MayaLiveLinkStreamManager::GetSubjectLinkStatus(MStringArray& Entries) const
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Entries.append(Subject->IsLinked() ? "1" : "0");
		}
	}
}

//======================================================================
//
/*!	\brief	Get all the subject's target class

\param[in] Entries Reference to string array to store the subject target assets

*/
void MayaLiveLinkStreamManager::GetSubjectClasses(MStringArray& Entries) const
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Entries.append(Subject->GetClass());
		}
	}
}

//======================================================================
//
/*!	\brief	Get all the subject's native unreal class (for blueprints)

\param[in] Entries Reference to string array to store the subject target assets

*/
void MayaLiveLinkStreamManager::GetSubjectUnrealNativeClasses(MStringArray& Entries) const
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Entries.append(Subject->GetUnrealNativeClass());
		}
	}
}

//======================================================================
//
/*!	\brief	Get the stream type using a subject's full DAG path from the root.

	\param[in] Path DAG path for the subject for which we need stream type.

	\return Stream type as an int.

*/
int MayaLiveLinkStreamManager::GetStreamTypeByDagPath(const MString& Path) const
{
	auto Subject = GetSubjectByDagPath(Path);
	if (nullptr != Subject)
	{
		return Subject->GetStreamType();
	}
	return -1;
}

//======================================================================
//
/*!	\brief	Remove a subject given it's full DAG path from the root.
	Note This function removes the subject from StreamedSubject list which will eventually call
	     the destructor for the subject which calls RemoveSubjectFromLiveLink method which removes it from UE.

	\param[in] Path DAG path for the subject which needs to be removed.

	\return Index of the subject in StreamedSubject array or -1 if subject was not part of the list.

*/
int MayaLiveLinkStreamManager::RemoveSubject(const MString& PathOfSubjectToRemove)
{
	for (int32 Index = StreamedSubjects.size() - 1; Index >= 0; --Index)
	{
		if (StreamedSubjects[Index]->ShouldDisplayInUI())
		{
			if (StreamedSubjects[Index]->GetDagPath().fullPathName() == PathOfSubjectToRemove)
			{
				StreamedSubjects.erase(StreamedSubjects.begin() + Index);
				return Index;
			}
		}
	}
	return -1;
}

//======================================================================
//
/*!	\brief	Remove a subject from Live Link provider which will remove it from UE.

	\param[in] SubjectName Name of the subject that needs to be removed from UE.

*/
void MayaLiveLinkStreamManager::RemoveSubjectFromLiveLink(const MString& SubjectName)
{
	FUnrealStreamManager::TheOne().GetLiveLinkProvider()->RemoveSubject(SubjectName.asChar());
}

//======================================================================
//
/*!	\brief	Change subject name of a subject given it's DAG path from the root.

	\param[in] Path    DAG path for the subject which needs renaming.
	\param[in] NewName New name of the subject.

	\return True if subject was renamed.

*/
bool MayaLiveLinkStreamManager::ChangeSubjectName(const MString& SubjectDagPath, const MString& NewName)
{
	// Check if the subject exists with this path name
	if (nullptr != GetSubjectByDagPath(SubjectDagPath))
	{
		// Remove the subject from the subject list in Unreal Stream Manager
		int StreamType   = GetStreamTypeByDagPath(SubjectDagPath);
		int SubjectIndex = RemoveSubject(SubjectDagPath);

		// Get the MDagPath from the full dag path string
		MSelectionList SelectionList;
		SelectionList.add(SubjectDagPath);
		MDagPath DagPath;
		SelectionList.getDagPath(0, DagPath);

		// Create a new Dag Node in Maya
		if (DagPath.hasFn(MFn::kDagNode))
		{
			MItDag DagIterator;
			MStatus Status = DagIterator.reset(DagPath.node());
			
			if (MStatus::kSuccess == Status)
				return AddSubject(DagIterator, NewName, StreamType, SubjectIndex);
		}
	}

	return false;
}

//======================================================================
//
/*!	\brief	Link a subject with an Unreal asset/actor and set the anim/level sequence path

\param[in] SubjectPathIn     DAG path for the subject.
\param[in] LinkInfo          Link asset info
*/
void MayaLiveLinkStreamManager::LinkUnrealAsset(const MString& SubjectPathIn,
												const IMStreamedEntity::LinkAssetInfo& LinkInfo)
{
	if (auto Subject = GetSubjectByDagPath(SubjectPathIn))
	{
		Subject->LinkUnrealAsset(LinkInfo);
	}
}

//======================================================================
//
/*!	\brief	Enable baked animations of a subject

\param[in] SubjectPathIn     DAG path for the subject.
*/
void MayaLiveLinkStreamManager::BakeUnrealAsset(const MString& SubjectPathIn)
{
	if (auto Subject = GetSubjectByDagPath(SubjectPathIn))
	{
		Subject->SetBakeUnrealAsset(true);
	}
}

//======================================================================
//
/*!	\brief	Unlink a subject from a previously linked Unreal asset/actor

\param[in] SubjectPathIn     DAG path for the subject.
*/
void MayaLiveLinkStreamManager::UnlinkUnrealAsset(const MString& SubjectPathIn)
{
	if (auto Subject = GetSubjectByDagPath(SubjectPathIn))
	{
		Subject->UnlinkUnrealAsset();
	}
}

//======================================================================
//
/*!	\brief	Disable baked animations of a subject

\param[in] SubjectPathIn     DAG path for the subject.
*/
void MayaLiveLinkStreamManager::UnbakeUnrealAsset(const MString& SubjectPathIn)
{
	if (auto Subject = GetSubjectByDagPath(SubjectPathIn))
	{
		Subject->SetBakeUnrealAsset(false);
	}
}

//======================================================================
//
/*!	\brief	Update the progress bar UI

\param[in] FrameNumber        Frame number
\param[in] NumberOfFrames     Number of frames
\param[in/out] LastPercentage Percentage since calling this function the last time
*/
void MayaLiveLinkStreamManager::UpdateProgressBar(int FrameNumber, int NumberOfFrames, int& LastPercentage) const
{
	const int Percentage = (FrameNumber + 1) * 100 / NumberOfFrames;
	if (Percentage != LastPercentage)
	{
		MGlobal::executeCommand(MString("MayaUnrealLiveLinkUpdateLinkProgress ") + Percentage);
		LastPercentage = Percentage;
	}
}

//======================================================================
//
/*!	\brief	Get a subject as IMStreamedEntity given it's full DAG path from the root.

	\param[in] Path DAG path for the subject.

	\return Pointer to the subject as IMStreamedEntity

*/
IMStreamedEntity* MayaLiveLinkStreamManager::GetSubjectByDagPath(const MString& Path) const
{
	auto Found = std::find_if(StreamedSubjects.cbegin(), StreamedSubjects.cend(), [&Path](const std::shared_ptr<IMStreamedEntity>& Subject) {
		if (Subject)
		{
			if (Subject->GetDagPath().fullPathName() == Path)
			{
				return Subject;
			}
		}
		return std::shared_ptr<IMStreamedEntity>();
	});

	if (StreamedSubjects.end() != Found)
	{
		return (*Found).get();
	}

	return nullptr;
}

//======================================================================
//
/*!	\brief	Get a subject as IMStreamedEntity given it's full DAG path from the root.

	\param[in] Path DAG path for the subject.

	\return Pointer to the subject as IMStreamedEntity

*/
IMStreamedEntity* MayaLiveLinkStreamManager::GetSubjectByDagPath(const MDagPath& Path) const
{
	auto Found = std::find_if(StreamedSubjects.cbegin(), StreamedSubjects.cend(), [&Path](const std::shared_ptr<IMStreamedEntity>& Subject) {
		if (Subject)
		{
			if (Subject->GetDagPath() == Path)
			{
				return Subject;
			}
		}
		return std::shared_ptr<IMStreamedEntity>();
	});

	if (StreamedSubjects.end() != Found)
	{
		return (*Found).get();
	}

	return nullptr;
}

//======================================================================
//
/*!	\brief	Get the subjects under a common parent path.

	\param[in] Parent DAG path.

	\param[out] List of subjects under the DAG path.

*/
void MayaLiveLinkStreamManager::GetSubjectsFromParentPath(const MDagPath& Path, std::vector<IMStreamedEntity*>& Subjects) const
{
	MStatus Status;
	MFnDagNode ParentDagNode(Path, &Status);
	for (const std::shared_ptr<IMStreamedEntity>& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			MObject Node = Subject->GetDagPath().node();
			if (ParentDagNode.isParentOf(Node))
			{
				Subjects.push_back(Subject.get());
			}
		}
	}
}

//======================================================================
//
/*!	\brief	Get a subject as IMStreamedEntity given a blendshape name owned by this subject.

	\param[in] Blendshape name

	\return Pointer to the subject as IMStreamedEntity

*/
IMStreamedEntity* MayaLiveLinkStreamManager::GetSubjectOwningBlendShape(const MString& Name) const
{
	auto Found = std::find_if(StreamedSubjects.cbegin(), StreamedSubjects.cend(), [&Name](const std::shared_ptr<IMStreamedEntity>& Subject) {
		if (Subject)
		{
			if (Subject->IsOwningBlendShape(Name))
			{
				return Subject;
			}
		}
		return std::shared_ptr<IMStreamedEntity>();
	});

	if (StreamedSubjects.end() != Found)
	{
		return (*Found).get();
	}

	return nullptr;
}

//======================================================================
//
/*!	\brief	Get a subject as IMStreamedEntity given a HumanIK character name owned by this subject.

	\param[in] HikIKEffector object

	\return Pointer to the subject as IMStreamedEntity

*/
IMStreamedEntity* MayaLiveLinkStreamManager::GetSubjectByHikIKEffector(const MObject& Object) const
{
	auto Found = std::find_if(StreamedSubjects.cbegin(), StreamedSubjects.cend(), [&Object](const std::shared_ptr<IMStreamedEntity>& Subject) {
		if (Subject)
		{
			if (Subject->IsUsingHikIKEffector(Object))
			{
				return Subject;
			}
		}
		return std::shared_ptr<IMStreamedEntity>();
	});

	if (StreamedSubjects.end() != Found)
	{
		return (*Found).get();
	}

	return nullptr;
}

//======================================================================
//
/*!	\brief	Change the stream type of a streamed subject.

	\param[in] SubjectPathIn DAG path for the subject.
	\param[in] StreamTypeIn  New stream type for the subject.
*/
void MayaLiveLinkStreamManager::ChangeStreamType(const MString& SubjectPathIn, const MString& StreamTypeIn)
{
	if (IMStreamedEntity* Subject = GetSubjectByDagPath(SubjectPathIn))
	{
		Subject->SetStreamType(StreamTypeIn);
	}
}

//======================================================================
//
/*!	\brief	Clear all subjects in StreamedSubjects list.
*/
void MayaLiveLinkStreamManager::ClearSubjects()
{
	StreamedSubjects.clear();
}

//======================================================================
//
/*!	\brief	Reset the streamed subjects and add active camera.
*/
void MayaLiveLinkStreamManager::Reset()
{
	ClearSubjects();
	AddSubjectOfType<MLiveLinkActiveCamera>(-1);
}

//======================================================================
//
/*!	\brief	Validates all the subjects that are being streamed.
			This function removes any subjects that are not valid.
			Validity is defined in each subject's class definition.

	\param[in] NeedToRefreshUI Need to refresh the UI
*/
void MayaLiveLinkStreamManager::ValidateSubjects(bool NeedToRefreshUI)
{
	StreamedSubjects.erase(std::remove_if( StreamedSubjects.begin(), StreamedSubjects.end(),
		[](const std::shared_ptr<IMStreamedEntity>& Item) { return !Item->ValidateSubject(); }),
		StreamedSubjects.end());

	if (NeedToRefreshUI)
	{
		MayaUnrealLiveLinkUtils::RefreshUI();
	}
}

//======================================================================
//
/*!	\brief	Validate the subjects and rebuild all the streamed subjects.

	\param[in] NeedToRefreshUI Need to refresh the UI
	\param[in] ForceRelink     Need to relink linked assets
*/
void MayaLiveLinkStreamManager::RebuildSubjects(bool NeedToRefreshUI, bool ForceRelink)
{
	ValidateSubjects(NeedToRefreshUI);
	for (const auto& Subject : StreamedSubjects)
	{
		Subject->RebuildSubjectData(ForceRelink);
	}
}

//======================================================================
//
/*!	\brief	Export static data to a file on a disk.
	
	\param[in] SubjectPathIn DAG path for the subject from root.
	\param[in] FilePath		 Path of file on disk where JSON data will be written.

	\return True when subject static data was written correctly to the File.
*/
bool MayaLiveLinkStreamManager::ExportSubjectStaticDataToJSON(const MString& SubjectDagPath,
	const MString& FilePath)
{
	MString Source(FUnrealStreamManager::TheOne().GetLiveLinkProvider()->GetSourceName());
	if (Source != "JSON")
	{
		MGlobal::displayInfo(MString("Cannot export JSON data unless the JSON source is selected\n"));
		return false;
	}

	if (auto Subject = GetSubjectByDagPath(SubjectDagPath))
	{
		FUnrealStreamManager::TheOne().GetLiveLinkProvider()->EnableFileExport(true, FilePath.asUTF8());
		Subject->RebuildSubjectData();
		FUnrealStreamManager::TheOne().GetLiveLinkProvider()->EnableFileExport(false);
	}
	else
	{
		MGlobal::displayInfo(MString("Subject ") + SubjectDagPath + MString(" must be in the Subject list\n"));
		return false;
	}

	return true;
}

//======================================================================
//
/*!	\brief	Export frame/animated data to a file on a disk.

	\param[in] SubjectPathIn DAG path for the subject from root.
	\param[in] FilePath		 Path of file on disk where JSON data will be written.

	\return True when subject frame data was written correctly to the File.
*/
bool MayaLiveLinkStreamManager::ExportSubjectFrameDataToJSON(const MString& SubjectDagPath,
	const MString& FilePath, double FrameTime)
{
	MString Source(FUnrealStreamManager::TheOne().GetLiveLinkProvider()->GetSourceName());
	if (Source != "JSON")
	{
		MGlobal::displayInfo(MString("Cannot export JSON data unless the JSON source is selected\n"));
		return false;
	}

	if (auto Subject = GetSubjectByDagPath(SubjectDagPath))
	{
		FUnrealStreamManager::TheOne().GetLiveLinkProvider()->EnableFileExport(true, FilePath.asUTF8());
		Subject->OnStream(0.0, FrameTime);
		FUnrealStreamManager::TheOne().GetLiveLinkProvider()->EnableFileExport(false);
	}
	else
	{
		MGlobal::displayInfo(MString("Subject ") + SubjectDagPath + MString(" must be in the Subject list\n"));
		return false;
	}

	return true;
}

//======================================================================
//
/*!	\brief	Function responsible for streaming all the subjects that are in StreamedSubject array.
			It loops over all the subjects and call their respective OnStream function.
*/
void MayaLiveLinkStreamManager::StreamSubjects() const
{
	double StreamTime = FPlatformTime::Seconds();
	auto FrameNumber = MAnimControl::currentTime().value();

	for (const auto& Subject : StreamedSubjects)
	{
		Subject->OnStream(StreamTime, FrameNumber);
	}
}

//======================================================================
//
/*!	\brief	Function responsible for streaming a specific the subject from its DAG path.

	\param[in] DagPath   DAG path for the subject from root.
*/
void MayaLiveLinkStreamManager::StreamSubject(const MDagPath& DagPath) const
{
	double StreamTime = FPlatformTime::Seconds();
	auto FrameNumber = MAnimControl::currentTime().value();

	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->GetDagPath() == DagPath)
		{
			Subject->OnStream(StreamTime, FrameNumber);
			break;
		}
	}
}

//======================================================================
//
/*!	\brief	Set anim sequence streaming state

*/
void MayaLiveLinkStreamManager::PauseAnimSequenceStreaming(bool PauseState)
{
	AnimSequenceStreamingPaused = PauseState;
}

//======================================================================
//
/*!	\brief	Return anim sequence streaming state

*/
bool MayaLiveLinkStreamManager::IsAnimSequenceStreamingPaused()
{
	return AnimSequenceStreamingPaused;
}

//======================================================================
//
/*!	\brief	Tell every subject in StreamedSubject list that anim curves are about to be edited.

*/
void MayaLiveLinkStreamManager::OnPreAnimCurvesEdited()
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Subject->OnPreAnimCurvesEdited();
		}
	}
}

//======================================================================
//
/*!	\brief	This function is called by OnAttributeChnaged callback. It forwards the callback
			to every subject in StreamedSubject list.

	\param[in] DagPath   DAG path for the subject from root.
	\param[in] Object    Handle to Maya object.
	\param[in] Plug      First Maya node plug. Meaning depends upon the specific message which invoked the callback.
	\param[in] OtherPlug Second Maya node plug. Meaning depends upon the specific message which invoked the callback.
*/
void MayaLiveLinkStreamManager::OnAttributeChanged(const MDagPath& DagPath, const MObject& Object, const MPlug& Plug, const MPlug& OtherPlug)
{
	double StreamTime = FPlatformTime::Seconds();
	auto FrameNumber = MAnimControl::currentTime().value();

	for (auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI() &&
			Subject->GetDagPath() == DagPath)
		{
			Subject->OnAttributeChanged(Object, Plug, OtherPlug);
			::StreamOnIdle(Subject, MGlobal::kLowIdlePriority);
			break;
		}
	}
}

//======================================================================
//
/*!	\brief	Update the animation curves for every subject in StreamedSubject list.

*/
void MayaLiveLinkStreamManager::OnTimeUnitChanged()
{
	for (const auto& Subject : StreamedSubjects)
	{
		if (Subject->ShouldDisplayInUI())
		{
			Subject->OnTimeUnitChanged();
		}
	}
}
//======================================================================
//
/*!	\brief	This function streams static data for Prop subject to UE.

	\param[in] SubjectName Name of the prop subject to be streamed.
	\param[in] StreamMode  Stream mode for the prop subject.

	\return True if the static prop data was streamed.
*/
bool MayaLiveLinkStreamManager::RebuildPropSubjectData(const MString& SubjectName, const MString& StreamMode)
{
	return FUnrealStreamManager::TheOne().RebuildPropSubjectData(SubjectName.asChar(), StreamMode.asChar());
}

//======================================================================
//
/*!	\brief	This function streams frame data for Prop subject to UE.

	\param[in] SubjectName     Name of the prop subject to be streamed.
	\param[in] StreamMode      Stream mode for the prop subject.
*/
void MayaLiveLinkStreamManager::OnStreamPropSubject(const MString& SubjectName, const MString& StreamMode)
{
	FUnrealStreamManager::TheOne().OnStreamPropSubject(SubjectName.asChar(), StreamMode.asChar());
}

//======================================================================
//
/*!	\brief	This function streams static data for Light subject to UE.

	\param[in] SubjectName Name of the light subject to be streamed.
	\param[in] StreamMode  Stream mode for the light subject.

	\return True if the static light data was streamed.
*/
bool MayaLiveLinkStreamManager::RebuildLightSubjectData(const MString& SubjectName, const MString& StreamMode)
{
	return FUnrealStreamManager::TheOne().RebuildLightSubjectData(SubjectName.asChar(), StreamMode.asChar());
}

//======================================================================
//
/*!	\brief	This function streams frame data for Light subject to UE.

	\param[in] SubjectName     Name of the light subject to be streamed.
	\param[in] StreamMode      Stream mode for the light subject.
*/
void MayaLiveLinkStreamManager::OnStreamLightSubject(const MString& SubjectName, const MString& StreamMode)
{
	FUnrealStreamManager::TheOne().OnStreamLightSubject(SubjectName.asChar(), StreamMode.asChar());
}

//======================================================================
//
/*!	\brief	This function streams static data for Base Camera subject to UE.

	\param[in] SubjectName Name of the prop subject to be streamed.
	\param[in] StreamMode  Stream mode for the prop subject.

	\return True if the static base camera data was streamed.
*/
bool MayaLiveLinkStreamManager::RebuildBaseCameraSubjectData(const MString& SubjectName, const MString& StreamMode)
{
	return FUnrealStreamManager::TheOne().RebuildBaseCameraSubjectData(SubjectName.asChar(), StreamMode.asChar());
}

//======================================================================
//
/*!	\brief	This function streams frame data for Camera subject to UE.

	\param[in] SubjectName     Name of the camera subject to be streamed.
	\param[in] StreamMode      Stream mode for the camera subject.
*/
void MayaLiveLinkStreamManager::StreamCamera(const MString& SubjectName, const MString& StreamMode)
{
	FUnrealStreamManager::TheOne().StreamCamera(SubjectName.asChar(), StreamMode.asChar());
}

//======================================================================
//
/*!	\brief	This function streams static data for Camera subject to UE.

	\param[in] SubjectName Name of the camera subject to be streamed.
	\param[in] StreamMode  Stream mode for the camera subject.

	\return True if the static camera data was streamed.
*/
bool MayaLiveLinkStreamManager::RebuildCameraSubjectData(const MString& SubjectName, const MString& StreamMode)
{
	return FUnrealStreamManager::TheOne().RebuildCameraSubjectData(SubjectName.asChar(), StreamMode.asChar());
}


//======================================================================
//
/*!	\brief	This function streams static data for Joint Hierarchy subject to UE.

	\param[in] SubjectName Name of the joint hierarchy subject to be streamed.
	\param[in] StreamMode  Stream mode for the joint hierarchy subject.

	\return True if the static joint hierarchy data was streamed.
*/
bool MayaLiveLinkStreamManager::RebuildJointHierarchySubject(const MString& SubjectName, const MString& StreamMode)
{
	return FUnrealStreamManager::TheOne().RebuildJointHierarchySubjectData(SubjectName.asChar(), StreamMode.asChar());
}

//======================================================================
//
/*!	\brief	This function streams frame data for Joint Hierarchy subject to UE.

	\param[in] SubjectName   Name of the joint hierarchy to be streamed.
	\param[in] StreamMode    Stream mode for the joint hierarchy subject.
*/
void MayaLiveLinkStreamManager::OnStreamJointHierarchySubject(const MString& SubjectName, const MString& StreamMode)
{
	return FUnrealStreamManager::TheOne().OnStreamJointHierarchySubject(SubjectName.asChar(), StreamMode.asChar());
}

//======================================================================
//
/*!	\brief	This function streams static data for an Anim Sequence subject to UE.

\param[in] SubjectName Name of the subject to be streamed.
*/
void MayaLiveLinkStreamManager::RebuildAnimSequenceSubject(const MString& SubjectName)
{
	FUnrealStreamManager::TheOne().RebuildAnimSequence(SubjectName.asChar());
}

//======================================================================
//
/*!	\brief	This function streams frame data for an Anim Sequence subject to UE.

\param[in] SubjectName   Name of the subject to be streamed.
*/
void MayaLiveLinkStreamManager::OnStreamAnimSequenceSubject(const MString& SubjectName)
{
	FUnrealStreamManager::TheOne().OnStreamAnimSequence(SubjectName.asChar());
}

//======================================================================
//
/*!	\brief	This function streams static data for a Level Sequence subject to UE.

\param[in] SubjectName Name of the subject to be streamed.
*/
void MayaLiveLinkStreamManager::RebuildLevelSequenceSubject(const MString& SubjectName)
{
	FUnrealStreamManager::TheOne().RebuildLevelSequence(SubjectName.asChar());
}

//======================================================================
//
/*!	\brief	This function streams frame data for an Level c Sequence subject to UE.

\param[in] SubjectName   Name of the subject hierarchy to be streamed.
*/
void MayaLiveLinkStreamManager::OnStreamLevelSequenceSubject(const MString& SubjectName)
{
	FUnrealStreamManager::TheOne().OnStreamLevelSequence(SubjectName.asChar());
}

//======================================================================
//
/*!	\brief	This function streams frame data for Joint Hierarchy subject to UE.
*/
MDagPath MayaLiveLinkStreamManager::GetActiveCameraSubjectPath() const
{
	MDagPath DagPath;
	for (auto& Subject : StreamedSubjects)
	{
		if (!Subject->ShouldDisplayInUI() && Subject->GetRole() == MStreamedEntity::Camera)
		{
			return Subject->GetDagPath();
		}
	}
	return DagPath;
}

//======================================================================
//
/*!	\brief	This function streams frame data for Joint Hierarchy subject to UE.

	\param[in] DagPath   DagPath of the subject
*/
void MayaLiveLinkStreamManager::SetActiveCameraDagPath(const MDagPath& DagPath)
{
	for (auto& Subject : StreamedSubjects)
	{
		if (!Subject->ShouldDisplayInUI() && Subject->GetRole() == MStreamedEntity::Camera)
		{
			static_cast<MLiveLinkActiveCamera*>(Subject.get())->CurrentActiveCameraDag = DagPath;
			break;
		}
	}
}

//======================================================================
/*!	\brief	Initialize and get the mutable reference to working static data that will be sent to UE.

	T must be static data for LiveLink subjects.

	\return	T Reference to Subject specific static data in WorkingStaticData.
*/
template<typename T>
T& MayaLiveLinkStreamManager::InitializeAndGetStaticDataFromUnreal()
{
	return FUnrealStreamManager::TheOne().InitializeAndGetStaticData<T>();
}

//======================================================================
/*!	\brief	Initialize and get the mutable reference to working frame data that will be sent to UE.

	T must be frame data for LiveLink subjects.

	\return	T Reference to Subject specific frame data in WorkingStaticData.
*/
template<typename T>
T& MayaLiveLinkStreamManager::InitializeAndGetFrameDataFromUnreal()
{
	return FUnrealStreamManager::TheOne().InitializeAndGetFrameData<T>();
}
