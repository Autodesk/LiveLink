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

#include "MLiveLinkBaseCameraSubject.h"

class MLiveLinkCameraSubject : public MLiveLinkBaseCameraSubject
{
public:

	MLiveLinkCameraSubject(const MString& InSubjectName, 
						   MDagPath InDagPath,
						   MLiveLinkBaseCameraSubject::MCameraStreamMode InStreamMode = MLiveLinkBaseCameraSubject::MCameraStreamMode::Camera);

	virtual bool ShouldDisplayInUI() const override;
	virtual const MDagPath& GetDagPath() const override;
	virtual bool RebuildSubjectData(bool ForceRelink = false) override;
	virtual void OnStream(double StreamTime, double CurrentTime) override;
	virtual void SetStreamType(MCameraStreamMode StreamModeIn) override;

	virtual void LinkUnrealAsset(const LinkAssetInfo& LinkInfo) override;
	virtual void UnlinkUnrealAsset() override;
	virtual bool IsLinked() const override;

	virtual void OnAnimCurveEdited(const MString& AnimCurveNameIn, MObject& AnimCurveObject, const MPlug& Plug, double ConversionFactor = 1.0) override;

	bool IsCineCamera() const { return bIsCineCamera; }

private:
	MDagPath CameraPath;
	bool bIsCineCamera;

	bool bLinked;
};