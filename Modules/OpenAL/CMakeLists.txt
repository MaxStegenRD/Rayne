cmake_minimum_required(VERSION 3.28)
project(RayneOpenAL)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)

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
set(VENDOR_INCLUDES)

if(APPLE)
        list(APPEND COMPILE_OPTIONS -xobjective-c++)
        find_library(AVFOUNDATION_LIBRARY AVFoundation)
        find_library(OBJC_LIBRARY objc)
        list(APPEND PRIVATE_LIBRARIES ${AVFOUNDATION_LIBRARY} ${OBJC_LIBRARY})
endif()
if(IOS OR VISIONOS)
        find_library(AUDIOTOOLBOX_LIB AudioToolbox)
        list(APPEND PRIVATE_LIBRARIES ${AUDIOTOOLBOX_LIB})
endif()

set(OPENAL_VERSION_TAG refs/tags/1.24.3)
set(OPENAL_RUNTIME_DEPENDENCY_OPTIONS)

if(IOS OR VISIONOS)
        set(LIBTYPE STATIC)
else()
        set(LIBTYPE SHARED)
endif()
if(ANDROID)
        set(ALSOFT_REQUIRE_OPENSL ON)
        set(OPENAL_ANDROID_RUNTIME_FILE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}openal${CMAKE_SHARED_LIBRARY_SUFFIX}")
        add_library(RayneOpenALRuntimeLink SHARED IMPORTED)
        set_property(TARGET RayneOpenALRuntimeLink PROPERTY IMPORTED_LOCATION "${OPENAL_ANDROID_RUNTIME_FILE}")
        list(APPEND PRIVATE_LIBRARIES RayneOpenALRuntimeLink)
else()
        set(ALSOFT_REQUIRE_OPENSL OFF)
endif()
if(APPLE AND NOT IOS AND NOT VISIONOS)
        set(OPENAL_VERSION_TAG refs/tags/1.23.1)
        set(OPENAL_RUNTIME_DEPENDENCY_OPTIONS OUTPUT_NAME libopenal.1.dylib)
endif()

set(ALSOFT_UTILS OFF)
set(ALSOFT_EXAMPLES OFF)
set(ALSOFT_TESTS OFF)
set(ALSOFT_NO_CONFIG_UTIL ON)
set(ALSOFT_HRTF_DEFS OFF)
set(ALSOFT_AMBDEC_PRESETS OFF)
set(ALSOFT_INSTALL OFF)
set(ALSOFT_INSTALL_CONFIG OFF)
set(ALSOFT_INSTALL_HRTF_DATA OFF)
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF)
set(ALSOFT_INSTALL_EXAMPLES OFF)
set(ALSOFT_INSTALL_UTILS OFF)
set(ALSOFT_EMBED_HRTF_DATA ON)
set(ALSOFT_CPUEXT_NEON OFF)

if(MSVC)
        set(OPENAL_DEBUG_OPTIMIZATION_FLAGS /O2 /Ob2 /RTC-)
else()
        set(OPENAL_DEBUG_OPTIMIZATION_FLAGS -O2)
endif()

rayne_fetch_content(openal
        GIT_REPOSITORY https://github.com/kcat/openal-soft.git
        GIT_TAG ${OPENAL_VERSION_TAG}
        GIT_SHALLOW ON
        GIT_PROGRESS ON)
FetchContent_GetProperties(openal SOURCE_DIR OPENAL_SOURCE_DIR BINARY_DIR OPENAL_BINARY_DIR)

foreach(OPENAL_TARGET OpenAL common soft_oal)
        if(TARGET ${OPENAL_TARGET})
                foreach(OPENAL_DEBUG_OPTIMIZATION_FLAG IN LISTS OPENAL_DEBUG_OPTIMIZATION_FLAGS)
                        target_compile_options(${OPENAL_TARGET} PRIVATE $<$<CONFIG:Debug>:${OPENAL_DEBUG_OPTIMIZATION_FLAG}>)
                endforeach()
        endif()
endforeach()

list(APPEND VENDOR_INCLUDES
        "${OPENAL_SOURCE_DIR}/include"
        "${OPENAL_BINARY_DIR}")

rayne_add_library(RayneOpenAL "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_LIBRARIES ${PRIVATE_LIBRARIES})
rayne_set_module_output_directory(RayneOpenAL)

target_include_directories(RayneOpenAL SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_compile_definitions(RayneOpenAL PRIVATE "${DEFINES}")
target_compile_options(RayneOpenAL PRIVATE ${COMPILE_OPTIONS})
rayne_add_runtime_dependency(RayneOpenAL OpenAL ${OPENAL_RUNTIME_DEPENDENCY_OPTIONS})
