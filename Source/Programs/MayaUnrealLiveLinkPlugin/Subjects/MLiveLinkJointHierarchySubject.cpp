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

MString CharacterStreams[2] = { "Root Only", "Full Hierarchy"};
MStringArray MLiveLinkJointHierarchySubject::CharacterStreamOptions(CharacterStreams, 2);

MLiveLinkJointHierarchySubject::MLiveLinkJointHierarchySubject(const MString& InSubjectName, const MDagPath& InRootPath, MCharacterStreamMode InStreamMode)
	: MStreamedEntity(InRootPath)
	, SubjectName(InSubjectName)
	, RootDagPath(InRootPath)
	, StreamMode(InStreamMode >= 0 && InStreamMode < CharacterStreamOptions.length() ? InStreamMode : MCharacterStreamMode::FullHierarchy)
{}

MLiveLinkJointHierarchySubject::~MLiveLinkJointHierarchySubject()
{
	MayaLiveLinkStreamManager::TheOne().RemoveSubjectFromLiveLink(SubjectName);
}

bool MLiveLinkJointHierarchySubject::ShouldDisplayInUI() const
{
	return true;
}

MDagPath MLiveLinkJointHierarchySubject::GetDagPath() const
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

MString MLiveLinkJointHierarchySubject::GetSubjectTypeDisplayText() const
{
	return MString("Character");
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

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Testing %s for removal Path:%s Valid:%s Status:%s\n"), SubjectName.asChar(), RootDagPath.fullPathName().asWChar(), bIsValid ? TEXT("true") : TEXT("false"), StatusMessage);
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

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\tTesting %s for removal Path:%s Valid:%s Status:%s\n"), SubjectName.asChar(), RootDagPath.fullPathName().asWChar(), bIsValid ? TEXT("true") : TEXT("false"), StatusMessage);
	}
	return bIsValid;
}

bool MLiveLinkJointHierarchySubject::RebuildSubjectData()
{
	bool ValidSubject = false;

	if (StreamMode == MCharacterStreamMode::RootOnly)
	{
		MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkTransformStaticData>();
		return MayaLiveLinkStreamManager::TheOne().RebuildJointHierarchySubject(SubjectName, "RootOnly");
	}
	else if (StreamMode == MCharacterStreamMode::FullHierarchy)
	{
		auto& SkeletonStaticData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetStaticDataFromUnreal<FLiveLinkSkeletonStaticData>();

		JointsToStream.clear();
		CurveNames.clear();
		DynamicPlugs.clear();
		BlendShapeObjects.clear();
		std::vector<MObject> SkeletonObjects;

		MStatus status;
		MItDag JointIterator;
		JointIterator.reset(RootDagPath, MItDag::kDepthFirst, MFn::kJoint);

		if (!JointIterator.isDone())
		{
			ValidSubject = true;
			JointIterator.reset(RootDagPath, MItDag::kDepthFirst, MFn::kTransform);

			// Build Hierarchy
			std::vector<uint32_t> ParentIndexStack(100);

			int32 Index = 0;

			for (; !JointIterator.isDone(); JointIterator.next())
			{
				uint32_t Depth = JointIterator.depth();
				if (Depth >= (uint32_t)ParentIndexStack.size())
				{
					ParentIndexStack.resize(Depth + 1);
				}
				ParentIndexStack[Depth] = Index++;

				int32_t ParentIndex = Depth == 0 ? -1 : ParentIndexStack[Depth - 1];

				MDagPath JointPath;
				status = JointIterator.getPath(JointPath);
				MString JointName;
				if (JointPath.hasFn(MFn::kJoint))
				{
					MFnIkJoint JointObject(JointPath);
					JointName = JointObject.name();
				}
				else
				{
					MFnTransform JointObject(JointPath);
					JointName = JointObject.name();
				}
				JointsToStream.emplace_back(MStreamHierarchy(JointName, JointPath, ParentIndex));
				SkeletonStaticData.BoneNames.Add(*MayaUnrealLiveLinkUtils::StripMayaNamespace(JointName));
				SkeletonStaticData.BoneParents.Add(ParentIndex);

				SkeletonObjects.emplace_back(JointPath.node());
			}

			// Streaming blend shapes
			std::vector<MObject> MeshObjects;
			MeshObjects.clear();

			GetGeometrySkinnedToSkeleton(SkeletonObjects, MeshObjects);
			AddBlendShapesWeightNameToStream(MeshObjects);

			// Add custom curves to stream blendshapes curves names.
			const auto CurveNamesLen = CurveNames.length();
			for (unsigned int i = 0; i < CurveNamesLen; ++i)
			{
				const auto& Name = CurveNames[i];
				FName CurveName(Name.asChar());
				SkeletonStaticData.PropertyNames.Add(CurveName);
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
						SkeletonStaticData.PropertyNames.Add(Attr.name().asChar());
					}
				}
			}

			return MayaLiveLinkStreamManager::TheOne().RebuildJointHierarchySubject(SubjectName, "FullHierarchy");
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
						MString WeightName = GetPlugAliasName(Plug[IdxWeight]);

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

MString MLiveLinkJointHierarchySubject::GetPlugAliasName(const MPlug& Plug)
{
	return Plug.partialName(false, false, false, true, false, false); // Get alias name
}

void MLiveLinkJointHierarchySubject::OnStream(double StreamTime, double CurrentTime)
{
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
		auto& AnimFrameData = MayaLiveLinkStreamManager::TheOne().InitializeAndGetFrameDataFromUnreal<FLiveLinkAnimationFrameData>();
		AnimFrameData.WorldTime = StreamTime;
		AnimFrameData.MetaData.SceneTime = SceneTime;
		AnimFrameData.Transforms.Reserve(JointsToStream.size());

		std::vector<MMatrix> InverseScales;
		InverseScales.reserve(JointsToStream.size());

		for (int Idx = 0; Idx < JointsToStream.size(); ++Idx)
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

			AnimFrameData.Transforms.Add(MayaUnrealLiveLinkUtils::BuildUETransformFromMayaTransform(MayaSpaceJointMatrix));
		}

		//streaming blend shapes
		TArray<float> CurvesValue;
		CurvesValue.Init(0.0f, CurveNames.length());

		// Iterate through the objects associate with the character that has blend shape on it
		for (int IdxBlendShapeObj = 0; IdxBlendShapeObj < BlendShapeObjects.size(); IdxBlendShapeObj++)
		{
			if (!BlendShapeObjects[IdxBlendShapeObj].isNull())
			{
				MFnBlendShapeDeformer BlendShape(BlendShapeObjects[IdxBlendShapeObj]);

				// Get the weight of the blend shape.
				MPlug Plug = BlendShape.findPlug("weight", false);

				if (!Plug.isNull() && Plug.isArray())
				{
					// Get the value of each weight and insert it in the index corresponding to the curve we want to stream
					for (unsigned int IdxWeight = 0; IdxWeight < Plug.numElements(); IdxWeight++)
					{
						MString WeightName = GetPlugAliasName(Plug[IdxWeight]);
						unsigned int IndexFind = 0;
						for (unsigned int i = 0; i < CurveNames.length(); i++)
						{
							if (CurveNames[i] == WeightName)
							{
								IndexFind = i;
							}
						}

						if (IndexFind >= 0 && IndexFind < CurveNames.length())
						{
							float WeightValue;
							Plug[IdxWeight].getValue(WeightValue);
							CurvesValue[IndexFind] = WeightValue;
						}
					}
				}
			}
			else
			{
				IdxBlendShapeObj--;
				BlendShapeObjects.erase(BlendShapeObjects.cbegin()+IdxBlendShapeObj);
			}
		}

		// Add custom curves value to stream blend shapes.
		AnimFrameData.PropertyValues.Append(CurvesValue);

		// Add the dynamic plug values as property values in AnimaFrameData
		for (unsigned int i = 0; i < DynamicPlugs.length(); ++i)
		{
			auto& Plug = DynamicPlugs[i];
			AnimFrameData.PropertyValues.Add(Plug.asFloat());
		}

		MayaLiveLinkStreamManager::TheOne().OnStreamJointHierarchySubject(SubjectName, "FullHierarchy");
	}
}

void MLiveLinkJointHierarchySubject::SetStreamType(const MString& StreamTypeIn)
{
	for (unsigned int StreamTypeIdx = 0; StreamTypeIdx < CharacterStreamOptions.length(); ++StreamTypeIdx)
	{
		if (CharacterStreamOptions[StreamTypeIdx] == StreamTypeIn && StreamMode != (MCharacterStreamMode)StreamTypeIdx)
		{
			StreamMode = (MCharacterStreamMode)StreamTypeIdx;
			RebuildSubjectData();
			return;
		}
	}
}

int MLiveLinkJointHierarchySubject::GetStreamType() const
{
	return StreamMode;
}
