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
#include "MLiveLinkActiveCamera.h"

// Boilerplate for importing OpenMaya
#if defined PLATFORM_WINDOWS
#include "Windows/WindowsPlatformCompilerPreSetup.h"
#else
#include "Unix/UnixPlatformCompilerPreSetup.h"
#endif

// Import OpenMaya headers
THIRD_PARTY_INCLUDES_START
#include <maya/M3dView.h>
THIRD_PARTY_INCLUDES_END


MString MLiveLinkActiveCamera::ActiveCameraName("EditorActiveCamera");

MLiveLinkActiveCamera::MLiveLinkActiveCamera() 
: MLiveLinkBaseCameraSubject(ActiveCameraName) {}

MDagPath MLiveLinkActiveCamera::GetDagPath() const
{
    return CurrentActiveCameraDag;
}

void MLiveLinkActiveCamera::OnStream(double StreamTime, double CurrentTime)
{
    MStatus Status;
    M3dView ActiveView = M3dView::active3dView(&Status);
    if (Status == MStatus::kSuccess)
    {
        MDagPath CameraDag;
		if (ActiveView.getCamera(CameraDag) == MStatus::kSuccess &&
			!(CameraDag == CurrentActiveCameraDag))
		{
			CurrentActiveCameraDag = CameraDag;
		}
	}
	if (CurrentActiveCameraDag.isValid())
	{
		StreamCamera(CurrentActiveCameraDag, StreamTime, CurrentTime);
	}
}