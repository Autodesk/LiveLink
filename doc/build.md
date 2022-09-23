# Building

## Getting and Building the Code

### **1. Tools and System Prerequisites**

Before building the project, consult the following table to ensure you use the recommended version of compiler, operating system, etc. 

|        Required       | ![](images/windows.png)   |   ![](images/linux.png)     |
|:---------------------:|:-------------------------:|:---------------------------:|
|    Operating System   |       Windows 10/11       |       CentOS 7/8            |
|   Compiler Requirement|     VS 2017/2019/2022     |     clang 11.0.1            |
| Supported Maya Version|      2020, 2022, 2023     |      2020, 2022, 2023       |

|        Optional       | ![](images/windows.png)   |   ![](images/linux.png)     |
|:---------------------:|:------------------------------------------------------------:|:---------------------------:|
|          Qt           | Maya 2020 = 5.12.5<br>Maya 2022 = 5.15.2<br>Maya 2023 = 5.15.2 | Maya 2020 = 5.12.5<br>Maya 2022 = 5.15.2<br>Maya 2023 = 5.15.2 |

<br>

### **2. Unreal Engine**
You need to build Unreal Engine from the source code before building the plugin.<br>
The source code is located at https://github.com/EpicGames/UnrealEngine.<br>
You need special access rights to clone the repository.<br>
For additional information on building Unreal Engine, follow the instructions in their own **README.md**.

* To build Unreal Engine 5.1.0 use this [tag](https://github.com/EpicGames/UnrealEngine/tree/5.1.0-release).

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
Create an environment variable called *MAYA_WIN_DIR_xxxx* where *xxxx* is replaced by the Maya year date of the SDK, e.g. 2020, 2022, 2023 etc.

- **Linux**: 
Create an environment variable called *MAYA_LNX_DIR_xxxx* where *xxxx* is replaced by the Maya year date of the SDK, e.g. 2020, 2022, 2023 etc.

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

You can build the plugins using different methods:
1. Go to the "*Source/Programs/MayaUnrealLiveLinkPlugin*" folder.<br>
    * **Windows**: Use the "*BuildMayaUnrealLiveLinkPlugin.bat*" batch file.<br>
    * **Linux**: Use the "*BuildMayaUnrealLiveLinkPlugin.sh*" shell script.
1. Go to the "*Plugins/Runtime/MayaLiveLink*" folder.<br>
    * **Windows**: Use the "*BuildUnrealPlugin.bat*" batch file.<br>
    * **Linux**: Use the "*BuildUnrealPlugin.sh*" shell script.
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

There are two arguments that must be passed to the "*BuildUnrealPlugin*" script: 

| Argument           | Description                               |
|--------------------|-------------------------------------------|
|  Package Folder    | The folder where to package the plugin.   |
|  Host Platform     | Host platforms to compile for.            |
|                    | Win64+Linux, Win64 or Linux               |

```
Linux:
➜ BuildUnrealPlugin.sh UnrealPackage Linux

Windows:
➜ BuildUnrealPlugin.bat UnrealPackage Win64
```

#### Build location

The binaries will be located under "*Engine\Restricted\NotForLicensees\Source\Binaries*" folder.
* Windows: Plugins for different Maya versions will be located under "*Win64\Maya*"
* Linux: Plugins for different Maya versions will be located under "*Linux\Maya*"

<br>

### **6. How To Run Unit Tests**
Unit tests can be found in the "*Engine/Restricted/NotForLicensees/test*" folder.

As an example, here is how to run the tests using the Maya 2023 Unreal Engine plugin:
```
Linux:
➜ /usr/autodesk/maya2023/bin/mayapy runTests.py 

Windows:
➜ "C:\Program Files\Autodesk\Maya2023\bin\mayapy.exe" runTests.py

Qt WebEngine seems to be initialized from a plugin. Please set Qt::AA_ShareOpenGLContexts using QCoreApplication::setAttribute before constructing QGuiApplication.
MayaUnrealLiveLinkPlugin initialized
LiveLinkUI Init:
        Registering Command 'MayaLiveLinkNotifyAndQuit'
        Registering Command 'MayaUnrealLiveLinkInitialized'        
        Registering Command 'MayaUnrealLiveLinkOnSceneOpen'        
        Registering Command 'MayaUnrealLiveLinkOnScenePreSave'     
        Registering Command 'MayaUnrealLiveLinkRefreshConnectionUI'
        Registering Command 'MayaUnrealLiveLinkRefreshUI'
        Registering Command 'MayaUnrealLiveLinkUI'
        Registering Command 'MayaUnrealLiveLinkUpdateLinkProgress' 
test_blendShapeStreaming.test_blendShapeStreamMultipleFrames : INFO : Started
LiveLinkAddSubjectCommand joint1
test_blendShapeStreaming.test_blendShapeStreamMultipleFrames : INFO : Completed

...

----------------------------------------------------------------------
Ran 25 tests in 3.900s

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

Alternatively, you can to `Windows -> Setting/Preferences -> Plug-in Manager`, type `Live` to filter the plugins and make sure `MayaUnrealLiveLinkPluginUI.py` and `MayaUnrealLiveLinkPlugin_5_1` are `Loaded` and set to `Auto load`.
