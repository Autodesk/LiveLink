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

// A redirect curve takes a given anim curve and it replaces it with another curve that
// Maya doesn't allow to key or that doesn't have a direct equivalent on the Unreal side
template<typename T>
class MRedirectCurve
{
public:
	MRedirectCurve(const std::string& NameIn) : Name(NameIn) {}
	virtual ~MRedirectCurve() {}

	const std::string& GetName() const { return Name; }

	virtual double GetValue(const T& Camera) const = 0;
	virtual void BakeKeyFrameRange(MStreamedEntity::MAnimCurve& AnimCurve,
								   const T& Type,
								   const MString& PlugName,
								   std::map<std::string, MStreamedEntity::MAnimCurve>& AnimCurves) const = 0;

private:
	std::string Name;
};