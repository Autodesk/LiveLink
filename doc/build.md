# Building

## Getting and Building the Code

### **1. Tools and System Prerequisites**

Before building the project, consult the following table to ensure you use the recommended version of compiler, operating system, etc. 

|        Required       | ![](images/windows.png)   |   ![](images/linux.png)     |
|:---------------------:|:-------------------------:|:---------------------------:|
|    Operating System   |         Windows 10        |       CentOS 7/8            |
|   Compiler Requirement|       VS 2017/2019        |     clang 11.0.1            |
| Supported Maya Version|     2019, 2020, 2022, 2023|      2020, 2022, 2023       |

|        Optional       | ![](images/windows.png)   |   ![](images/linux.png)     |
|:---------------------:|:------------------------------------------------------------:|:---------------------------:|
|          Qt           | Maya 2019 = 5.6.1<br>Maya 2020 = 5.12.5<br>Maya 2022 = 5.15.2<br>Maya 2023 = 5.15.2 | Maya 2020 = 5.12.5<br>Maya 2022 = 5.15.2<br>Maya 2023 = 5.15.2 |

<br>

### **2. Unreal Engine**
You need to build Unreal Engine from the source code before building the plugin.<br>
The source code is located at https://github.com/EpicGames/UnrealEngine.<br>
You need special access rights to clone the repository.<br>
For additional information on building Unreal Engine, follow the instructions in their own **README.md**.

* To build Unreal Engine 4.27.2 use this [tag](https://github.com/EpicGames/UnrealEngine/tree/4.27.2-release).
    * Linux users need to merge this pull request to avoid memory crashes:<br>
    https://github.com/EpicGames/UnrealEngine/pull/8710
    * Linux users also have to edit *SDL2.Build.cs* in "*Engine\Source\ThirdParty\SDL2*" to comment or remove `"Target.LinkType == TargetLinkType.Monolithic"` block so only the `if` and `else` remains.
* To build Unreal Engine 5.0.0 use this [tag](https://github.com/EpicGames/UnrealEngine/tree/5.0.0-release).

<br>

### **3. Install third party libraries**
#### **Maya SDK**
##### **Download SDK**

You need to download and unzip the Maya SDK for your version of Maya at a location of your choice.

The SDKs can be found [here](https://www.autodesk.com/developer-network/platform-technologies/maya).

##### **Setting up the environment variable**

To build the plugin, you will need to tell to where to find the Maya SDK you just downloaded.
We are using an environment variable to know its location. The location should be set on the `devkitBase` folder.

- **Windows**: 
Create an environment variable called *MAYA_WIN_DIR_xxxx* where *xxxx* is replaced by the Maya year date of the SDK, e.g. 2019, 2020, 2022, etc.

- **Linux**: 
Create an environment variable called *MAYA_LNX_DIR_xxxx* where *xxxx* is replaced by the Maya year date of the SDK, e.g. 2019, 2020, 2022, etc.

For example, if you extracted the Maya 2022 SDK on Windows to `c:\MayaDevKits\2022`, the environment variable would be:
```
MAYA_WIN_DIR_2022=c:\MayaDevKits\2022\devkitBase
```

<br>

### **4. Download the plugin source code**
Create a folder structure under the "Engine" folder that looks like this: Restricted/NotForLicensees.
Navigate to the "NotForLicensees" folder in a command shell and the repository:
```
git clone https://github.com/Autodesk/LiveLink.git .
```

### Repository Layout
Under the "*Engine/Restricted/NotForLicensees*" folder, you will see the folder layout.

| Location                                                   | Description                                      |
|------------------------------------------------------------|--------------------------------------------------|
| resource                                                   | The resource folder                              |
| Source                                                     | The Autodesk Maya plugin                         |
| Source/Programs/MayaUnrealLiveLinkPlugin                   | The MayaUnrealLiveLinkPlugin module and plugin   |
| Source/Programs/MayaUnrealLiveLinkPlugin/icons             | The icons folder                                 |
| Source/Programs/MayaUnrealLiveLinkPlugin/scripts           | The Python UI classes                            |
| Source/Programs/MayaUnrealLiveLinkPlugin/Subjects          | The Subjects classes                             |
| Source/Programs/MayaUnrealLiveLinkPlugin/UnrealInitializer | The Unreal initializer and stream manager module |
| test                                                       | The Python unit tests                            |

<br>

### **5. How to build the plugin**

You can build the plugin using different methods:
1. Go to the "*Source/Programs/MayaUnrealLiveLinkPlugin*" folder.<br>
    * **Windows**: Use the "*BuildMayaUnrealLiveLinkPlugin.bat*" batch file.<br>
    * **Linux**: Use the "*BuildMayaUnrealLiveLinkPlugin.sh*" shell script.
2. **Windows only**: Open the Unreal Engine .sln file and build the Engine using the "Development Editor" configuration.
3. **VSCode/CMake**: Set the environment variables MAYA_LOCATION and MAYA_DEVKIT_LOCATION to the proper location before launching VSCode. Build with RelWithDebInfo to compile plugin. After opening a python file the Test Explorer will detect all the python unittests. UE version, MayaVersion and Platform will be detected automatically.

#### Arguments method (1)

There are two arguments that must be passed to the "*BuildMayaUnrealLiveLinkPlugin*" script: 

| Argument           | Description                               |
|--------------------|-------------------------------------------|
|  MayaVersion       | Maya yearly version to build for.         |
|  Platform          | Use either Win64 or Linux                 |

```
Linux:
➜ BuildMayaUnrealLiveLinkPlugin.sh 2022 Linux

Windows:
➜ MayaUnrealLiveLinkPlugin.bat 2022 Win64
```

#### Build location

The binaries will be located under "*Engine\Restricted\NotForLicensees\Source\Binaries*" folder.
* Windows: Plugins for different Maya versions will be located under "*Win64\Maya*"
* Linux: Plugins for different Maya versions will be located under "*Linux\Maya*"

<br>

### **6. How To Run Unit Tests**
Unit tests can be found in the "*test*" folder.

As an example, here is how to run the tests using the Maya 2022 Unreal Engine 4.27.2 plugin:
```
rem Testing the Unreal Engine 4.27.2 plugin
set UNITTEST_UNREAL_VERSION=4_27

rem Set the Maya Version to use
set MAYA_VERSION=2022

rem To test the Unreal Engine 5 plugin, use this line instead
rem set UNITTEST_UNREAL_VERSION=5_0

C:\UnrealEngine\Engine\Restricted\NotForLicensees>cd test

C:\UnrealEngine\Engine\Restricted\NotForLicensees>"C:\Program Files\Autodesk\Maya2022\bin\bin\mayapy.exe" runTests.py

 MayaUnrealLiveLinkPlugin initialized
LiveLink:
        Registering Command 'MayaLiveLinkNotifyAndQuit'
        Registering Command 'MayaUnrealLiveLinkInitialized'
        Registering Command 'MayaUnrealLiveLinkRefreshConnectionUI'
        Registering Command 'MayaUnrealLiveLinkRefreshUI'
        Registering Command 'MayaUnrealLiveLinkUI'
test_blendShapeStreaming.test_blendShapeStreamMultipleFrames : INFO : Started
LiveLinkAddSubjectCommand joint1
test_blendShapeStreaming.test_blendShapeStreamMultipleFrames : INFO : Completed
.test_blendShapeStreaming.test_blendShapeStreamSingleFrame : INFO : Started
LiveLinkAddSubjectCommand joint1
test_blendShapeStreaming.test_blendShapeStreamSingleFrame : INFO : Completed
.test_camera.test_cameraAspectRatio : INFO : Started
LiveLinkAddSubjectCommand camera1
test_camera.test_cameraAspectRatio : INFO : Completed
.test_camera.test_cameraDoF : INFO : Started
LiveLinkAddSubjectCommand camera1
test_camera.test_cameraDoF : INFO : Completed
.test_camera.test_cameraFoV : INFO : Started
LiveLinkAddSubjectCommand camera1
test_camera.test_cameraFoV : INFO : Completed
.test_camera.test_cameraFocalLength : INFO : Started
LiveLinkAddSubjectCommand camera1
test_camera.test_cameraFocalLength : INFO : Completed
.test_camera.test_cameraTranslationRotation : INFO : Started
LiveLinkAddSubjectCommand camera1
test_camera.test_cameraTranslationRotation : INFO : Completed
.test_customAttributes.test_jointsWithCustomAttr : INFO : Started
LiveLinkAddSubjectCommand joint1
test_customAttributes.test_jointsWithCustomAttr : INFO : Completed
.test_customAttributes.test_propWithCustomAttr : INFO : Started
LiveLinkAddSubjectCommand joint1
test_customAttributes.test_propWithCustomAttr : INFO : Completed
.test_lights.test_directionalLight : INFO : Started
LiveLinkAddSubjectCommand directionalLight1
test_lights.test_directionalLight : INFO : Completed
.test_lights.test_pointLight : INFO : Started
LiveLinkAddSubjectCommand pointLight1
test_lights.test_pointLight : INFO : Completed
.test_lights.test_spotLight : INFO : Started
LiveLinkAddSubjectCommand spotLight1
test_lights.test_spotLight : INFO : Completed
.test_mayaAppTest.test_emptyBaseAnimation : INFO : Started
test_mayaAppTest.test_emptyBaseAnimation : INFO : Completed
.test_mayaAppTest.test_polyAnimation : INFO : Started
test_mayaAppTest.test_polyAnimation : INFO : Completed
.test_propTransfroms : INFO : Started
LiveLinkAddSubjectCommand pCube1
test_propTransfroms : INFO : Completed
.test_rename.test_renameCamera : INFO : Started
LiveLinkAddSubjectCommand camera

LiveLinkAddSubjectCommand camera2
LiveLinkAddSubjectCommand camera3
test_rename.test_renameCamera : INFO : Completed
.test_rename.test_renameProp : INFO : Started
LiveLinkAddSubjectCommand prop

LiveLinkAddSubjectCommand prop2
LiveLinkAddSubjectCommand prop3
test_rename.test_renameProp : INFO : Completed
.test_rename.test_renameSpotLight : INFO : Started
LiveLinkAddSubjectCommand spotlight

LiveLinkAddSubjectCommand spotlight2
LiveLinkAddSubjectCommand spotlight3
test_rename.test_renameSpotLight : INFO : Completed
.test_rename.test_renameskeletonWithTransfNode : INFO : Started
LiveLinkAddSubjectCommand Root

LiveLinkAddSubjectCommand Root2
LiveLinkAddSubjectCommand Root3
test_rename.test_renameskeletonWithTransfNode : INFO : Completed
.test_skeletonHierarchy.test_JointOnly : INFO : Started
LiveLinkAddSubjectCommand Root
test_skeletonHierarchy.test_JointOnly : INFO : Completed
.test_skeletonHierarchy.test_TransfNodeOnly : INFO : Started
LiveLinkAddSubjectCommand Root
test_skeletonHierarchy.test_TransfNodeOnly : INFO : Completed
.test_skeletonHierarchy.test_skeletonWithTransfNode : INFO : Started
LiveLinkAddSubjectCommand Root
test_skeletonHierarchy.test_skeletonWithTransfNode : INFO : Completed
.
----------------------------------------------------------------------
Ran 22 tests in 0.806s

OK
```

<br>

# How to Load Plug-ins in Maya 
You need to edit the `Maya.env` for the Maya version you're using to add the plugin path.
Note that there are 2 paths to set, one for the `.mll`/`.so` files and one for the UI `.py` script.

Example for Maya on Windows:
```
MAYA_MODULE_PATH=C:\UnrealEngine\Engine\Restricted\NotForLicensees\Binaries\Win64\Maya

MAYA_PLUG_IN_PATH=C:\UnrealEngine\Engine\Restricted\NotForLicensees\Source\Programs\MayaUnrealLiveLinkPlugin

MAYA_SCRIPT_PATH=C:\UnrealEngine\Engine\Restricted\NotForLicensees\Source\Programs\MayaUnrealLiveLinkPlugin
```
Once this is set, run Maya, go to `File -> Unreal Live Link` to start the UI and select which Unreal version of the plugin if want to use.

Alternatively, you can to `Windows -> Setting/Preferences -> Plug-in Manager`, type `Live` to filter the plugins and make sure `MayaUnrealLiveLinkPluginUI.py` is `Loaded` and set to `Auto load` and either `MayaUnrealLiveLinkPlugin_4_27` or `MayaUnrealLiveLinkPlugin_5_0` is loaded and auto load, but not both of them at the same time.
