cmake_minimum_required(VERSION 3.23)
project(RayneVR)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
    RNVRWindow.cpp
    RNVRCompositorLayer.cpp
    RNVRCamera.cpp)

set(HEADERS
	RNVR.h
	RNVRApplication.h
	RNVRApplicationImpl.h
    RNVRWindow.h
    RNVRCompositorLayer.h
    RNVRStubWindow.h
    RNVRCamera.h
    RNVRTrackingState.h)

set(DEFINES RN_BUILD_VR)

set(PRIVATE_LIBRARIES)

if(APPLE)
	if(${RN_BUILD_METAL_MODULE})
		list(APPEND PRIVATE_LIBRARIES RayneMetal)
	endif()
elseif(ANDROID OR UNIX)
	if(${RN_BUILD_VULKAN_MODULE})
		list(APPEND PRIVATE_LIBRARIES RayneVulkan)
	endif()
else(WIN32)
	if(${RN_BUILD_VULKAN_MODULE})
		list(APPEND PRIVATE_LIBRARIES RayneVulkan)
	endif()
endif()

rayne_add_library(RayneVR "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_LIBRARIES ${PRIVATE_LIBRARIES})
rayne_set_module_output_directory(RayneVR)

target_compile_definitions(RayneVR PRIVATE "${DEFINES}")

rayne_install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../../Build/RayneVR/. DESTINATION lib/Rayne/RayneVR)
