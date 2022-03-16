# Building

## Getting and Building the Code

### **1. Tools and System Prerequisites**

Before building the project, consult the following table to ensure you use the recommended version of compiler, operating system, cmake, etc. 

|        Required       | ![](images/windows.png)   |   ![](images/linux.png)     |
|:---------------------:|:-------------------------:|:---------------------------:|
|    Operating System   |         Windows 10        |       CentOS 7              |
|   Compiler Requirement| Maya 2019 (VS 2017)<br>Maya 2020 (VS 2017)<br>Maya 2022 (VS 2017/2019) | Maya 2020 (gcc 6.3.1)<br>Maya 2022 (gcc 6.3.1/9.3.1) |
| CMake Version (min/max) |        3.13 - 3.17      |           3.13 - 3.17       |
|         Python        |       2.7.15 or 3.7.7     |        2.7.15 or 3.7.7      |
|    Python Packages    | PyYAML, PySide, PyOpenGL, Jinja2        |PyYAML, PySide, PyOpenGL, Jinja2 |
|    Build generator    | Visual Studio, Ninja (Recommended)    |    Ninja (Recommended)      |
|    Command processor  | Visual Studio x64 2017 or 2019 command prompt  |             bash            |
| Supported Maya Version|     2019, 2020, 2022      |      2020, 2022       |

|        Optional       | ![](images/windows.png)   |   ![](images/linux.png)     |
|:---------------------:|:------------------------------------------------------------:|:---------------------------:|
|          Qt           | Maya 2019 = 5.6.1<br>Maya 2020 = 5.12.5<br>Maya 2022 = 5.15.2 | Maya 2020 = 5.12.5<br>Maya 2022 = 5.15.2 |

<br>

### **2. Unreal Engine**
You can install the Unreal Engine (Windows only) or you can build it yourself from the source code.

#### **1. Install Unreal Engine (Windows only)**
You can download it from https://www.unrealengine.com and install it.

#### **2. Download and build the Unreal Engine source code**
The source code is located at https://github.com/EpicGames/UnrealEngine.<br>
You need special access rights to clone the repository.<br>

* To build Unreal Engine 4.27.2 use this [tag](https://github.com/EpicGames/UnrealEngine/tree/4.27.2-release).
* To build Unreal Engine 5.0.0 preview 1 use this [tag](https://github.com/EpicGames/UnrealEngine/tree/5.0.0-preview-1).

For additional information on building Unreal Engine, follow the instructions in their own **README.md**.

<br>

### **3. Download the plugin source code**
Create a folder structure under the "Engine" folder that looks like this: Restricted/NotForLicensees.
Navigate to the "NotForLicensees" folder in a command shell and the repository:
```
git clone https://github.com/Autodesk/LiveLink.git
```

#### Repository Layout
Under the "Engine/Restricted/NotForLicensees" folder, you will see the folder layout.

| Location                                          | Description                                      |
|---------------------------------------------------|--------------------------------------------------|
| resource                                          | The resource folder                              |
| Source                                            | The Autodesk Maya plugin                         |
| Source/Programs/MayaUnrealLiveLinkPlugin          | The MayaUnrealLiveLinkPlugin module and plugin   |
| Source/Programs/MayaUnrealLiveLinkPlugin/icons    | The icons folder                                 |
| Source/Programs/MayaUnrealLiveLinkPlugin/scripts  | The Python UI classes                            |
| Source/Programs/MayaUnrealLiveLinkPlugin/Subjects | The Subjects classes                             |
| Source/Programs/UnrealInitializer                 | The Unreal initializer and stream manager module |
| test                                              | The Python unit tests                            |

<br>

### **4. How to build the plugin**

You can build the plugin using different methods:
1. Go to the "*Source/Programs/MayaUnrealLiveLinkPlugin*" folder.<br>
    * **Windows**: Use the "*BuildMayaUnrealLiveLinkPlugin.bat*" batch file.<br>
    * **Linux**: Use the "*BuildMayaUnrealLiveLinkPlugin.sh*" shell script.
2. **Windows only**: Open the Unreal Engine .sln file and build the Engine using the "Development Editor" configuration.

#### Arguments

There are two arguments that must be passed to the "*BuildMayaUnrealLiveLinkPlugin*" script: 

| Argument           | Description                               |
|--------------------|-------------------------------------------|
|  MayaVersion       | Maya yearly version to build for.         |
|  Platform          | Use either Win64 or Linux                 |

```
Linux:
➜ BuildMayaUnrealLiveLinkPlugin 2022 Linux

Windows:
cd /d c:\UnrealEngine\Engine\Restricted\NotForLicensees\Source\MayaUnrealLiveLinkPlugin
➜ MayaUnrealLiveLinkPlugin 2022 Win64
```

#### Build location

The binaries will be located under "*Engine\Restricted\NotForLicensees\Source\Binaries*" folder.

<br>

### **6. How To Run Unit Tests**

Unit tests can be run by setting ```--stages=test``` or by simply calling `ctest` directly from the build directory.

For example, to run all Animal Logic's tests from the command-line go into ```build/<variant>/plugin/al``` and call `ctest`.

```
➜  ctest -j 8
Test project /Users/sabrih/Desktop/workspace/build/Debug/plugin/al
    Start 4: AL_USDMayaTestPlugin
    Start 5: TestUSDMayaPython
    Start 8: TestPxrUsdTranslators
    Start 7: TestAdditionalTranslators
    Start 1: AL_MayaUtilsTests
    Start 3: Python:AL_USDTransactionTests
    Start 2: GTest:AL_USDTransactionTests
    Start 6: testMayaSchemas
1/8 Test #2: GTest:AL_USDTransactionTests .....   Passed    0.06 sec
2/8 Test #6: testMayaSchemas ..................   Passed    0.10 sec
3/8 Test #3: Python:AL_USDTransactionTests ....   Passed    0.73 sec
4/8 Test #1: AL_MayaUtilsTests ................   Passed    6.01 sec
5/8 Test #8: TestPxrUsdTranslators ............   Passed    9.96 sec
6/8 Test #5: TestUSDMayaPython ................   Passed   10.28 sec
7/8 Test #7: TestAdditionalTranslators ........   Passed   12.06 sec
8/8 Test #4: AL_USDMayaTestPlugin .............   Passed   27.43 sec
100% tests passed, 0 tests failed out of 8
```

# Additional Build Instruction

##### Python:

It is important to use the Python version shipped with Maya and not the system version when building USD on MacOS. Note that this is primarily an issue on MacOS, where Maya's version of Python is likely to conflict with the version provided by the system. 

To build USD and the Maya plug-ins on MacOS for Maya (2019, 2020, 2022), run:
```
/Applications/Autodesk/maya2019/Maya.app/Contents/bin/mayapy build_usd.py ~/Desktop/BUILD
```
By default, ``usdview`` is built which has a dependency on PyOpenGL. Since the Python version of Maya doesn't ship with PyOpenGL you will be prompted with the following error message:
```
PyOpenGL is not installed. If you have pip installed, run "pip install PyOpenGL" to install it, then re-run this script.
If PyOpenGL is already installed, you may need to update your ```PYTHONPATH``` to indicate where it is located.
```
The easiest way to bypass this error is by setting ```PYTHONPATH``` to point at your system python or third-party python package manager that has PyOpenGL already installed.
e.g
```
export PYTHONPATH=$PYTHONPATH:Library/Frameworks/Python.framework/Versions/2.7/lib/python2.7/site-packages
```
Use `pip list` to see the list of installed packages with your python's package manager.

e.g
```
➜ pip list
DEPRECATION: Python 2.7 will reach the end of its life on January 1st, 2020. Please upgrade your Python as Python 2.7 won't be maintained after that date. A future version of pip will drop support for Python 2.7.
Package    Version
---------- -------
Jinja2     2.10   
MarkupSafe 1.1.0  
pip        19.1.1 
PyOpenGL   3.1.0  
PySide2    5.12.1 
PyYAML     3.13   
setuptools 39.0.1 
shiboken2  5.12.1 
```

##### Dependencies on Linux DSOs when running tests

Normally either runpath or rpath are used on some DSOs in this library to specify explicit on other libraries (such as USD itself)

If for some reason you don't want to use either of these options, and switch them off with:
```
CMAKE_SKIP_RPATH=TRUE
```
To allow your tests to run, you can inject LD_LIBRARY_PATH into any of the mayaUSD_add_test calls by setting the ADDITIONAL_LD_LIBRARY_PATH cmake variable to $ENV{LD_LIBRARY_PATH} or similar.

There is a related ADDITIONAL_PXR_PLUGINPATH_NAME cmake var which can be used if schemas are installed in a non-standard location

<br>

# How to Load Plug-ins in Maya 

The provided module file (*.mod) facilitates setting various environment variables for plugins and libraries. After the project is successfully built, ```mayaUsd.mod``` is installed inside the install directory. In order for Maya to discover this mod file, ```MAYA_MODULE_PATH``` environment variable needs to be set to point to the location where the mod file is installed.
Examples:
```
set MAYA_MODULE_PATH=C:\workspace\install\RelWithDebInfo
export MAYA_MODULE_PATH=/usr/local/workspace/install/RelWithDebInfo
```
Once MAYA_MODULE_PATH is set, run maya and go to ```Windows -> Setting/Preferences -> Plug-in Manager``` to load the plugins.

![](images/plugin_manager.png) 
