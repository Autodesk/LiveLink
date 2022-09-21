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

#include "MFilmbackRedirectCurve.h"
#include "MFocalLengthRedirectCurve.h"

#include <cmath>

THIRD_PARTY_INCLUDES_START
#include <maya/MDGContextGuard.h>
THIRD_PARTY_INCLUDES_END

MFilmbackRedirectCurve::MFilmbackRedirectCurve(const std::string& NameIn)
: MRedirectCurve(NameIn)
{
}

MFilmbackRedirectCurve::~MFilmbackRedirectCurve()
{
}

double MFilmbackRedirectCurve::GetValue(const MFnCamera& Camera) const
{
	return GetValueInternal(Camera);
}

void MFilmbackRedirectCurve::BakeKeyFrameRange(MStreamedEntity::MAnimCurve& AnimCurve,
											   const MFnCamera& Camera,
											   const MString& PlugName,
											   std::map<std::string, MStreamedEntity::MAnimCurve>& AnimCurves) const
{
	MStatus Status;
	MPlug OtherAnglePlug;
	if (PlugName == "hfa")
	{
		OtherAnglePlug = Camera.findPlug("vfa", true, &Status);
	}
	else if (PlugName == "vfa")
	{
		OtherAnglePlug = Camera.findPlug("hfa", true, &Status);
	}

	auto& KeyFrames = AnimCurve.KeyFrames;

	// Find the last frame to determine when to stop baking frames
	double MaxTime = 0;
	auto LastIter = KeyFrames.rbegin();
	if (LastIter != KeyFrames.rend())
	{
		MaxTime = LastIter->first;
	}

	// Also, check for the other curve since we need both curve values to determine the aspect ratio
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

	auto FieldOfViewAnimCurveIter = AnimCurves.find("FieldOfView");
	if (FieldOfViewAnimCurveIter == AnimCurves.end())
	{
		MStreamedEntity::MAnimCurve FieldOfViewAnimCurve;
		FieldOfViewAnimCurveIter = AnimCurves.emplace("FieldOfView", std::move(FieldOfViewAnimCurve)).first;
	}
	else
	{
		FieldOfViewAnimCurveIter->second.KeyFrames.clear();
	}

	KeyFrames.clear();

	// Bake the anim curves
	int32 Key = 0;
	MStreamedEntity::MAnimCurve& FieldOfViewAnimCurve = FieldOfViewAnimCurveIter->second;
	MFocalLengthRedirectCurve FocalLengthCurve("FieldOfView");
	const int NumKeys = static_cast<int>(std::ceil(MaxTime));
	for (double Time = 0.0; Time <= MaxTime; ++Time, ++Key)
	{
		MTime MayaTime(Time, MTime::uiUnit());
		MDGContext TimeContext(MayaTime);
		MDGContextGuard ContextGuard(TimeContext);
		AnimCurve.BakeKeyFrame(Time, GetValueInternal(Camera), Key, NumKeys);
		FieldOfViewAnimCurve.BakeKeyFrame(Time, FocalLengthCurve.GetValue(Camera), Key, NumKeys);
	}
}

double MFilmbackRedirectCurve::GetValueInternal(const MFnCamera& Camera) const
{
	return Camera.aspectRatio();
}
