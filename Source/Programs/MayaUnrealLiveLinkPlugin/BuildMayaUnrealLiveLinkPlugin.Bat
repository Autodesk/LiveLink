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

@rem If a platform is given then we build for it or we build Win64
if "%~2" == "" (
    @echo Missing Maya OS
    @echo Building for Win64
    SET PLATFORM=Win64
) else (
    SET PLATFORM=%2
)

if exist "%~dp0Staging\" (
  RMDIR "%~dp0Staging" /S /Q
)
"%~dp0..\..\..\..\..\Build\BatchFiles\RunUAT.bat"  BuildGraph -Script="%~dp0BuildMayaUnrealLiveLinkPlugin.xml" -Target="Stage Maya Plugin Module" -set:MayaVersion=%1 -set:MayaPlatform=%PLATFORM% -NoXGE