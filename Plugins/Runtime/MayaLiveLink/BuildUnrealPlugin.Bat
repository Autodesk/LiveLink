@REM MIT License

@REM Copyright (c) 2022 Autodesk, Inc.

@REM Permission is hereby granted, free of charge, to any person obtaining a copy
@REM of this software and associated documentation files (the "Software"), to deal
@REM in the Software without restriction, including without limitation the rights
@REM to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
@REM copies of the Software, and to permit persons to whom the Software is
@REM furnished to do so, subject to the following conditions:

@REM The above copyright notice and this permission notice shall be included in all
@REM copies or substantial portions of the Software.

@REM THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
@REM IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
@REM FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
@REM AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
@REM LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
@REM OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
@REM SOFTWARE.

@echo off

SET PACKAGE_FOLDER="UnrealPackage"
SET HOST_PLATFORMS="Win64+Linux"

@rem Check if a package folder exists
if "%~1" == "" (
    @echo Missing Parameters
    @echo BuildUnrealPlugin.bat [^<package_folder^>] [^<host_platforms^>]
	@echo   package_folder          The folder where to package the plugin. Defaults to "UnrealPackage"
    @echo   host_platforms          Host platforms to compile for. Defaults to "Win64+Linux"
) else (
    SET PACKAGE_FOLDER=%1
)

@rem Check if the host plaforms exist
if "%~2" == "" (
    @echo Missing target version
	@echo Building for Win64 and Linux
) else (
    SET HOST_PLATFORMS=%2
)

"%~dp0..\..\..\..\..\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin=Engine\Restricted\NotForLicensees\Plugins\Runtime\MayaLiveLink\MayaLiveLink.uplugin -Package=%1 -HostPlatforms=%2 -NoTargetPlatforms -Target=UnrealEditor -NoXGE

