cmake_minimum_required(VERSION 3.23)
project(RayneOpenAL)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)
include(ExternalProject)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNOpenALInit.cpp
        RNOpenALWorld.cpp
        RNOpenALListener.cpp
        RNOpenALResourceAttachment.cpp
        RNOpenALSource.cpp)

set(HEADERS
	RNOpenAL.h
        RNOpenALWorld.h
        RNOpenALListener.h
        RNOpenALResourceAttachment.h
        RNOpenALSource.h)

set(DEFINES RN_BUILD_OPENAL AL_LIBTYPE_STATIC)

set(COMPILE_OPTIONS)
set(PRIVATE_LIBRARIES)

if(APPLE)
        list(APPEND COMPILE_OPTIONS -xobjective-c++)
        find_library(AVFOUNDATION_LIBRARY AVFoundation)
        list(APPEND PRIVATE_LIBRARIES ${AVFOUNDATION_LIBRARY})
endif()
if(IOS OR VISIONOS)
        find_library(AUDIOTOOLBOX_LIB AudioToolbox)
        list(APPEND PRIVATE_LIBRARIES ${AUDIOTOOLBOX_LIB})
endif()

set(VENDOR_INCLUDES
        "${CMAKE_CURRENT_BINARY_DIR}/openal-prefix/src/openal/include")

rayne_add_library(RayneOpenAL "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_LIBRARIES ${PRIVATE_LIBRARIES})
rayne_set_module_output_directory(RayneOpenAL)

target_include_directories(RayneOpenAL SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_compile_definitions(RayneOpenAL PRIVATE "${DEFINES}")
target_compile_options(RayneOpenAL PRIVATE ${COMPILE_OPTIONS})

set(OPENAL_BUILD_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/openal-prefix/src/openal-build")
set(OPENAL_VERSION_TAG refs/tags/1.24.3)
set(OPENAL_CMAKE_CACHE_ARGS
        "-DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH:BOOL=False"
        "-DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}"
        "-DCMAKE_SYSTEM_NAME:STRING=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_OSX_SYSROOT:STRING=${CMAKE_OSX_SYSROOT}"
        "-DALSOFT_UTILS:BOOL=False"
        "-DALSOFT_EXAMPLES:BOOL=False"
        "-DALSOFT_TESTS:BOOL=False"
        "-DALSOFT_CONFIG:BOOL=False"
        "-DALSOFT_HRTF_DEFS:BOOL=False"
        "-DALSOFT_AMBDEC_PRESETS:BOOL=False"
        "-DALSOFT_INSTALL:BOOL=False"
        "-DALSOFT_EMBED_HRTF_DATA:BOOL=True"
        "-DALSOFT_REQUIRE_OPENSL:BOOL=${ANDROID}"
        "-DALSOFT_CPUEXT_NEON:BOOL=False"
        "-DCMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}"
        "-DANDROID_NATIVE_API_LEVEL:STRING=${ANDROID_NATIVE_API_LEVEL}"
        "-DANDROID_ABI:STRING=${ANDROID_ABI}"
        "-DANDROID_NDK:STRING=${ANDROID_NDK}"
        "-DANDROID_STL:STRING=${ANDROID_STL}"
        "-DCMAKE_SYSTEM_VERSION:STRING=${CMAKE_SYSTEM_VERSION}")
set(OPENAL_BUILD_BYPRODUCTS
        "${OPENAL_BUILD_DIRECTORY}/Debug/libopenal.1.23.1.dylib"
        "${OPENAL_BUILD_DIRECTORY}/Release/libopenal.1.23.1.dylib"
        "${OPENAL_BUILD_DIRECTORY}/libopenal.so"
        "${OPENAL_BUILD_DIRECTORY}/Debug/OpenAL32.lib"
        "${OPENAL_BUILD_DIRECTORY}/Release/OpenAL32.lib")
set(OPENAL_RUNTIME_DEPENDENCY_OPTIONS)

if(IOS OR VISIONOS)
        list(APPEND OPENAL_CMAKE_CACHE_ARGS "-DLIBTYPE:STRING=STATIC")
        set(OPENAL_BUILD_BYPRODUCTS
                "${OPENAL_BUILD_DIRECTORY}/Debug-${APPLE_SDK_NAME}/libopenal.a"
                "${OPENAL_BUILD_DIRECTORY}/Release-${APPLE_SDK_NAME}/libopenal.a")
        set(OPENAL_IMPORTED_PROPERTIES
                IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
                IMPORTED_LOCATION_DEBUG "${OPENAL_BUILD_DIRECTORY}/Debug-${APPLE_SDK_NAME}/libopenal.a"
                IMPORTED_LOCATION_RELEASE "${OPENAL_BUILD_DIRECTORY}/Release-${APPLE_SDK_NAME}/libopenal.a"
                IMPORTED_LOCATION_RELWITHDEBINFO "${OPENAL_BUILD_DIRECTORY}/RelWithDebInfo-${APPLE_SDK_NAME}/libopenal.a")
elseif(WIN32)
        set(OPENAL_IMPORTED_PROPERTIES
                IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
                IMPORTED_LOCATION_DEBUG "${OPENAL_BUILD_DIRECTORY}/Debug/OpenAL32.dll"
                IMPORTED_IMPLIB_DEBUG "${OPENAL_BUILD_DIRECTORY}/Debug/OpenAL32.lib"
                IMPORTED_LOCATION_RELEASE "${OPENAL_BUILD_DIRECTORY}/Release/OpenAL32.dll"
                IMPORTED_IMPLIB_RELEASE "${OPENAL_BUILD_DIRECTORY}/Release/OpenAL32.lib"
                IMPORTED_LOCATION_RELWITHDEBINFO "${OPENAL_BUILD_DIRECTORY}/RelWithDebInfo/OpenAL32.dll"
                IMPORTED_IMPLIB_RELWITHDEBINFO "${OPENAL_BUILD_DIRECTORY}/RelWithDebInfo/OpenAL32.lib")
elseif(APPLE)
        set(OPENAL_VERSION_TAG refs/tags/1.23.1)
        set(OPENAL_RUNTIME_DEPENDENCY_OPTIONS OUTPUT_NAME libopenal.1.dylib)
        set(OPENAL_IMPORTED_PROPERTIES
                IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
                IMPORTED_LOCATION_DEBUG "${OPENAL_BUILD_DIRECTORY}/Debug/libopenal.1.23.1.dylib"
                IMPORTED_LOCATION_RELEASE "${OPENAL_BUILD_DIRECTORY}/Release/libopenal.1.23.1.dylib"
                IMPORTED_LOCATION_RELWITHDEBINFO "${OPENAL_BUILD_DIRECTORY}/RelWithDebInfo/libopenal.1.23.1.dylib")
else()
        set(OPENAL_IMPORTED_PROPERTIES
                IMPORTED_LOCATION "${OPENAL_BUILD_DIRECTORY}/libopenal.so")
endif()

ExternalProject_Add(openal
        GIT_REPOSITORY https://github.com/kcat/openal-soft.git
        GIT_TAG ${OPENAL_VERSION_TAG}
        GIT_SHALLOW ON
        GIT_PROGRESS ON
        UPDATE_COMMAND ""
        CMAKE_CACHE_ARGS ${OPENAL_CMAKE_CACHE_ARGS}
        BUILD_BYPRODUCTS ${OPENAL_BUILD_BYPRODUCTS}
        INSTALL_COMMAND "")

if(IOS OR VISIONOS)
        add_library(OpenALLib STATIC IMPORTED)
else()
        add_library(OpenALLib SHARED IMPORTED)
endif()
set_target_properties(OpenALLib PROPERTIES ${OPENAL_IMPORTED_PROPERTIES})

add_dependencies(OpenALLib openal)
rayne_add_runtime_dependency(RayneOpenAL OpenALLib ${OPENAL_RUNTIME_DEPENDENCY_OPTIONS})
