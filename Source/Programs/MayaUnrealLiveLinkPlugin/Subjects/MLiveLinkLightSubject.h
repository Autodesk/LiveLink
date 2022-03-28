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

#include "Roles/LiveLinkLightTypes.h"
#include "IMStreamedEntity.h"
#include "MStreamedEntity.h"


class MLiveLinkLightSubject : public IMStreamedEntity, public MStreamedEntity
{
public:
	enum FLightStreamMode : uint16_t
	{
		RootOnly,
		FullHierarchy,
		Light,
	};

	static MStringArray LightStreamOptions;

	MLiveLinkLightSubject(const MString& InSubjectName, const MDagPath& InRootPath,
		FLightStreamMode InStreamMode = FLightStreamMode::Light);

	~MLiveLinkLightSubject();

	virtual bool ShouldDisplayInUI() const override;
	virtual MDagPath GetDagPath() const override;
	virtual MString GetNameDisplayText() const override;
	virtual MString GetRoleDisplayText() const override;
	virtual MString GetSubjectTypeDisplayText() const override;

	virtual bool ValidateSubject() const override;
	virtual bool RebuildSubjectData() override;
	virtual void OnStream(double StreamTime, double CurrentTime) override;
	virtual void SetStreamType(const MString& StreamTypeIn) override;
	virtual int GetStreamType() const override;

private:
	MString SubjectName;
	MDagPath RootDagPath;

	FLightStreamMode StreamMode;
};