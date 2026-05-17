cmake_minimum_required(VERSION 3.28)
project(RayneAssimp)

include(../../CMake/LibraryTarget.cmake)

set(BUILD_SHARED_LIBS OFF)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_NO_EXPORT ON)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
set(ASSIMP_BUILD_ZLIB OFF)

rayne_fetch_content(assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG refs/tags/v5.0.1
        GIT_SHALLOW ON
        GIT_PROGRESS ON)
FetchContent_GetProperties(assimp SOURCE_DIR ASSIMP_SOURCE_DIR BINARY_DIR ASSIMP_BINARY_DIR)
target_compile_definitions(assimp PRIVATE NOUNCRYPT)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNAssimpInit.cpp
        RNAssimpAssetLoader.cpp)

set(HEADERS
        RNAssimp.h)

set(PRIVATE_HEADERS
        RNAssimpAssetLoader.h)

set(DEFINES
        RN_BUILD_ASSIMP)

set(VENDOR_INCLUDES
        ${ASSIMP_SOURCE_DIR}/include
        ${ASSIMP_BINARY_DIR}/include)

rayne_add_library(RayneAssimp "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_LIBRARIES assimp PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneAssimp)

target_include_directories(RayneAssimp SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_compile_definitions(RayneAssimp PRIVATE "${DEFINES}")
