cmake_minimum_required(VERSION 3.28)
project(RayneENet)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNENetInit.cpp
        RNENetWorld.cpp
        RNENetHost.cpp
        RNENetClient.cpp
        RNENetServer.cpp
        RNENetInternals.cpp)

set(HEADERS
	RNENet.h
        RNENetWorld.h
        RNENetHost.h
        RNENetClient.h
        RNENetServer.h)

set(PRIVATE_HEADERS
        RNENetInternals.h)

set(DEFINES RN_BUILD_ENET)
set(PRIVATE_LIBRARIES)

if(WIN32)
        list(APPEND PRIVATE_LIBRARIES ws2_32.lib winmm.lib)
endif()

set(BUILD_SHARED_LIBS OFF)
rayne_fetch_content(enet
        GIT_REPOSITORY https://github.com/lsalzman/enet.git
        GIT_TAG origin/master
        GIT_SHALLOW ON
        GIT_PROGRESS ON)
FetchContent_GetProperties(enet SOURCE_DIR ENET_SOURCE_DIR)

set(VENDOR_INCLUDES
        "${ENET_SOURCE_DIR}/include")

rayne_add_library(RayneENet "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_LIBRARIES ${PRIVATE_LIBRARIES} enet PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneENet)

target_include_directories(RayneENet SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_compile_definitions(RayneENet PRIVATE "${DEFINES}")
