cmake_minimum_required(VERSION 3.28)
project(RayneGlTF)

include(../../CMake/LibraryTarget.cmake)

set(BUILD_SHARED_LIBS OFF)
set(TINYGLTF_HEADER_ONLY OFF)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF)
set(TINYGLTF_BUILD_GL_EXAMPLES OFF)
set(TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF)

rayne_fetch_content(tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
        GIT_SHALLOW ON
        GIT_PROGRESS ON)
FetchContent_GetProperties(tinygltf SOURCE_DIR TINYGLTF_SOURCE_DIR)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNglTFInit.cpp
        RNglTFAssetLoader.cpp)

set(HEADERS
        RNglTF.h)

set(PRIVATE_HEADERS
        RNglTFAssetLoader.h)

set(DEFINES
        RN_BUILD_GLTF)

set(VENDOR_INCLUDES
        ${TINYGLTF_SOURCE_DIR})

rayne_add_library(RayneGlTF "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_LIBRARIES tinygltf PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneGlTF)

target_include_directories(RayneGlTF SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_compile_definitions(RayneGlTF PRIVATE "${DEFINES}")
