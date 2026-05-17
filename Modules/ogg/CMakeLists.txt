cmake_minimum_required(VERSION 3.28)
project(RayneOgg)

include(../../CMake/LibraryTarget.cmake)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNOggInit.cpp
        RNOggAssetLoader.cpp)

set(HEADERS
        RNOggAssetLoader.h
        RNOgg.h)

set(DEFINES
        RN_BUILD_OGG)

rayne_add_library(RayneOgg "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne)
rayne_set_module_output_directory(RayneOgg)

target_include_directories(RayneOgg SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Vendor)
target_compile_definitions(RayneOgg PRIVATE "${DEFINES}")
