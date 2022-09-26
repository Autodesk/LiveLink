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

#include "MFocalLengthRedirectCurve.h"

#include <cmath>

THIRD_PARTY_INCLUDES_START
#include <maya/MDGContextGuard.h>
THIRD_PARTY_INCLUDES_END

MFocalLengthRedirectCurve::MFocalLengthRedirectCurve(const std::string& NameIn)
: MRedirectCurve(NameIn)
{
}

MFocalLengthRedirectCurve::~MFocalLengthRedirectCurve()
{
}

double MFocalLengthRedirectCurve::GetValue(const MFnCamera& Camera) const
{
	return GetValueInternal(Camera);
}

void MFocalLengthRedirectCurve::BakeKeyFrameRange(MStreamedEntity::MAnimCurve& AnimCurve,
												  const MFnCamera& Camera,
												  const MString& PlugName,
												  std::map<std::string, MStreamedEntity::MAnimCurve>& AnimCurves) const
{
	auto& KeyFrames = AnimCurve.KeyFrames;

	if (KeyFrames.size() != 0)
	{
		auto AspectRatioCurveIter = AnimCurves.find("AspectRatio");
		if (AspectRatioCurveIter != AnimCurves.end())
		{
			// Don't bake keyframes if there are already some and the AspectRatio curve is there.
			// The FieldOfView curve in Unreal changes when the Sensor.Width changes which is mapped to the AspectRatio curve in Unreal.
			return;
		}
	}

	// Find the last frame to determine when to stop baking frames
	auto LastIter = KeyFrames.rbegin();
	double MaxTime = 0;
	if (LastIter != KeyFrames.rend())
	{
		MaxTime = LastIter->first;
	}

	// Also, check for the other curve since we need to determine which curve has the latest frame
	MStatus Status;
	MPlug OtherAnglePlug = Camera.findPlug("hfa", true, &Status);
	if (Status && !OtherAnglePlug.isNull())
	{
		MPlugArray PlugArray;
		OtherAnglePlug.connectedTo(PlugArray, true, false);
		for (unsigned int i = 0; i < PlugArray.length(); ++i)
		{
			auto Object = PlugArray[i].node();
			if (Object.hasFn(MFn::kAnimCurve))
			{
				MFnAnimCurve OtherCurve(Object);
				if (OtherCurve.numKeys() != 0)
				{
					MaxTime = FMath::Max(MaxTime, OtherCurve.time(OtherCurve.numKeys() - 1).value());
				}
				break;
			}
		}
	}

	KeyFrames.clear();

	// Bake the anim curve
	int32 Key = 0;
	const int NumKeys = static_cast<int>(std::ceil(MaxTime));
	for (double Time = 0.0; Time <= MaxTime; ++Time, ++Key)
	{
		MTime MayaTime(Time, MTime::uiUnit());
		MDGContext TimeContext(MayaTime);
		MDGContextGuard ContextGuard(TimeContext);
		AnimCurve.BakeKeyFrame(Time, GetValueInternal(Camera), Key, NumKeys);
	}
}

double MFocalLengthRedirectCurve::GetValueInternal(const MFnCamera& Camera) const
{
	return MAngle(Camera.horizontalFieldOfView()).asUnits(MAngle::kDegrees);
}
