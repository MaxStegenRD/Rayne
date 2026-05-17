cmake_minimum_required(VERSION 3.28)
project(RayneAppleXR)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)
include(ExternalProject)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNAppleXRInit.cpp
        RNAppleXRWindow.cpp
        RNAppleXRSwapChain.cpp)

set(HEADERS
		RNAppleXR.h
        RNAppleXRWindow.h)

set(PRIVATE_HEADERS
        RNAppleXRSwapChain.h)

set(DEFINES RN_BUILD_APPLEXR APPLEXR_BUILD_STATIC)

find_library(COMPOSITORSERVICES_LIBRARY CompositorServices)
find_library(ARKIT_LIBRARY ARKit)
set(PRIVATE_LIBRARIES ${COMPOSITORSERVICES_LIBRARY} ${ARKIT_LIBRARY})

if(APPLE AND ${RN_BUILD_METAL_MODULE})
    set(SOURCES
        ${SOURCES}
        RNAppleXRMetalSwapChain.cpp)

    set(PRIVATE_HEADERS
        ${PRIVATE_HEADERS}
        RNAppleXRMetalSwapChain.h)

    list(APPEND PRIVATE_LIBRARIES RayneMetal)
endif()

rayne_add_library(RayneAppleXR "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne RayneVR PRIVATE_LIBRARIES ${PRIVATE_LIBRARIES} PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneAppleXR)

if(APPLE AND ${RN_BUILD_METAL_MODULE})
    target_compile_definitions(RayneAppleXR PRIVATE -DRN_APPLEXR_SUPPORTS_METAL)
    target_compile_options(RayneAppleXR PRIVATE "-xobjective-c++")
endif()

target_compile_definitions(RayneAppleXR PRIVATE "${DEFINES}")
