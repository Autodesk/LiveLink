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

#include "MayaUnrealLiveLinkUtils.h"

THIRD_PARTY_INCLUDES_START
#include <maya/MEulerRotation.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnTransform.h>
#include <maya/MMatrix.h>
#include <maya/MQuaternion.h>
THIRD_PARTY_INCLUDES_END

void MayaUnrealLiveLinkUtils::SetMatrixRow(double* Row, MVector Vec)
{
	Row[0] = Vec.x;
	Row[1] = Vec.y;
	Row[2] = Vec.z;
}

double MayaUnrealLiveLinkUtils::RadToDeg(double Rad)
{
	const double E_PI = 3.1415926535897932384626433832795028841971693993751058209749445923078164062;
	return (Rad*180.0) / E_PI;
}

double MayaUnrealLiveLinkUtils::DegToRad(double Deg)
{
	const double E_PI = 3.1415926535897932384626433832795028841971693993751058209749445923078164062;
	return Deg * (E_PI / 180.0f);
}

MMatrix MayaUnrealLiveLinkUtils::GetScale(const MFnTransform& Joint)
{
	double Scale[3];
	Joint.getScale(Scale);
	MTransformationMatrix M;
	M.setScale(Scale, G_TransformSpace);
	return M.asMatrix();
}

MMatrix MayaUnrealLiveLinkUtils::GetRotationOrientation(const MFnIkJoint& Joint, MTransformationMatrix::RotationOrder& RotOrder)
{
	double ScaleOrientation[3];
	Joint.getScaleOrientation(ScaleOrientation, RotOrder);
	MTransformationMatrix M;
	M.setRotation(ScaleOrientation, RotOrder);
	return M.asMatrix();
}

MMatrix MayaUnrealLiveLinkUtils::GetRotation(const MFnTransform& Joint, MTransformationMatrix::RotationOrder& RotOrder)
{
	double Rotation[3];
	Joint.getRotation(Rotation, RotOrder);
	MTransformationMatrix M;
	M.setRotation(Rotation, RotOrder);
	return M.asMatrix();
}

MMatrix MayaUnrealLiveLinkUtils::GetJointOrientation(const MFnIkJoint& Joint, MTransformationMatrix::RotationOrder& RotOrder)
{
	double JointOrientation[3];
	Joint.getOrientation(JointOrientation, RotOrder);
	MTransformationMatrix M;
	M.setRotation(JointOrientation, RotOrder);
	return M.asMatrix();
}

MMatrix MayaUnrealLiveLinkUtils::GetTranslation(const MFnTransform& Joint)
{
	MVector Translation = Joint.getTranslation(G_TransformSpace);
	MTransformationMatrix M;
	M.setTranslation(Translation, G_TransformSpace);
	return M.asMatrix();
}

void MayaUnrealLiveLinkUtils::ComputeTransformHierarchy(MObject& Node, MMatrix& MayaTransform)
{
	MFnTransform TransformNode(Node);
	MayaTransform *= TransformNode.transformation().asMatrix();
	if (TransformNode.parentCount() != 0)
	{
		MObject Parent = TransformNode.parent(0);
		MayaUnrealLiveLinkUtils::ComputeTransformHierarchy(Parent, MayaTransform);
	}
}

void MayaUnrealLiveLinkUtils::RotateCoordinateSystemForUnreal(MMatrix& InOutMatrix)
{
	if (MGlobal::isYAxisUp())
	{
		MQuaternion RotOffset;
		RotOffset.setToXAxis(DegToRad(90.0));
		InOutMatrix *= RotOffset.asMatrix();
	}
}

FTransform MayaUnrealLiveLinkUtils::BuildUETransformFromMayaTransform(MMatrix& InMatrix)
{
	MMatrix UnrealSpaceJointMatrix;

	// from FFbxDataConverter::ConvertMatrix
	for (int i = 0; i < 4; ++i)
	{
		double* Row = InMatrix[i];
		if (i == 1)
		{
			UnrealSpaceJointMatrix[i][0] = -Row[0];
			UnrealSpaceJointMatrix[i][1] = Row[1];
			UnrealSpaceJointMatrix[i][2] = -Row[2];
			UnrealSpaceJointMatrix[i][3] = -Row[3];
		}
		else
		{
			UnrealSpaceJointMatrix[i][0] = Row[0];
			UnrealSpaceJointMatrix[i][1] = -Row[1];
			UnrealSpaceJointMatrix[i][2] = Row[2];
			UnrealSpaceJointMatrix[i][3] = Row[3];
		}
	}

	//OutputRotation(FinalJointMatrix);

	MTransformationMatrix UnrealSpaceJointTransform(UnrealSpaceJointMatrix);

	// getRotation is MSpace::kTransform
	double tx, ty, tz, tw;
	UnrealSpaceJointTransform.getRotationQuaternion(tx, ty, tz, tw);

	FTransform UETrans;
	UETrans.SetRotation(FQuat(tx, ty, tz, tw));

	MVector Translation = UnrealSpaceJointTransform.getTranslation(MSpace::kWorld);
	UETrans.SetTranslation(FVector(Translation.x, Translation.y, Translation.z));

	double Scale[3];
	UnrealSpaceJointTransform.getScale(Scale, MSpace::kWorld);
	UETrans.SetScale3D(FVector((float)Scale[0], (float)Scale[1], (float)Scale[2]));
	return UETrans;
}

FColor MayaUnrealLiveLinkUtils::MayaColorToUnreal(MColor Color)
{
	FColor Result;
	Result.R = FMath::Clamp(Color[0] * 255.0, 0.0, 255.0);
	Result.G = FMath::Clamp(Color[1] * 255.0, 0.0, 255.0);
	Result.B = FMath::Clamp(Color[2] * 255.0, 0.0, 255.0);
	Result.A = 255;
	return Result;
}

FFrameRate MayaUnrealLiveLinkUtils::GetMayaFrameRateAsUnrealFrameRate()
{
	switch (MAnimControl::currentTime().unit())
	{
	case MTime::Unit::k240FPS:
		return FFrameRate(240, 1);
	case MTime::Unit::k120FPS:
		return FFrameRate(120, 1);
	case MTime::Unit::k100FPS:
		return FFrameRate(100, 1);
	case MTime::Unit::k60FPS:
		return FFrameRate(60, 1);
	case MTime::Unit::k50FPS:
		return FFrameRate(50, 1);
	case MTime::Unit::k48FPS:
		return FFrameRate(48, 1);
	case MTime::Unit::k30FPS:
		return FFrameRate(30, 1);
	case MTime::Unit::k25FPS:
		return FFrameRate(25, 1);
	case MTime::Unit::k24FPS:
		return FFrameRate(24, 1);
	case MTime::Unit::k23_976FPS:
		return FFrameRate(24000, 1001);
	case MTime::Unit::k15FPS:
		return FFrameRate(15, 1);
	case MTime::Unit::k12FPS:
		return FFrameRate(12, 1);

	default:
		// Time unit used in supported by Unreal default to 24fps
		return FFrameRate(24, 1);
	}
}

FQualifiedFrameTime MayaUnrealLiveLinkUtils::GetMayaFrameTimeAsUnrealTime()
{
	MTime Time = MAnimControl::currentTime();
	return FQualifiedFrameTime(static_cast<int32>(Time.as(Time.unit())), GetMayaFrameRateAsUnrealFrameRate());
}

void MayaUnrealLiveLinkUtils::OutputRotation(const MMatrix& M)
{
	MTransformationMatrix TM(M);

	MEulerRotation Euler = TM.eulerRotation();

	FVector V;

	V.X = RadToDeg(Euler[0]);
	V.Y = RadToDeg(Euler[1]);
	V.Z = RadToDeg(Euler[2]);
	MGlobal::displayInfo(TCHAR_TO_ANSI(*V.ToString()));
}

FString MayaUnrealLiveLinkUtils::StripMayaNamespace(const MString& InName)
{
	//Strip colons from joint names to keep initial hierarchy names when referencing scenes
	//Colon is an illegal character for names in Maya so only namespaces will be affected
	FString StringName(InName.asChar());
	const int32 CharIndex = StringName.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (StringName.IsValidIndex(CharIndex))
	{
		StringName.RightChopInline(CharIndex+1);
	}

	return StringName;
}

MStatus MayaUnrealLiveLinkUtils::GetSelectedSubjectDagPath(MDagPath& DagPath)
{
	MStatus Status = MS::kFailure;

	MSelectionList SelectedItems;
	MGlobal::getActiveSelectionList(SelectedItems);

	if (SelectedItems.length() > 0)
	{
		MObject SelectedRoot;
		SelectedItems.getDependNode(0, SelectedRoot);

		if (SelectedRoot.hasFn(MFn::kDagNode))
		{
			Status = MDagPath::getAPathTo(SelectedRoot, DagPath);
		}
	}

	return Status;
}

// Execute the python command to refresh our UI
void MayaUnrealLiveLinkUtils::RefreshUI()
{
	MGlobal::executeCommandOnIdle("MayaUnrealLiveLinkRefreshUI");
}

MString MayaUnrealLiveLinkUtils::GetMStringFromFString(const FString& String)
{
	return MString(TCHAR_TO_UTF8(*String));
}

FString MayaUnrealLiveLinkUtils::GetFStringFromMString(const MString& String)
{
	return FString(String.asChar());
}

MString MayaUnrealLiveLinkUtils::GetPlugAliasName(const MPlug& Plug, bool UseLongName)
{
	return Plug.partialName(false, false, false, true, false, UseLongName); // Get alias name
}

bool MayaUnrealLiveLinkUtils::AddUnique(const MDagPath& DagPath, MDagPathArray& DagPathArray)
{
	for (const auto& Path : DagPathArray)
	{
		if (DagPath == Path)
		{
			return false;
		}
	}

	DagPathArray.append(DagPath);
	return true;
}
