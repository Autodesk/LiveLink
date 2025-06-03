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

#include "MLiveLinkJointHierarchySubject.h"
#include <algorithm>

#include "../MayaLiveLinkStreamManager.h"
#include "../MayaUnrealLiveLinkUtils.h"

#include "Roles/LiveLinkAnimationTypes.h"
#include "Roles/MayaLiveLinkTimelineTypes.h"

THIRD_PARTY_INCLUDES_START
#include <maya/MAnimUtil.h>
#include <maya/MDGContextGuard.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MItDependencyNodes.h>
THIRD_PARTY_INCLUDES_END

MString CharacterStreams[2] = { "Transform", "Animation" };
MStringArray MLiveLinkJointHierarchySubject::CharacterStreamOptions(CharacterStreams, 2);

MLiveLinkJointHierarchySubject::MLiveLinkJointHierarchySubject(const MString& InSubjectName, const MDagPath& InRootPath, MCharacterStreamMode InStreamMode)
: IMStreamedEntity(InRootPath)
, SubjectName(InSubjectName)
, StreamMode(InStreamMode >= 0 && InStreamMode < CharacterStreamOptions.length() ? InStreamMode : MCharacterStreamMode::FullHierarchy)
, bLinked(false)
, StreamFullAnimSequence(false)
, ForceLinkAsset(false)
{}

MLiveLinkJointHierarchySubject::~MLiveLinkJointHierarchySubject()
{
	MayaLiveLinkStreamManager::TheOne().RemoveSubjectFromLiveLink(SubjectName);
}

bool MLiveLinkJointHierarchySubject::ShouldDisplayInUI() const
{
	return true;
}

const MDagPath& MLiveLinkJointHierarchySubject::GetDagPath() const
{
	return RootDagPath;
}

MString MLiveLinkJointHierarchySubject::GetNameDisplayText() const
{
	return SubjectName;
}

MString MLiveLinkJointHierarchySubject::GetRoleDisplayText() const
{
	return CharacterStreamOptions[StreamMode];
}

const MString& MLiveLinkJointHierarchySubject::GetSubjectTypeDisplayText() const
{
	static const MString CharacterText("Character");
	return CharacterText;
}

bool MLiveLinkJointHierarchySubject::ValidateSubject() const
{
	MStatus Status;
	bool bIsValid = RootDagPath.isValid(&Status);

	auto StatusMessage = TEXT("Unset");

	if (Status == MS::kSuccess)
	{
		StatusMessage = TEXT("Success");
	}
	else if (Status == MS::kFailure)
	{
		StatusMessage = TEXT("Failure");
	}
	else
	{
		StatusMessage = TEXT("Other");
	}

	if (Status != MS::kFailure && bIsValid)
	{
		//Path checks out as valid
		MFnIkJoint Joint(RootDagPath, &Status);

		MVector returnvec = Joint.getTranslation(MSpace::kWorld, &Status);
		if (Status == MS::kSuccess)
		{
			StatusMessage = TEXT("Success");
		}
		else if (Status == MS::kFailure)
		{
			StatusMessage = TEXT("Failure");
		}
		else
		{
			StatusMessage = TEXT("Other");
		}
	}
	return bIsValid;
}

bool MLiveLinkJointHierarchySubject::RebuildSubjectData(bool ForceRelink)
{
	if (IsLinked() && MayaLiveLinkStreamManager::TheOne().IsAnimSequenceStreamingPaused())
	{
		return false;
	}

	AnimCurves.clear();

	if (StreamMode == MCharacterStreamMode::RootOnly)
	{
		MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkTransformStaticData>();
		return MayaLiveLinkStreamManager::TheOne().RebuildJointHierarchySubject(SubjectName, "RootOnly");
	}
	else if (StreamMode == MCharacterStreamMode::FullHierarchy)
	{
		BlendShapeObjects.clear();
		std::vector<MObject> SkeletonObjects;

		bool Status = false;
		FLiveLinkBaseStaticData* BaseStaticData = nullptr;
		if (IsLinked())
		{
			FMayaLiveLinkAnimSequenceStaticData& TimelineData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FMayaLiveLinkAnimSequenceStaticData>();
			BaseStaticData = &TimelineData;
			Status = BuildStaticData<FMayaLiveLinkAnimSequenceStaticData>(TimelineData, SkeletonObjects);
			if (Status)
			{
				StreamFullAnimSequence = true;

				TimelineData.SequenceName = SavedAssetName.asChar();
				TimelineData.SequencePath = SavedAssetPath.asChar();
				TimelineData.LinkedAssetPath = UnrealAssetPath.asChar();

				auto TimeUnit = MTime::uiUnit();
				TimelineData.FrameRate = MayaUnrealLiveLinkUtils::GetMayaFrameRateAsUnrealFrameRate();
				TimelineData.StartFrame = static_cast<int32>(MAnimControl::minTime().as(TimeUnit));
				TimelineData.EndFrame = static_cast<int32>(MAnimControl::maxTime().as(TimeUnit));
			}
		}
		else
		{
			auto& SkeletonStaticData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkSkeletonStaticData>();
			BaseStaticData = &SkeletonStaticData;

			Status = BuildStaticData<FLiveLinkSkeletonStaticData>(SkeletonStaticData, SkeletonObjects);
		}

		if (Status && BaseStaticData)
		{
			// Streaming blend shapes
			std::vector<MObject> MeshObjects;

			GetGeometrySkinnedToSkeleton(SkeletonObjects, MeshObjects);
			AddBlendShapesWeightNameToStream(MeshObjects);

			if (!IsLinked() || ShouldBakeCurves)
			{
				// Add custom curves to stream blendshapes curves names.
				const auto CurveNamesLen = CurveNames.length();
				for (unsigned int i = 0; i < CurveNamesLen; ++i)
				{
					const auto& Name = CurveNames[i];
					FName CurveName(Name.asChar());
					BaseStaticData->PropertyNames.Add(CurveName);
				}
			}

			// For all the SkeletonObjects add the dynamic attribute names to StaticData
			// that would correspond to values to be stored in FrameData.
			for (auto& Object : SkeletonObjects)
			{
				MFnDependencyNode Node(Object);
				for (unsigned int i = 0; i < Node.attributeCount(); ++i)
				{
					MFnAttribute Attr(Node.attribute(i));
					auto Plug = Node.findPlug(Attr.name(), true);

					if (Plug.isDynamic() && Plug.isKeyable())
					{
						DynamicPlugs.append(Plug);

						if (!IsLinked())
						{
							auto Name = Attr.name();
							BaseStaticData->PropertyNames.Add(Name.asChar());
						}
					}
				}
			}

			if (IsLinked() && !ShouldBakeCurves)
			{
				// Add custom curves to stream blendshapes curves names.
				auto UpdateKeyFrames = [this](const MPlug& Plug, MPlug& PlugParent)
				{
					// Create an empty curve
					MAnimCurve Curve;
					MString Name = MayaUnrealLiveLinkUtils::GetPlugAliasName(Plug);
					auto AnimCurveIter = AnimCurves.emplace(Name.asChar(), std::move(Curve)).first;

					// Read the keyframes
					MObjectArray Curves;
					MAnimUtil::findAnimation(Plug, Curves);
					if (Curves.length() != 0)
					{
						OnAnimKeyframeEdited(Name, Curves[0], PlugParent);
					}
					else
					{
						auto& Frame = AnimCurveIter->second.FindOrAddKeyFrame(0.0, true);
						Frame.Value = Plug.asDouble();
					}
				};

				// Update blend shapes
				for (auto& BlendShapeObject : BlendShapeObjects)
				{
					MFnBlendShapeDeformer BlendShape(BlendShapeObject);
					MPlug Plug = BlendShape.findPlug("weight", false);

					if (!Plug.isNull())
					{
						if (Plug.isArray())
						{
							// Get the value of each weight and insert it in the index corresponding to the curve we want to stream
							for (unsigned int IdxWeight = 0; IdxWeight < Plug.numElements(); IdxWeight++)
							{
								const auto& PlugElement = Plug[IdxWeight];
								UpdateKeyFrames(PlugElement, Plug);
							}
						}
						else
						{
							UpdateKeyFrames(Plug, Plug);
						}
					}
				}

				// Update custom attributes
				for (unsigned int PlugIndex = 0; PlugIndex < DynamicPlugs.length(); ++PlugIndex)
				{
					auto& Plug = DynamicPlugs[PlugIndex];

					if (!Plug.isNull())
					{
						UpdateKeyFrames(Plug, Plug);
					}
				}
			}

			if (IsLinked())
			{
				if(!ShouldBakeCurves)
					BaseStaticData->PropertyNames.Empty();
				MayaLiveLinkStreamManager::TheOne().RebuildAnimSequenceSubject(SubjectName);
			}
			else
			{
				Status = MayaLiveLinkStreamManager::TheOne().RebuildJointHierarchySubject(SubjectName, "FullHierarchy");
			}
		}
		return Status;
	}

	return false;
}

template<typename T>
bool MLiveLinkJointHierarchySubject::BuildStaticData(T& AnimationData,
													 std::vector<MObject>& SkeletonObjects)
{
	JointsToStream.clear();
	CurveNames.clear();
	DynamicPlugs.clear();

	MItDag JointIterator;
	JointIterator.reset(RootDagPath, MItDag::kDepthFirst, MFn::kJoint);

	bool ValidSubject = false;
	if (!JointIterator.isDone())
	{
		MStatus Status;

		ValidSubject = true;
		JointIterator.reset(RootDagPath, MItDag::kDepthFirst, MFn::kTransform);

		// Build Hierarchy
		std::vector<uint32_t> ParentIndexStack(100);

		int32 Index = 0;

		for (; !JointIterator.isDone(); JointIterator.next())
		{
			MDagPath JointPath;
			Status = JointIterator.getPath(JointPath);
			MString JointName;
			bool Valid = false;
			if (JointPath.hasFn(MFn::kJoint))
			{
				MFnIkJoint JointObject(JointPath);
				JointName = TCHAR_TO_ANSI(*MayaUnrealLiveLinkUtils::StripMayaNamespace(JointObject.name()));
				Valid = true;
			}
			else if ((JointIterator.currentItem().apiType() == MFn::kTransform && !JointPath.hasFn(MFn::kShape)) ||
					 JointPath.hasFn(MFn::kMesh) ||
					 JointPath.hasFn(MFn::kIkHandle) ||
					 JointPath.hasFn(MFn::kLocator) ||
					 JointPath.hasFn(MFn::kDistance))
			{
				MFnTransform JointObject(JointPath);
				JointName = JointObject.name();
				Valid = true;
			}

			if (Valid)
			{
				uint32_t Depth = JointIterator.depth();
				if (Depth >= ParentIndexStack.size())
				{
					ParentIndexStack.resize(Depth + 1);
				}
				ParentIndexStack[Depth] = Index++;

				int32_t ParentIndex = Depth == 0 ? -1 : ParentIndexStack[Depth - 1];

				JointsToStream.emplace_back(MStreamHierarchy(JointName, JointPath, ParentIndex));
				AnimationData.BoneNames.Add(*MayaUnrealLiveLinkUtils::StripMayaNamespace(JointName));
				AnimationData.BoneParents.Add(ParentIndex);

				SkeletonObjects.emplace_back(JointPath.node());
			}
		}
	}

	return ValidSubject;
}

// Iterate through skin clusters in the scene and find skin cluster attach to the skeleton we try to stream
// Will return the MObject skinned to the skeleton through second argument "meshes"
void MLiveLinkJointHierarchySubject::GetGeometrySkinnedToSkeleton(const std::vector<MObject>& Skeleton, std::vector<MObject>& Meshes)
{
	// Iterate through skin clusters
	MItDependencyNodes SkinClusterIterator(MFn::kSkinClusterFilter);

	for (; !SkinClusterIterator.isDone(); SkinClusterIterator.next())
	{
		MObject SkinClusterObject = SkinClusterIterator.thisNode();
		MFnSkinCluster SkinCluster(SkinClusterObject);

		// Get the objects that influence the skin cluster. Should contain the bones bind to this skin cluster.
		MDagPathArray InfluenceObjectPaths;
		unsigned int NumInfluenceObjects = SkinCluster.influenceObjects(InfluenceObjectPaths);

		// For each influence objects, check if one of them is an object in the skeleton. If so, that means the skin cluster
		//    is attached to the character we are trying to stream.
		for (unsigned int IdxInfluenceObject = 0; IdxInfluenceObject < NumInfluenceObjects; IdxInfluenceObject++)
		{
			MDagPath& InfluenceObjectPath = InfluenceObjectPaths[IdxInfluenceObject];

			if (std::find(Skeleton.begin(), Skeleton.end(), InfluenceObjectPath.node()) != Skeleton.end())
			{
				// Loop through the geometries affected by this skin cluster
				unsigned int NumGeoms = SkinCluster.numOutputConnections();

				for (unsigned int IdxGeoms = 0; IdxGeoms < NumGeoms; IdxGeoms++)
				{
					unsigned int IndexGeometry = SkinCluster.indexForOutputConnection(IdxGeoms);

					// Get the dag path of the IdxGeoms'th geometry
					MDagPath GeometryPath;
					SkinCluster.getPathAtIndex(IndexGeometry, GeometryPath);


					// Add the geometry object to the meshes to return if its not already in the list
					if (std::find(Meshes.begin(), Meshes.end(), GeometryPath.node()) == Meshes.end())
					{
						Meshes.emplace_back(GeometryPath.node());
					}
				}
				// If the skeleton identified as one of the influence object of the current skin cluster, All the geometry
				//    have been added to the meshes list, we can now jump to the next skin cluster without having to go through
				//    all the influence objects.
				break;
			}
		}
	}
}

// Iterate through the blend shapes in the scene to find the blend shapes associate with the Meshes given in argument.
// If the blend shapes is related to the Meshes given in argument, it will add the name of each weight to the CurveNames.
// Stock the blend shapes related to the character in the variable "BlendShapeObjects".
void MLiveLinkJointHierarchySubject::AddBlendShapesWeightNameToStream(const std::vector<MObject>& Meshes)
{
	// Iterate through blendshapes
	MItDependencyNodes BlendShapeIterator(MFn::kBlendShape);
	const auto CurveNamesLen = CurveNames.length();

	while (!BlendShapeIterator.isDone())
	{
		MFnBlendShapeDeformer BlendShape(BlendShapeIterator.thisNode());

		// Get the base objects of the current blendshape. The base objects are the shapes that are to be deformed
		MObjectArray BaseObjects;
		BlendShape.getBaseObjects(BaseObjects);

		// Iterate through the base object to see if the base objects are contains in the list of meshes associate with the character.
		for (unsigned int IdxBaseObject = 0; IdxBaseObject < BaseObjects.length(); IdxBaseObject++)
		{
			if (std::find(Meshes.begin(), Meshes.end(), BaseObjects[IdxBaseObject]) != Meshes.end())
			{
				// Get the plug of the weight attribute. Get their alias name and add them to the curvesName to be stream. 
				// Does not allow 2 curves with the same name.
				MPlug Plug = BlendShape.findPlug("weight", false);

				if (!Plug.isNull() && Plug.isArray())
				{
					for (unsigned int IdxWeight = 0; IdxWeight < Plug.numElements(); IdxWeight++)
					{
						MString WeightName = MayaUnrealLiveLinkUtils::GetPlugAliasName(Plug[IdxWeight]);

						bool CurveFound = false;
						for (unsigned int i = 0; i < CurveNamesLen; ++i)
						{
							if (WeightName == CurveNames[i])
							{
								CurveFound = true;
								break;
							}
						}
						if (!CurveFound)
						{
							CurveNames.append(WeightName);
						}
					}
				}
				BlendShapeObjects.emplace_back(BlendShapeIterator.thisNode());
			}
		}
		BlendShapeIterator.next();
	}
}

void MLiveLinkJointHierarchySubject::OnStream(double StreamTime, double CurrentTime)
{
	if (IsLinked() && MayaLiveLinkStreamManager::TheOne().IsAnimSequenceStreamingPaused())
	{
		return;
	}

	auto SceneTime = MayaUnrealLiveLinkUtils::GetMayaFrameTimeAsUnrealTime();

	if (StreamMode == MCharacterStreamMode::RootOnly)
	{
		MFnTransform TransformNode(RootDagPath);
		MMatrix Transform = TransformNode.transformation().asMatrix();
		MayaUnrealLiveLinkUtils::RotateCoordinateSystemForUnreal(Transform);
		
		auto& TransformData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkTransformFrameData>();
		TransformData.WorldTime = StreamTime;
		TransformData.MetaData.SceneTime = SceneTime;
		// Convert Maya Camera orientation to Unreal
		TransformData.Transform = MayaUnrealLiveLinkUtils::BuildUETransformFromMayaTransform(Transform);

		MayaLiveLinkStreamManager::TheOne().OnStreamJointHierarchySubject(SubjectName, "RootOnly");
	}
	else if (StreamMode == MCharacterStreamMode::FullHierarchy)
	{
		const auto JointsToStreamLen = JointsToStream.size();
		std::vector<MMatrix> InverseScales;
		InverseScales.reserve(JointsToStreamLen);

		if (IsLinked())
		{
			auto ReserveLambda = [](FMayaLiveLinkAnimSequenceFrameData& AnimationData, int FrameStartIndex, int NumFrames, int NumTransforms)
			{
				if (NumFrames > 0)
				{
					AnimationData.StartFrame = FrameStartIndex;
					AnimationData.Frames.Reserve(NumFrames);
					for (int Index = 0; Index < NumFrames; ++Index, ++FrameStartIndex)
					{
						FMayaLiveLinkAnimSequenceFrame Frame;
						Frame.Locations.Reserve(NumTransforms);
						Frame.Rotations.Reserve(NumTransforms);
						Frame.Scales.Reserve(NumTransforms);
						AnimationData.Frames.Emplace(MoveTemp(Frame));
					}
				}
			};
			auto AddLambda = [](FMayaLiveLinkAnimSequenceFrameData& AnimationData, int FrameIndex, const FTransform& Transform)
			{
				auto& Frame = AnimationData.Frames[FrameIndex];
				Frame.Locations.Add(Transform.GetLocation());
				Frame.Rotations.Add(Transform.GetRotation());
				Frame.Scales.Add(Transform.GetScale3D());
			};
			auto AddBlendShapeWeightsLambda = [](FMayaLiveLinkAnimSequenceFrameData& AnimationData, int FrameIndex, const TArray<float>& CurvesValue)
			{
				auto& Frame = AnimationData.Frames[FrameIndex];
				Frame.PropertyValues.Append(CurvesValue);
			};
			auto AddDynamicPlugLambda = [](FMayaLiveLinkAnimSequenceFrameData& AnimationData, int FrameIndex, float val)
			{
				auto& Frame = AnimationData.Frames[FrameIndex];
				Frame.PropertyValues.Add(val);
			};

			auto InitializeAndStreamFrameData = [this](FMayaLiveLinkAnimSequenceFrameData& AnimationData, double StreamTime)
			{
				InitializeFrameData(AnimationData, MAnimControl::minTime().as(MTime::uiUnit()));
				AnimationData.PropertyValues.Empty();
				AnimationData.WorldTime = StreamTime;
				AnimCurves.clear();
				MayaLiveLinkStreamManager::TheOne().OnStreamAnimSequenceSubject(SubjectName);
			};

			auto StartFrame = MAnimControl::minTime();
			auto EndFrame = MAnimControl::maxTime();
			const int NumberOfFrames = static_cast<int>((EndFrame - StartFrame).as(MTime::uiUnit())) + 1;
			if (StreamFullAnimSequence)
			{
				FMayaLiveLinkAnimSequenceFrameData& AnimationData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FMayaLiveLinkAnimSequenceFrameData>();

				int NumberOfFramesToReserve = NumberOfFrames;
				auto MayaTime = StartFrame;
				int LastPercentage = -1;
				for (int Index = 0; Index < NumberOfFrames; ++Index, MayaTime += 1)
				{
					MDGContext timeContext(MayaTime);
					MDGContextGuard ContextGuard(timeContext);
					ReserveLambda(AnimationData, 0, NumberOfFramesToReserve, JointsToStreamLen);
					BuildFrameData<FMayaLiveLinkAnimSequenceFrameData>(AnimationData, AddLambda, InverseScales, Index);
					BuildBlendShapeWeights<FMayaLiveLinkAnimSequenceFrameData>(AnimationData, AddBlendShapeWeightsLambda, Index);
					BuildDynamicPlugValues<FMayaLiveLinkAnimSequenceFrameData>(AnimationData, AddDynamicPlugLambda, Index);

					// Reserve memory only on the first frame
					NumberOfFramesToReserve = -1;

					MayaLiveLinkStreamManager::TheOne().UpdateProgressBar(Index, NumberOfFrames, LastPercentage);

					InverseScales.clear();
				}

				// TODO: need to optimize using the anim cache playback telling which frames changed instead
				StreamFullAnimSequence = false;

				InitializeAndStreamFrameData(AnimationData, StreamTime);
			}
			else
			{
				FMayaLiveLinkAnimSequenceFrameData& AnimationData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FMayaLiveLinkAnimSequenceFrameData>();

				int Index = static_cast<int>(MAnimControl::currentTime().as(MTime::uiUnit())) - StartFrame.as(MTime::uiUnit());
				if (Index < 0 || Index >= NumberOfFrames)
				{
					return;
				}

				ReserveLambda(AnimationData, Index, 1, JointsToStreamLen);
				BuildFrameData<FMayaLiveLinkAnimSequenceFrameData>(AnimationData, AddLambda, InverseScales, 0);
				BuildBlendShapeWeights<FMayaLiveLinkAnimSequenceFrameData>(AnimationData, AddBlendShapeWeightsLambda, 0);
				BuildDynamicPlugValues<FMayaLiveLinkAnimSequenceFrameData>(AnimationData, AddDynamicPlugLambda, 0);
				InitializeAndStreamFrameData(AnimationData, StreamTime);
			}
		}
		else
		{
			auto& AnimFrameData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkAnimationFrameData>();

			auto ReserveLambda = [](FLiveLinkAnimationFrameData& AnimationData, int Num)
			{
				AnimationData.Transforms.Reserve(Num);
			};
			auto AddLambda = [](FLiveLinkAnimationFrameData& AnimationData, int, const FTransform& Transform)
			{
				AnimationData.Transforms.Add(Transform);
			};
			auto AddBlendShapeWeightsLambda = [](FLiveLinkAnimationFrameData& AnimationData, int, const TArray<float>& CurvesValue)
			{
				AnimationData.PropertyValues.Append(CurvesValue);
			};
			auto AddDynamicPlugLambda = [](FLiveLinkAnimationFrameData& AnimationData, int, float val)
			{
				AnimationData.PropertyValues.Add(val);
			};
			ReserveLambda(AnimFrameData, JointsToStreamLen);
			BuildFrameData<FLiveLinkAnimationFrameData>(AnimFrameData, AddLambda, InverseScales, 0);
			BuildBlendShapeWeights<FLiveLinkAnimationFrameData>(AnimFrameData, AddBlendShapeWeightsLambda, 0);
			BuildDynamicPlugValues<FLiveLinkAnimationFrameData>(AnimFrameData, AddDynamicPlugLambda, 0);

			AnimFrameData.WorldTime = StreamTime;
			AnimFrameData.MetaData.SceneTime = SceneTime;
			MayaLiveLinkStreamManager::TheOne().OnStreamJointHierarchySubject(SubjectName, "FullHierarchy");
		}
	}
}

template<typename T, typename F>
void MLiveLinkJointHierarchySubject::BuildBlendShapeWeights(T& AnimationData, F& AddLambda, int FrameIndex)
{
	TArray<float> CurvesValue;
	CurvesValue.Init(0.0f, CurveNames.length());
	MStatus Status;

	// Iterate through the objects associate with the character that has blend shape on it
	for (int IdxBlendShapeObj = 0; IdxBlendShapeObj < BlendShapeObjects.size(); IdxBlendShapeObj++)
	{
		if (!BlendShapeObjects[IdxBlendShapeObj].isNull())
		{
			MFnBlendShapeDeformer BlendShape(BlendShapeObjects[IdxBlendShapeObj]);
			// Get the parent directory array
			MPlug ParentDirectoryPlug = BlendShape.findPlug("parentDirectory", true);
			// Get the target visibility array
			MPlug TargetVisibilityPlug = BlendShape.findPlug("targetVisibility", true);
			// Get the target parent visibility array
			MPlug TargetParentVisibilityPlug = BlendShape.findPlug("targetParentVisibility", true);

			//// Get the weight array
			MPlug WeightPlug = BlendShape.findPlug("weight", false);
			// NOTE: We need to evaluate the plug here, otherwise numElements() could return 0. 
			MObject getVal;
			WeightPlug.getValue(getVal);
			
			if (!WeightPlug.isNull() && WeightPlug.isArray())
			{
				// For every weight of a blendshape, compute recursively the parent directories weights
				// and multiply them with the actual weight
				for (unsigned int IdxWeight = 0; IdxWeight < WeightPlug.numElements(); IdxWeight++) {
					// Parent visibility
					MPlug CurrentTargetParentVisibilityPlug = TargetParentVisibilityPlug[IdxWeight];
					bool IsCurrentTargetParentVisible = CurrentTargetParentVisibilityPlug.asBool(&Status);
					// Target visibility
					MPlug CurrentTargetVisibilityPlug = TargetVisibilityPlug[IdxWeight];
					bool IsCurrentTargetVisible = CurrentTargetVisibilityPlug.asBool(&Status);

					float ActualWeightValue = 0.0;

					if (IsCurrentTargetParentVisible && IsCurrentTargetVisible) {
						// Target directory plug
						MPlug TargetDirectoryPlug = BlendShape.findPlug("targetDirectory", false);

						// Weight plug
						MPlug CurrentWeightPlug = WeightPlug[IdxWeight];
						ActualWeightValue = CurrentWeightPlug.asFloat(&Status);

						// Get the parent directory weight
						int ParentDirectoryIndex = ParentDirectoryPlug[IdxWeight].asInt();
						float CumulatedParentsWeights = 1.0;

						while (ParentDirectoryIndex >= 0) {
							// When hitting the envelope itself
							if (ParentDirectoryIndex == 0) {
								MPlug EnvelopePlug = BlendShape.findPlug("envelope", true);
								float BlendShapeValue = EnvelopePlug.asFloat(&Status);
								CumulatedParentsWeights *= BlendShapeValue;
								break;
							}

							Status = TargetDirectoryPlug.selectAncestorLogicalIndex(ParentDirectoryIndex);

							// https://help.autodesk.com/view/MAYAUL/2023/ENU/?guid=__Nodes_blendShape_html, under targetDirectory for child indexes
							MPlug TargetDirectoryParentIndexPlug = TargetDirectoryPlug.child(1);
							MPlug TargetDirectoryWeightPlug = TargetDirectoryPlug.child(5);
							MPlug DirectoryVisibilityPlug = TargetDirectoryPlug.child(3);
							MPlug DirectoryParentVisibilityPlug = TargetDirectoryPlug.child(4);

							bool DirectoryVisibility = DirectoryVisibilityPlug.asBool();
							bool DirectoryParentVisibility = DirectoryParentVisibilityPlug.asBool();

							float TargetDirectoryWeight = TargetDirectoryWeightPlug.asFloat(&Status);
							CumulatedParentsWeights *= TargetDirectoryWeight;
							ParentDirectoryIndex = TargetDirectoryParentIndexPlug.asInt(&Status);
						}
								ActualWeightValue *= CumulatedParentsWeights;
					}

					//// Insert the real weight value in the index corresponding to the curve we want to stream
					MString WeightName = MayaUnrealLiveLinkUtils::GetPlugAliasName(WeightPlug[IdxWeight]);
					unsigned int IndexFind = 0;
					for (unsigned int i = 0; i < CurveNames.length(); i++)
					{
						if (CurveNames[i] == WeightName)
						{
							IndexFind = i;
							break;
						}
					}

					if (IndexFind >= 0 && IndexFind < CurveNames.length())
					{
						CurvesValue[IndexFind] = ActualWeightValue;
					} 
				}
			}
		}
		else
		{
			IdxBlendShapeObj--;
			BlendShapeObjects.erase(BlendShapeObjects.cbegin() + IdxBlendShapeObj);
		}
	}

	// Add custom curves value to stream blend shapes.
	AddLambda(AnimationData, FrameIndex, CurvesValue);
}

template<typename T, typename F>
void MLiveLinkJointHierarchySubject::BuildDynamicPlugValues(T& AnimationData, F& AddLambda, int FrameIndex)
{
	// Add the dynamic plug values as property values in AnimeFrameData
	for (unsigned int i = 0; i < DynamicPlugs.length(); ++i)
	{
		auto& Plug = DynamicPlugs[i];
		AddLambda(AnimationData, FrameIndex, Plug.asFloat());
	}
}

template<typename T, typename F>
void MLiveLinkJointHierarchySubject::BuildFrameData(T& AnimationData,
													F& AddLambda,
													std::vector<MMatrix>& InverseScales,
													int FrameIndex)
{
	const auto JointsToStreamLen = JointsToStream.size();
	for (uint32_t Idx = 0; Idx < JointsToStreamLen; ++Idx)
	{
		const MStreamHierarchy& H = JointsToStream[Idx];

		const MFnTransform& TransformObject = H.IsTransform ? H.TransformObject : H.JointObject;

		MTransformationMatrix::RotationOrder RotOrder = TransformObject.rotationOrder();

		MMatrix JointScale = MayaUnrealLiveLinkUtils::GetScale(TransformObject);
		InverseScales.emplace_back(JointScale.inverse());

		MMatrix ParentInverseScale = (H.ParentIndex == -1) ? MMatrix::identity : InverseScales[H.ParentIndex];

		MMatrix MayaSpaceJointMatrix;
		if (!H.IsTransform)
		{
			MayaSpaceJointMatrix = JointScale *
				MayaUnrealLiveLinkUtils::GetRotationOrientation(H.JointObject, RotOrder) *
				MayaUnrealLiveLinkUtils::GetRotation(TransformObject, RotOrder) *
				MayaUnrealLiveLinkUtils::GetJointOrientation(H.JointObject, RotOrder) *
				ParentInverseScale *
				MayaUnrealLiveLinkUtils::GetTranslation(TransformObject);
		}
		else
		{
			MayaSpaceJointMatrix = JointScale *
				MayaUnrealLiveLinkUtils::GetRotation(TransformObject, RotOrder) *
				ParentInverseScale *
				MayaUnrealLiveLinkUtils::GetTranslation(TransformObject);
		}

		if (Idx == 0 && MGlobal::isYAxisUp()) // rotate the root joint to get the correct character rotation in Unreal
		{
			MayaUnrealLiveLinkUtils::RotateCoordinateSystemForUnreal(MayaSpaceJointMatrix);
		}

		AddLambda(AnimationData, FrameIndex, MayaUnrealLiveLinkUtils::BuildUETransformFromMayaTransform(MayaSpaceJointMatrix));
	}
}

void MLiveLinkJointHierarchySubject::SetStreamType(const MString& StreamTypeIn)
{
	for (unsigned int StreamTypeIdx = 0; StreamTypeIdx < CharacterStreamOptions.length(); ++StreamTypeIdx)
	{
		if (CharacterStreamOptions[StreamTypeIdx] == StreamTypeIn && StreamMode != (MCharacterStreamMode)StreamTypeIdx)
		{
			SetStreamType(static_cast<MCharacterStreamMode>(StreamTypeIdx));
			return;
		}
	}
}

void MLiveLinkJointHierarchySubject::SetStreamType(MCharacterStreamMode StreamModeIn)
{
	StreamMode = StreamModeIn;
	if (StreamModeIn != MCharacterStreamMode::FullHierarchy)
	{
		UnrealAssetPath.clear();
		SavedAssetPath.clear();
		SavedAssetName.clear();
	}
	RebuildSubjectData();
}

int MLiveLinkJointHierarchySubject::GetStreamType() const
{
	return StreamMode;
}

void MLiveLinkJointHierarchySubject::OnAttributeChanged(const MObject& Object, const MPlug& Plug, const MPlug& OtherPlug)
{
	if (Object.isNull())
	{
		return;
	}

	if (!IsLinked() || MayaLiveLinkStreamManager::TheOne().IsAnimSequenceStreamingPaused())
	{
		return;
	}

	bool bSendEvent = false;
	MPlug ResolvedPlug;
	MObject ResolvedObject;
	if (Object.hasFn(MFn::kTransform))
	{
		MPlugArray SrcPlugArray;
		Plug.connectedTo(SrcPlugArray, false, true);
		for (unsigned int j = 0; j < SrcPlugArray.length() && !bSendEvent; ++j)
		{
			MPlug& SrcPlug = SrcPlugArray[j];
			MPlugArray SrcPlugArray2;
			SrcPlug.connectedTo(SrcPlugArray2, false, true);
			for (unsigned int k = 0; k < SrcPlugArray2.length(); ++k)
			{
				MPlug& SrcPlug2 = SrcPlugArray2[k];
				if (SrcPlug2.node().hasFn(MFn::kBlendShape))
				{
					ResolvedObject = SrcPlug2.node();
					ResolvedPlug = SrcPlug2;
					bSendEvent = true;
					break;
				}
			}
		}
	}
	else if (Object.hasFn(MFn::kBlendShape) || Object.hasFn(MFn::kHikIKEffector))
	{
		ResolvedPlug = Plug;
		ResolvedObject = Object;
		bSendEvent = true;
	}

	if (bSendEvent)
	{
		StreamFullAnimSequence = false;

		MObjectArray AnimationCurves;

		// Find the animation curve(s) that animate this plug
		bool HasAnimatedCurves = MAnimUtil::findAnimation(ResolvedPlug, AnimationCurves);

		MObjectArray ObjectArray;
		unsigned int numAnimCurves = AnimationCurves.length();
		for (unsigned int Curve = 0; Curve < numAnimCurves; ++Curve)
		{
			ObjectArray.append(AnimationCurves[Curve]);
		}

		// Notify that we want to send these anim curves
		MObject InvalidObject;
		OnAnimCurveEdited(MayaUnrealLiveLinkUtils::GetPlugAliasName(ResolvedPlug),
						  HasAnimatedCurves && ObjectArray.length() != 0 ? ObjectArray[0] : InvalidObject,
						  ResolvedPlug);
	}

	IMStreamedEntity::OnAttributeChanged(ResolvedObject, ResolvedPlug, OtherPlug);
}

void MLiveLinkJointHierarchySubject::OnAnimCurveEdited(const MString& AnimCurveNameIn, MObject& AnimCurveObject, const MPlug& Plug, double ConversionFactor)
{
	if (!IsLinked() || MayaLiveLinkStreamManager::TheOne().IsAnimSequenceStreamingPaused() || ShouldBakeCurves)
	{
		return;
	}

	MString AnimCurveName(AnimCurveNameIn);
	if (!Plug.node().hasFn(MFn::kBlendShape))
	{
		// Check if the plug is a custom attribute
		bool Found = false;
		for( const auto& DynamicPlug : DynamicPlugs)
		{
			if (DynamicPlug == Plug)
			{
				AnimCurveName = MayaUnrealLiveLinkUtils::GetPlugAliasName(Plug);
				Found = true;
				break;
			}
		}

		// We want to stream everything else other than blend shapes and custom attributes.
		if (!Found)
		{
			return;
		}
	}

	// Create or clear the anim curve
	auto AnimCurveIter = AnimCurves.find(AnimCurveName.asChar());
	if (AnimCurveIter != AnimCurves.end())
	{
		AnimCurveIter->second.KeyFrames.clear();
	}
	else
	{
		MAnimCurve Curve;
		AnimCurveIter = AnimCurves.emplace(AnimCurveName.asChar(), std::move(Curve)).first;
	}

	MAnimCurve& AnimCurve = AnimCurveIter->second;

	UpdateAnimCurveKeys(AnimCurveObject, AnimCurve);

	// An invalid anim curve usually refers to a custom attribute/blend shape with no associated anim curve
	// Still stream the value to maintain the original behavior when not linked to an Unreal asset.

	if (AnimCurveObject.isNull())
	{
		MStreamedEntity::OnAnimCurveEdited(AnimCurveNameIn, AnimCurveObject, Plug);

		// Also update connected plugs
		MObject InvalidObject;
		auto UpdateConnectedPlugs = [this, &InvalidObject](MPlugArray& PlugArray)
		{
			for (unsigned int i = 0; i < PlugArray.length(); ++i)
			{
				MPlug& ConnectedPlug = PlugArray[i];
				MObject Object = ConnectedPlug.node();
				if (Object.hasFn(MFn::kBlendShape)) 
				{
					MStreamedEntity::OnAnimCurveEdited(MayaUnrealLiveLinkUtils::GetPlugAliasName(ConnectedPlug),
													InvalidObject,
													ConnectedPlug);
				}
			}
		};

		MPlugArray PlugArray;
		Plug.connectedTo(PlugArray, false, true);
		UpdateConnectedPlugs(PlugArray);

		PlugArray.clear();
		Plug.connectedTo(PlugArray, true, false);
		UpdateConnectedPlugs(PlugArray);
	}
}

void MLiveLinkJointHierarchySubject::OnAnimKeyframeEdited(const MString& MayaAnimCurveName, MObject& AnimCurveObject, const MPlug& Plug)
{
	if (!IsLinked() || MayaLiveLinkStreamManager::TheOne().IsAnimSequenceStreamingPaused())
	{
		return;
	}

	if (Plug.isNull())
	{
		return;
	}
	
	if(ShouldBakeCurves)
	{
		StreamFullAnimSequence = true;
		return;
	}

	MString AnimCurveName(MayaAnimCurveName);
	if (!Plug.node().hasFn(MFn::kBlendShape))
	{
		// Check if the plug is a custom attribute
		bool Found = false;
		for( const auto& DynamicPlug : DynamicPlugs)
		{
			if (DynamicPlug == Plug)
			{
				AnimCurveName = MayaUnrealLiveLinkUtils::GetPlugAliasName(Plug);
				Found = true;
				break;
			}
		}

		// We want to stream everything else other than blend shapes and custom attributes.
		if (!Found)
		{
			StreamFullAnimSequence = true;
			return;
		}
	}

	// Create or clear the anim curve
	auto AnimCurveIter = AnimCurves.find(AnimCurveName.asChar());
	if (AnimCurveIter != AnimCurves.end())
	{
		AnimCurveIter->second.KeyFrames.clear();
	}
	else
	{
		MAnimCurve Curve;
		AnimCurveIter = AnimCurves.emplace(AnimCurveName.asChar(), std::move(Curve)).first;
	}

	MAnimCurve& AnimCurve = AnimCurveIter->second;
	UpdateAnimCurveKeys(AnimCurveObject, AnimCurve);
}

void MLiveLinkJointHierarchySubject::LinkUnrealAsset(const LinkAssetInfo& LinkInfo)
{
	if (!bLinked ||
		(bLinked &&
		 (LinkInfo.UnrealAssetPath != UnrealAssetPath ||
		  LinkInfo.SavedAssetPath != SavedAssetPath ||
		  LinkInfo.SavedAssetName != SavedAssetName)) ||
		ForceLinkAsset)
	{
		UnrealAssetPath = LinkInfo.UnrealAssetPath;
		SavedAssetPath = LinkInfo.SavedAssetPath;
		SavedAssetName = LinkInfo.SavedAssetName;

		if (!LinkInfo.bSetupOnly && !MayaLiveLinkStreamManager::TheOne().IsAnimSequenceStreamingPaused())
		{
			bLinked = true;

			RebuildSubjectData();

			// Wait a bit after rebuilding the subject data before sending the frame data to Unreal.
			// Otherwise, Unreal will ignore it.
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(100ms);

			if (!ForceLinkAsset)
			{
				OnStreamCurrentTime();
			}
		}
	}
}

void MLiveLinkJointHierarchySubject::UnlinkUnrealAsset()
{
	bLinked = false;
	FUnrealStreamManager::TheOne().UpdateWhenDisconnected(true);
	SetStreamType(StreamMode);
	OnStreamCurrentTime();
	FUnrealStreamManager::TheOne().UpdateWhenDisconnected(false);
}

void MLiveLinkJointHierarchySubject::SetBakeUnrealAsset(bool shouldBakeCurves)
{
	ShouldBakeCurves = shouldBakeCurves;
	if (!MayaLiveLinkStreamManager::TheOne().IsAnimSequenceStreamingPaused())
	{
		RebuildSubjectData();

		// Wait a bit after rebuilding the subject data before sending the frame data to Unreal.
		// Otherwise, Unreal will ignore it.
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(100ms);

		if (!ForceLinkAsset)
		{
			OnStreamCurrentTime();
		}
	}
}

void MLiveLinkJointHierarchySubject::OnTimeUnitChanged()
{
	if (!IsLinked() || MayaLiveLinkStreamManager::TheOne().IsAnimSequenceStreamingPaused())
	{
		return;
	}

	ForceLinkAsset = true;
	IMStreamedEntity::LinkAssetInfo LinkInfo;
	LinkInfo.UnrealAssetPath = UnrealAssetPath;
	LinkInfo.UnrealAssetClass = MString("");
	LinkInfo.SavedAssetPath = SavedAssetPath;
	LinkInfo.SavedAssetName = SavedAssetName;
	LinkInfo.UnrealNativeClass = MString("Skeleton");
	LinkUnrealAsset(LinkInfo);
	ForceLinkAsset = false;
}

bool MLiveLinkJointHierarchySubject::IsLinked() const
{
	return bLinked &&
		   (UnrealAssetPath.length() ||
		    SavedAssetPath.length() ||
		    SavedAssetName.length());
}

bool MLiveLinkJointHierarchySubject::ShouldBakeTransform() const
{
	return ShouldBakeCurves;
}
