cmake_minimum_required(VERSION 3.0)

project(MayaLiveLink NONE) # Enable no language.

set(MAYA_LOCATION $ENV{MAYA_LOCATION})
if(NOT EXISTS ${MAYA_LOCATION})
    message(WARNING "MAYA_LOCATION not set properly, unitests will not be available! Current value is: ${MAYA_LOCATION} ")
endif()

set(MAYA_DEVKIT_LOCATION $ENV{MAYA_DEVKIT_LOCATION})
if(NOT EXISTS ${MAYA_DEVKIT_LOCATION})
    message(FATAL_ERROR "MAYA_DEVKIT_LOCATION not set properly! Current value is: ${MAYA_DEVKIT_LOCATION} ")
endif()

# Detect Maya version from Devkit
if(EXISTS "${MAYA_DEVKIT_LOCATION}/include/maya/MTypes.h")
    # Tease the MAYA_API_VERSION numbers from the lib headers
    file(STRINGS ${MAYA_DEVKIT_LOCATION}/include/maya/MTypes.h TMP REGEX "#define MAYA_API_VERSION.*$")
    string(REGEX MATCHALL "[0-9]+" MAYA_API_VERSION ${TMP})
    string(SUBSTRING ${MAYA_API_VERSION} 0 4 MayaVersion)
else()
    message(FATAL_ERROR "Unable to determine the Maya version from ${MAYA_DEVKIT_LOCATION}")
endif()

# Detect Engine version
set(UE_ENGINE_DIR ${CMAKE_SOURCE_DIR}/../..)
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

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(BuildTarget Debug)
else()
    set(BuildTarget Development)
endif()

set(MAYA_MODULE_PATH ${CMAKE_SOURCE_DIR}/Binaries/${Platform}/Maya)
set(CMAKE_BINARY_DIR ${MAYA_MODULE_PATH}/${MayaVersion})
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})

message(STATUS "Using MAYA_LOCATION        = ${MAYA_LOCATION}")
message(STATUS "Using MAYA_DEVKIT_LOCATION = ${MAYA_DEVKIT_LOCATION}")
message(STATUS "Using MAYA_MODULE_PATH     = ${MAYA_MODULE_PATH}")
message(STATUS "Using MayaVersion          = ${MayaVersion}")
message(STATUS "Using UEVersion            = ${UEVersion}")

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_target(
    MayaUnrealLiveLinkPlugin ALL
    COMMAND ${CMAKE_COMMAND} -E env MAYA_${MayaDir}_DIR_${MayaVersion}=${MAYA_DEVKIT_LOCATION} "${CMAKE_SOURCE_DIR}/../../Build/BatchFiles/RunUAT${ScriptExtension}" BuildGraph -Script=Engine/Restricted/NotForLicensees/Source/Programs/MayaUnrealLiveLinkPlugin/BuildMayaUnrealLiveLinkPlugin.xml -Target=\"Stage Maya Plugin Module\" -set:MayaVersion=${MayaVersion} -set:MayaPlatform=${Platform} -set:BuildTarget=${BuildTarget} -NoXGE
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building MayaLive plugin"
  )

# Create .env file for setting environment variables for Python unit tests
file(WRITE ${CMAKE_SOURCE_DIR}/.env "UNITTEST_UNREAL_VERSION=${UEVersion}\nMAYA_VERSION=${MayaVersion}")
