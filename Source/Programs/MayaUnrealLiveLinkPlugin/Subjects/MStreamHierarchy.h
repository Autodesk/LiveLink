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

THIRD_PARTY_INCLUDES_START
#include <maya/MFnIkJoint.h>
THIRD_PARTY_INCLUDES_END

class MStreamHierarchy
{
public:
	MString JointName;
	MFnIkJoint JointObject;
	MFnTransform TransformObject;
	int32_t ParentIndex;
	bool IsTransform;

	MStreamHierarchy() {}

	MStreamHierarchy(const MStreamHierarchy& Other)
		: JointName(Other.JointName)
		, JointObject(Other.JointObject.dagPath())
		, TransformObject(Other.TransformObject.dagPath())
		, ParentIndex(Other.ParentIndex)
		, IsTransform(Other.IsTransform)
	{}

	MStreamHierarchy(const MString& InJointName, const MDagPath& InJointPath, int32_t InParentIndex)
		: JointName(InJointName)
		, JointObject(InJointPath.apiType() == MFn::kTransform ? MDagPath() : InJointPath)
		, TransformObject(InJointPath.apiType() == MFn::kTransform ? InJointPath : MDagPath())
		, ParentIndex(InParentIndex)
		, IsTransform(InJointPath.apiType() == MFn::kTransform)
	{}
};