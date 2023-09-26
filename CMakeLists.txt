cmake_minimum_required(VERSION 3.0)

project(MayaLiveLink NONE) # Enable no language.

set(MAYA_VERSIONS 2022 2023 2024)
if(WIN32)
    set(Platform Win64)
    set(Extension .mll)
    set(MayaDir WIN)
    set(ScriptExtension .bat)
else()
    set(Platform Linux)
    set(Extension ${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(MayaDir LNX)
    set(ScriptExtension .sh)
endif()

set(MAYA_LOCATION $ENV{MAYA_LOCATION})
if(NOT EXISTS ${MAYA_LOCATION})
    message(WARNING "MAYA_LOCATION not set properly, unitests will not be available! Current value is: ${MAYA_LOCATION} ")
endif()

set(MAYA_DEVKIT_LOCATION $ENV{MAYA_DEVKIT_LOCATION})
if(EXISTS ${MAYA_DEVKIT_LOCATION})
    # Detect Maya version from Devkit
    if(EXISTS "${MAYA_DEVKIT_LOCATION}/include/maya/MTypes.h")
        # Tease the MAYA_API_VERSION numbers from the lib headers
        file(STRINGS ${MAYA_DEVKIT_LOCATION}/include/maya/MTypes.h TMP REGEX "#define MAYA_API_VERSION.*$")
        string(REGEX MATCHALL "[0-9]+" MAYA_API_VERSION ${TMP})
        string(SUBSTRING ${MAYA_API_VERSION} 0 4 MayaVersion)
    else()
        message(FATAL_ERROR "Unable to determine the Maya version from ${MAYA_DEVKIT_LOCATION}")
    endif()
endif()

# Detect Engine version
set(UE_ENGINE_DIR ${CMAKE_SOURCE_DIR}/../..)
set(UE_BASE_DIR ${UE_ENGINE_DIR}/..)
set(UE_VERSION_FILE ${UE_ENGINE_DIR}/Source/Runtime/Launch/Resources/Version.h)
if(EXISTS ${UE_VERSION_FILE})
    file(STRINGS ${UE_VERSION_FILE} TMP REGEX "#define ENGINE_MAJOR_VERSION.*$")
    string(REGEX MATCHALL "[0-9]+" ENGINE_MAJOR_VERSION ${TMP})
    file(STRINGS ${UE_VERSION_FILE} TMP REGEX "#define ENGINE_MINOR_VERSION.*$")
    string(REGEX MATCHALL "[0-9]+" ENGINE_MINOR_VERSION ${TMP})
    set(UEVersion ${ENGINE_MAJOR_VERSION}_${ENGINE_MINOR_VERSION})
else()
    message(FATAL_ERROR "Unable to determine the Engine version from ${UE_VERSION_FILE}")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(BuildTarget Debug)
elseif(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
    set(BuildTarget Development)
else()
    set(BuildTarget Shipping)
endif()

set(MAYA_MODULE_PATH ${CMAKE_SOURCE_DIR}/Binaries/${Platform}/Maya)
set(CMAKE_BINARY_DIR ${MAYA_MODULE_PATH}/${MayaVersion})
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})

message(STATUS "Using MAYA_LOCATION        = ${MAYA_LOCATION}")
if(NOT "${MAYA_DEVKIT_LOCATION}" STREQUAL "")
    message(STATUS "Using MAYA_DEVKIT_LOCATION = ${MAYA_DEVKIT_LOCATION}")
endif()
foreach(year ${MAYA_VERSIONS})
    message(STATUS "Using MAYA_${MayaDir}_DIR_${year}    = $ENV{MAYA_${MayaDir}_DIR_${year}}")
    if(NOT "${MAYA_DEVKIT_LOCATION}" STREQUAL "") # Set the MayaVersion only if it was not already set. (when setting MAYA_DEVKIT_LOCATION)
        set(MayaVersion ${year})
    endif()
endforeach()
message(STATUS "Using MayaVersion          = ${MayaVersion}")
message(STATUS "Using MAYA_MODULE_PATH     = ${MAYA_MODULE_PATH}")
message(STATUS "Using UEVersion            = ${UEVersion}")
message(STATUS "Using BuildTarget          = ${BuildTarget}")

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR})

set(MAYA_SDK_FOUND FALSE)
foreach(year ${MAYA_VERSIONS})
    if(EXISTS $ENV{MAYA_${MayaDir}_DIR_${year}})
        # Build Maya Plugin
        add_custom_target(
            MayaPlugin${year} ALL
            COMMAND "${CMAKE_SOURCE_DIR}/../../Build/BatchFiles/RunUAT${ScriptExtension}" BuildGraph -Script=Engine/Restricted/NotForLicensees/Source/Programs/MayaUnrealLiveLinkPlugin/BuildMayaUnrealLiveLinkPlugin.xml -Target=\"Stage Maya Plugin Module\" -set:MayaVersion=${MayaVersion} -set:MayaPlatform=${Platform} -set:BuildTarget=${BuildTarget} -NoXGE
            COMMENT "Building Maya Plugin"
        )
        set(MAYA_SDK_FOUND TRUE)
    endif()
endforeach()

if(NOT ${MAYA_SDK_FOUND} AND NOT "${MAYA_DEVKIT_LOCATION}" STREQUAL "")
    # Build Maya Plugin
    add_custom_target(
        MayaPlugin${MayaVersion} ALL
        COMMAND ${CMAKE_COMMAND} -E env MAYA_${MayaDir}_DIR_${MayaVersion}=${MAYA_DEVKIT_LOCATION} "${CMAKE_SOURCE_DIR}/../../Build/BatchFiles/RunUAT${ScriptExtension}" BuildGraph -Script=Engine/Restricted/NotForLicensees/Source/Programs/MayaUnrealLiveLinkPlugin/BuildMayaUnrealLiveLinkPlugin.xml -Target=\"Stage Maya Plugin Module\" -set:MayaVersion=${MayaVersion} -set:MayaPlatform=${Platform} -set:BuildTarget=${BuildTarget} -NoXGE
        COMMENT "Building Maya Plugin"
    )
    set(MAYA_SDK_FOUND TRUE)
endif()

if(NOT ${MAYA_SDK_FOUND})
    # Devkit location: https://www.autodesk.com/developer-network/platform-technologies/maya#
    message(FATAL_ERROR "MAYA_DEVKIT_LOCATION or MAYA_${MayaDir}_DIR_xxxx not set properly!")
endif()

# Build Unreal Plugin
set(Package UnrealPackage)
#set(HostPlatforms "Win64+Linux")   # This would be for cross compile
set(HostPlatforms "${Platform}")

add_custom_target(
    UnrealPlugin ALL
    COMMAND "${CMAKE_SOURCE_DIR}/../../Build/BatchFiles/RunUAT${ScriptExtension}" BuildPlugin -Plugin=Engine/Restricted/NotForLicensees/Plugins/Runtime/MayaLiveLink/MayaLiveLink.uplugin -Package=${Package} -HostPlatforms=${HostPlatforms} -NoTargetPlatforms -Target=UnrealEditor -NoXGE
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${UE_BASE_DIR}/${Package}/Binaries ${CMAKE_SOURCE_DIR}/Plugins/Runtime/MayaLiveLink/Binaries
    COMMENT "Building Unreal Plugin"
)

if(EXISTS ${CMAKE_SOURCE_DIR}/Source/Programs/MayaLiveLinkUPluginInstaller/BuildMayaLiveLinkUPluginInstaller.xml)
    # Build Installer
    add_custom_target(
        Installer ALL
        COMMAND "${CMAKE_SOURCE_DIR}/../../Build/BatchFiles/RunUAT${ScriptExtension}" BuildGraph -Script=Engine/Restricted/NotForLicensees/Source/Programs/MayaLiveLinkUPluginInstaller/BuildMayaLiveLinkUPluginInstaller.xml -Target="Stage Unreal Plugin Installer" -set:InstallerPlatform=${Platform} -set:BuildTarget=Shipping -NoXGE
    #    DEPENDS MayaPlugin UnrealPlugin
        COMMENT "Building Installer"
    )

    set(UNREAL_PLUGIN_FOLDER "Engine/Restricted/NotForLicensees")
    file(TO_NATIVE_PATH "${UNREAL_PLUGIN_FOLDER}" UNREAL_PLUGIN_FOLDER)
    set(UNREAL5_BASE_FOLDER ${UE_BASE_DIR})
    file(TO_NATIVE_PATH "${UNREAL5_BASE_FOLDER}" UNREAL5_BASE_FOLDER)
    set(UNREAL5LINK_PLUGIN_FOLDER "${UE_BASE_DIR}/UnrealPackage")
    file(TO_NATIVE_PATH "${UNREAL5LINK_PLUGIN_FOLDER}" UNREAL5LINK_PLUGIN_FOLDER)
    set(UNREAL5_ENGINE_VERSION ${UEVersion})

    # InstallerSetup
    add_custom_target(
        InstallerSetup
        COMMAND ${CMAKE_COMMAND} -E env UNREAL_PLUGIN_FOLDER=${UNREAL_PLUGIN_FOLDER}
                                    env UNREAL5_BASE_FOLDER=${UNREAL5_BASE_FOLDER}
                                    env UNREAL5LINK_PLUGIN_FOLDER=${UNREAL5LINK_PLUGIN_FOLDER}
                                    env UNREAL5_ENGINE_VERSION=${UNREAL5_ENGINE_VERSION}
                                    "${CMAKE_SOURCE_DIR}/build-scripts/InstallerSetup.bat" Installer
    #    DEPENDS MayaPlugin UnrealPlugin Installer
        COMMENT "InstallerSetup"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

# Create .env file for setting environment variables for Python unit tests
file(WRITE ${CMAKE_SOURCE_DIR}/.env "MAYA_VERSION=${MayaVersion}")
