cmake_minimum_required(VERSION 3.23)
project(RayneOpenXR)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)
include(ExternalProject)

ExternalProject_Add(OpenXRSDK
	GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
	GIT_TAG refs/tags/release-1.1.58
	GIT_SHALLOW ON
	GIT_PROGRESS ON
	UPDATE_COMMAND ""
	CMAKE_CACHE_ARGS
		"-DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}"
		"-DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH:BOOL=False"
		"-DCMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}"
		"-DANDROID_NATIVE_API_LEVEL:STRING=${ANDROID_NATIVE_API_LEVEL}"
		"-DANDROID_ABI:STRING=${ANDROID_ABI}"
		"-DANDROID_NDK:STRING=${ANDROID_NDK}"
		"-DANDROID_STL:STRING=${ANDROID_STL}"
		"-DCMAKE_SYSTEM_VERSION:STRING=${CMAKE_SYSTEM_VERSION}"
		"-DDYNAMIC_LOADER:BOOL=OFF"
	BUILD_BYPRODUCTS
		"${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/Debug/openxr_loaderd.lib"
		"${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/RelWithDebInfo/openxr_loader.lib"
		"${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/Release/openxr_loader.lib"

		"${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/Debug/libopenxr_loader.a"
		"${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/RelWithDebInfo/libopenxr_loader.a"
		"${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/Release/libopenxr_loader.a"

		"${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/libopenxr_loader.a"
	INSTALL_COMMAND "")

add_library(OpenXRLoader STATIC IMPORTED)

if(APPLE)
	set_target_properties(OpenXRLoader PROPERTIES
		IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
		IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/Debug/libopenxr_loader.a"
		IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/Release/libopenxr_loader.a"
		IMPORTED_LOCATION_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/RelWithDebInfo/libopenxr_loader.a")
elseif(ANDROID)
	set_target_properties(OpenXRLoader PROPERTIES
		IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/libopenxr_loader.a")
else()
	set_target_properties(OpenXRLoader PROPERTIES
		IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
		IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/Debug/openxr_loaderd.lib"
		IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/Release/openxr_loader.lib"
		IMPORTED_LOCATION_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}/OpenXRSDK-prefix/src/OpenXRSDK-build/src/loader/RelWithDebInfo/openxr_loader.lib")
endif()

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
	set(SOURCES ${SOURCES} RNOpenXRVulkanSwapChain.cpp)
	list(APPEND PRIVATE_HEADERS RNOpenXRVulkanSwapChain.h)
endif()

if(APPLE AND ${RN_BUILD_METAL_MODULE})
	set(SOURCES ${SOURCES} RNOpenXRMetalSwapChain.cpp)
	list(APPEND PRIVATE_HEADERS RNOpenXRMetalSwapChain.h)
endif()

ExternalProject_Get_property(OpenXRSDK SOURCE_DIR)
set(OpenXRSDK_SOURCE_DIR ${SOURCE_DIR})

set(RAYNE_LIBRARIES Rayne RayneVR)

rayne_add_library(RayneOpenXR "${SOURCES}" "${HEADERS}" "${RAYNE_LIBRARIES}" "${VERSION_STRING}" "${VERSION_ABI}" PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneOpenXR)
add_dependencies(RayneOpenXR OpenXRSDK)

target_link_libraries(RayneOpenXR PRIVATE OpenXRLoader)

if(NOT APPLE AND ${RN_BUILD_VULKAN_MODULE})
	target_link_libraries(RayneOpenXR PUBLIC RayneVulkan)
	list(APPEND PRIVATE_DEFINES XR_USE_GRAPHICS_API_VULKAN)
endif()

if(APPLE AND ${RN_BUILD_METAL_MODULE})
	target_link_libraries(RayneOpenXR PUBLIC RayneMetal)
	list(APPEND PRIVATE_DEFINES XR_USE_GRAPHICS_API_METAL)
	target_compile_options(RayneOpenXR PRIVATE "-xobjective-c++")
endif()

target_include_directories(RayneOpenXR SYSTEM PRIVATE ${OpenXRSDK_SOURCE_DIR}/include ${OpenXRSDK_SOURCE_DIR}/src/common)
target_compile_definitions(RayneOpenXR PRIVATE "${PRIVATE_DEFINES}")

rayne_install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../../Build/RayneOpenXR/. DESTINATION lib/Rayne/RayneOpenXR)
