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

#include "MRedirectCurve.h"

// Redirect curve from filmback to aspect ratio for CameraActors
class MFilmbackRedirectCurve : public MRedirectCurve<MFnCamera>
{
public:
	MFilmbackRedirectCurve(const std::string& NameIn);
	virtual ~MFilmbackRedirectCurve();

	virtual double GetValue(const MFnCamera& Camera) const override;

	virtual void BakeKeyFrameRange(MStreamedEntity::MAnimCurve& AnimCurve,
								   const MFnCamera& Camera,
								   const MString& PlugName,
								   std::map<std::string, MStreamedEntity::MAnimCurve>& AnimCurves) const;

private:
	double GetValueInternal(const MFnCamera& Camera) const;
};
