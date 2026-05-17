cmake_minimum_required(VERSION 3.28)
project(RayneBHaptics)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)
include(ExternalProject)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNBHapticsInit.cpp
        RNBHapticsTypes.cpp
        RNBHapticsManager.cpp
        RNBHapticsAndroidWrapper.cpp)

set(HEADERS
        RNBHaptics.h
        RNBHapticsTypes.h
        RNBHapticsManager.h)

set(PRIVATE_HEADERS
        RNBHapticsAndroidWrapper.h)

set(DEFINES RN_BUILD_BHAPTICS)

set(COMPILE_OPTIONS "")

set(VENDOR_INCLUDES)

if(WIN32)
        list(APPEND VENDOR_INCLUDES
                "${CMAKE_CURRENT_BINARY_DIR}/bhaptics-prefix/src/bhaptics/include/shared"
                "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/BhapticsCPP")
endif()

rayne_add_library(RayneBHaptics "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneBHaptics)

target_include_directories(RayneBHaptics SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_compile_definitions(RayneBHaptics PRIVATE "${DEFINES}")
target_compile_options(RayneBHaptics PRIVATE ${COMPILE_OPTIONS})

if(WIN32)
        add_library(BHapticsLib SHARED IMPORTED)

        set_property(TARGET BHapticsLib PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/BhapticsCPP/Win64/bhaptics_library.dll)
        set_property(TARGET BHapticsLib PROPERTY IMPORTED_IMPLIB ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/BhapticsCPP/Win64/bhaptics_library.lib)

        rayne_add_runtime_dependency(RayneBHaptics BHapticsLib)
endif()
