cmake_minimum_required(VERSION 3.28)
project(RayneOpenXR)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)

set(BUILD_TESTS OFF)
set(BUILD_API_LAYERS OFF)
set(BUILD_CONFORMANCE_TESTS OFF)
set(BUILD_LOADER ON)
set(DYNAMIC_LOADER OFF)

rayne_fetch_content(OpenXRSDK
	GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
	GIT_TAG refs/tags/release-1.1.58
	GIT_SHALLOW ON
	GIT_PROGRESS ON)
FetchContent_GetProperties(OpenXRSDK SOURCE_DIR OpenXRSDK_SOURCE_DIR)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
	RNOpenXRInit.cpp
	RNOpenXRInternals.cpp
	RNOpenXRFramePresentationState.cpp
	RNOpenXRSwapChain.cpp
	RNOpenXRWindow.cpp
	RNOpenXRCompositorLayer.cpp)

set(HEADERS
	RNOpenXR.h
	RNOpenXRWindow.h
	RNOpenXRCompositorLayer.h)

set(PRIVATE_HEADERS
	RNOpenXRInternals.h
	RNOpenXRFramePresentationState.h
	RNOpenXRSwapChain.h)

set(PRIVATE_DEFINES RN_BUILD_OPENXR)

if(ANDROID)
	list(APPEND PRIVATE_DEFINES XR_USE_PLATFORM_ANDROID)
elseif(WIN32)
	list(APPEND PRIVATE_DEFINES XR_USE_PLATFORM_WIN32)
elseif(UNIX AND NOT APPLE)
	list(APPEND PRIVATE_DEFINES XR_USE_PLATFORM_XLIB)
endif()

if(NOT APPLE AND ${RN_BUILD_VULKAN_MODULE})
	set(SOURCES ${SOURCES} RNOpenXRVulkanGraphicsProvider.cpp RNOpenXRVulkanSwapChain.cpp)
	list(APPEND PRIVATE_HEADERS RNOpenXRVulkanGraphicsProvider.h RNOpenXRVulkanSwapChain.h)
endif()

if(APPLE AND ${RN_BUILD_METAL_MODULE})
	set(SOURCES ${SOURCES} RNOpenXRMetalSwapChain.cpp)
	list(APPEND PRIVATE_HEADERS RNOpenXRMetalSwapChain.h)
endif()

rayne_add_library(RayneOpenXR "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne RayneVR PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneOpenXR)

target_link_libraries(RayneOpenXR PRIVATE openxr_loader)

if(ANDROID)
	target_link_libraries(RayneOpenXR PRIVATE android log)
endif()

if(NOT APPLE AND ${RN_BUILD_VULKAN_MODULE})
	target_link_libraries(RayneOpenXR PRIVATE RayneVulkan)
	list(APPEND PRIVATE_DEFINES XR_USE_GRAPHICS_API_VULKAN)
endif()

if(APPLE AND ${RN_BUILD_METAL_MODULE})
	target_link_libraries(RayneOpenXR PRIVATE RayneMetal)
	list(APPEND PRIVATE_DEFINES XR_USE_GRAPHICS_API_METAL)
	target_compile_options(RayneOpenXR PRIVATE "-xobjective-c++")
endif()

target_include_directories(RayneOpenXR SYSTEM PRIVATE ${OpenXRSDK_SOURCE_DIR}/include ${OpenXRSDK_SOURCE_DIR}/src/common)
target_compile_definitions(RayneOpenXR PRIVATE "${PRIVATE_DEFINES}")
