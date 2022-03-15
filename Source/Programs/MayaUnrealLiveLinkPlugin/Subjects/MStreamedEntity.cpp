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

#include "MStreamedEntity.h"
#include "../MayaLiveLinkStreamManager.h"
#include "../MayaUnrealLiveLinkUtils.h"

// Boilerplate for importing OpenMaya
#if defined PLATFORM_WINDOWS
#include "Windows/WindowsPlatformCompilerPreSetup.h"
#else
#include "Unix/UnixPlatformCompilerPreSetup.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MFnMesh.h>
THIRD_PARTY_INCLUDES_END

MStreamedEntity::MStreamedEntity(const MDagPath& DagPath)
: HIKEffectorsProcessed(false)
{
	if (DagPath.isValid())
	{
		RootDagPath = DagPath;
		RegisterNodeCallbacks(DagPath);
	}
}

void MStreamedEntity::RegisterNodeCallbacks(const MDagPath& DagPath, bool IsRoot)
{
	if (!DagPath.isValid())
	{
		return;
	}

	MStatus Status;
	MObject Node = DagPath.node(&Status);
	if (!Status)
	{
		MGlobal::displayWarning("Could not get node for DAG path " + DagPath.fullPathName());
		return;
	}

	// AboutToDelete callback
	auto AboutToDeleteCallbackId = MNodeMessage::addNodeAboutToDeleteCallback(Node, AboutToDeleteCallback, nullptr, &Status);
	if (Status)
	{
		CallbackIds.append(AboutToDeleteCallbackId);
	}
	else
	{
		MGlobal::displayWarning("Could not attach about to delete callback for node.");
		CallbackIds.clear();
		return;
	}

	// NameChanged callback
	auto NameChangedCallbackId = MNodeMessage::addNameChangedCallback(Node, NameChangedCallback, nullptr, &Status);
	if (Status)
	{
		CallbackIds.append(NameChangedCallbackId);
	}
	else
	{
		MGlobal::displayWarning("Could not attach name changed callback for node.");
		CallbackIds.clear();
		return;
	}

	// AttributeChanged callback
	auto AttributeChangedCallbackId = MNodeMessage::addAttributeChangedCallback(Node, AttributeChangedCallback, &RootDagPath, &Status);
	if (Status)
	{
		CallbackIds.append(AttributeChangedCallbackId);
	}
	else
	{
		MGlobal::displayWarning("Could not attach attribute changed callback for node.");
		CallbackIds.clear();
		return;
	}

	// Get shape node
	if (DagPath.hasFn(MFn::kShape))
	{
		MDagPath ShapeDagPath = DagPath;
		ShapeDagPath.extendToShape();
		MObject ShapeNode = ShapeDagPath.node(&Status);
		if (!Status)
		{
			MGlobal::displayWarning("Could not get shape node for DAG path " + ShapeDagPath.fullPathName());
			return;
		}
		AttributeChangedCallbackId = MNodeMessage::addAttributeChangedCallback(ShapeNode, AttributeChangedCallback, &RootDagPath, &Status);
		if (Status)
		{
			CallbackIds.append(AttributeChangedCallbackId);
		}
		else
		{
			MGlobal::displayWarning("Could not attach attribute changed callback for node.");
			CallbackIds.clear();
			return;
		}
	}

	// Add children
	MFnDagNode DagNode(Node, &Status);
	if (Status)
	{
		if (IsRoot)
		{
			ProcessBlendShapes(Node);
		}

		if (!HIKEffectorsProcessed && Node.hasFn(MFn::kJoint))
		{
			ProcessHumanIKEffectors(Node);
		}

		const auto ChildCount = DagNode.childCount();
		for (unsigned int Child = 0; Child < ChildCount; ++Child)
		{
			MObject ChildObject = DagNode.child(Child);
			if (ChildObject.hasFn(MFn::kDagNode))
			{
				MDagPath ChildDagPath;
				Status = MDagPath::getAPathTo(ChildObject, ChildDagPath);
				if (Status && ChildDagPath.isValid())
				{
					RegisterNodeCallbacks(ChildDagPath, false);
				}
			}
		}
	}
}

void MStreamedEntity::UnregisterNodeCallbacks()
{
	if (CallbackIds.length() != 0)
	{
		MMessage::removeCallbacks(CallbackIds);
		CallbackIds.clear();
	}
}

void MStreamedEntity::AboutToDeleteCallback(MObject& Node, MDGModifier& Modifier, void* ClientData)
{
	if (Node.hasFn(MFn::kDagNode))
	{
		MDagPath DagPath;
		MStatus Status = MDagPath::getAPathTo(Node, DagPath);
		if (Status && DagPath.isValid())
		{
			// Remove the subject from LiveLink
			MayaLiveLinkStreamManager::TheOne().RemoveSubject(DagPath.fullPathName());
			MayaUnrealLiveLinkUtils::RefreshUI();
		}
	}
}

void MStreamedEntity::NameChangedCallback(MObject& Node, const MString& OldName, void* ClientData)
{
	MayaUnrealLiveLinkUtils::RefreshUI();
}

void MStreamedEntity::AttributeChangedCallback(MNodeMessage::AttributeMessage Msg,
											   MPlug& Plug,
											   MPlug& OtherPlug,
											   void* ClientData)
{
	// Check which attribute was set
	if (Msg & MNodeMessage::kAttributeSet)
	{
		MStatus Status;
		MObject Object = Plug.node(&Status);
		if (Status && ClientData)
		{
			MDagPath& DagPath = *reinterpret_cast<MDagPath*>(ClientData);
			MayaLiveLinkStreamManager::TheOne().OnAttributeChanged(DagPath, Object, Plug, OtherPlug);
		}
	}
}

void MStreamedEntity::ProcessBlendShapes(const MObject& SubjectObject)
{
	MStatus Status;

	// Iterate through blendshapes
	MItDependencyNodes BlendShapeIterator(MFn::kBlendShape);
	while (!BlendShapeIterator.isDone())
	{
		MFnBlendShapeDeformer BlendShape(BlendShapeIterator.thisNode());

		MPlug WeightPlug = BlendShape.findPlug("weight", false);
		if (WeightPlug.isNull() || !WeightPlug.isArray())
		{
			BlendShapeIterator.next();
			continue;
		}

		// Get the base objects of the current blendshape.
		// The base objects are the shapes that are to be deformed.
		MObjectArray BaseObjects;
		BlendShape.getBaseObjects(BaseObjects);
		const MString InMeshString("inMesh");

		// Iterate through the base objects to see if they are associated with the subject.
		for (unsigned int IdxBaseObject = 0; IdxBaseObject < BaseObjects.length(); IdxBaseObject++)
		{
			auto& BaseObject = BaseObjects[IdxBaseObject];
			if (!BaseObject.hasFn(MFn::kMesh))
			{
				continue;
			}

			// Find the "inMesh" plug.
			MFnMesh Mesh(BaseObject);
			auto InMeshPlug = Mesh.findPlug(InMeshString, true, &Status);
			if (!Status)
			{
				continue;
			}

			// Check if a skin cluster is attached to the mesh.
			MPlugArray PlugArray;
			InMeshPlug.connectedTo(PlugArray, true, false);
			for (unsigned int i = 0; i < PlugArray.length(); ++i)
			{
				auto PlugNode = PlugArray[i].node();
				if (!PlugNode.hasFn(MFn::kSkinClusterFilter))
				{
					continue;
				}

				// Find the objects influenced by the skin cluster.
				MFnSkinCluster SkinCluster(PlugNode);
				MDagPathArray InfluenceObjectPaths;
				const unsigned int NumInfluenceObjects = SkinCluster.influenceObjects(InfluenceObjectPaths);
				for (unsigned int Dag = 0; Dag < NumInfluenceObjects; ++Dag)
				{
					// Check if the object is part of the subject hierarchy.
					MFnDagNode ChildNode(InfluenceObjectPaths[Dag]);
					if (!ChildNode.isChildOf(SubjectObject))
					{
						continue;
					}

					// Add a callback on the blendshape node to know when it changes.
					auto BlendShapeObj = BlendShapeIterator.thisNode();
					auto CallbackId = MNodeMessage::addAttributeChangedCallback(BlendShapeObj,
																				AttributeChangedCallback,
																				&RootDagPath,
																				&Status);
					if (Status)
					{
						CallbackIds.append(CallbackId);
					}
					break;
				}
			}
		}

		BlendShapeIterator.next();
	}
}

void MStreamedEntity::ProcessHumanIKEffectors(const MObject& Node)
{
	MFnIkJoint Joint(Node);

	// Find the HIK character plug
	MStatus Status;
	auto CharacterPlug = Joint.findPlug("Character", true, &Status);
	if (!Status)
	{
		return;
	}

	// Get the connected plugs to the HIK character plug
	MPlugArray ConnectedPlugs;
	CharacterPlug.connectedTo(ConnectedPlugs, false, true);
	if (ConnectedPlugs.length() == 0)
	{
		return;
	}

	// The connected plugs are on the HIKCharacter node that will be used to
	// match with the HikIKEffectors
	MFnDependencyNode HIKCharacterNode(ConnectedPlugs[0].node());
	const auto HIKCharacterNodeName = HIKCharacterNode.name();

	HIKEffectorsProcessed = true;

	// Look at all the HikIKEffectors in the scene to find the ones affecting the selected subject
	MItDependencyNodes HikIKEffectorIterator(MFn::kHikIKEffector);
	while (!HikIKEffectorIterator.isDone())
	{
		auto Object = HikIKEffectorIterator.thisNode();
		MFnTransform HikIKEffector(Object);

		// Get the control set plug which will refer to the control rig
		auto ControlSetPlug = HikIKEffector.findPlug("ControlSet", true, &Status);
		if (!Status)
		{
			HikIKEffectorIterator.next();
			continue;
		}

		// Get the source plugs connected to the control set
		MPlugArray ControlSetPlugSrcs;
		ControlSetPlug.connectedTo(ControlSetPlugSrcs, false, true);
		if (ControlSetPlugSrcs.length() == 0)
		{
			HikIKEffectorIterator.next();
			continue;
		}

		// Get the control rig node and find the InputCharacterDefinition plug which will refer a HIKCharacter node
		MFnDependencyNode ControlRigNode(ControlSetPlugSrcs[0].node());
		auto ICDPlug = ControlRigNode.findPlug("InputCharacterDefinition", true, &Status);
		if (!Status)
		{
			HikIKEffectorIterator.next();
			continue;
		}

		MPlugArray ICDPlugs;
		ICDPlug.connectedTo(ICDPlugs, true, false);
		for (unsigned int i = 0; i < ICDPlugs.length(); ++i)
		{
			MFnDependencyNode ICD(ICDPlugs[i].node());

			// Try to match the InputCharacterDefinition from the effector to the one of this subject
			if (ICD.name() == HIKCharacterNodeName)
			{
				// Add a callback so that we can stream the transforms when an effector is moved
				auto AttributeChangedCallbackId = MNodeMessage::addAttributeChangedCallback(Object,
																							AttributeChangedCallback,
																							&RootDagPath,
																							&Status);
				if (Status)
				{
					CallbackIds.append(AttributeChangedCallbackId);
				}
				else
				{
					MGlobal::displayWarning("Could not attach attribute changed callback for node.");
					CallbackIds.clear();
					return;
				}
				break;
			}
		}

		HikIKEffectorIterator.next();
	}
}
