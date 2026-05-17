cmake_minimum_required(VERSION 3.28)
project(RayneRecast)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)

FetchContent_Declare(recast
        GIT_REPOSITORY https://github.com/recastnavigation/recastnavigation.git
        GIT_TAG origin/main
        GIT_SHALLOW ON
        GIT_PROGRESS ON)
FetchContent_GetProperties(recast)
if(NOT recast_POPULATED)
        FetchContent_Populate(recast)
endif()
FetchContent_GetProperties(recast SOURCE_DIR RECAST_SOURCE_DIR)


set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNRecastInit.cpp
        RNRecastWorld.cpp
        RNRecastMesh.cpp
        RNRecastAgent.cpp

        ${RECAST_SOURCE_DIR}/Recast/Source/Recast.cpp
        ${RECAST_SOURCE_DIR}/Recast/Source/RecastAlloc.cpp
        ${RECAST_SOURCE_DIR}/Recast/Source/RecastArea.cpp
        ${RECAST_SOURCE_DIR}/Recast/Source/RecastAssert.cpp
        ${RECAST_SOURCE_DIR}/Recast/Source/RecastContour.cpp
        ${RECAST_SOURCE_DIR}/Recast/Source/RecastFilter.cpp
        ${RECAST_SOURCE_DIR}/Recast/Source/RecastLayers.cpp
        ${RECAST_SOURCE_DIR}/Recast/Source/RecastMesh.cpp
        ${RECAST_SOURCE_DIR}/Recast/Source/RecastMeshDetail.cpp
        ${RECAST_SOURCE_DIR}/Recast/Source/RecastRasterization.cpp
        ${RECAST_SOURCE_DIR}/Recast/Source/RecastRegion.cpp

        ${RECAST_SOURCE_DIR}/Detour/Source/DetourAlloc.cpp
        ${RECAST_SOURCE_DIR}/Detour/Source/DetourAssert.cpp
        ${RECAST_SOURCE_DIR}/Detour/Source/DetourCommon.cpp
        ${RECAST_SOURCE_DIR}/Detour/Source/DetourNavMesh.cpp
        ${RECAST_SOURCE_DIR}/Detour/Source/DetourNavMeshBuilder.cpp
        ${RECAST_SOURCE_DIR}/Detour/Source/DetourNavMeshQuery.cpp
        ${RECAST_SOURCE_DIR}/Detour/Source/DetourNode.cpp

        ${RECAST_SOURCE_DIR}/DetourCrowd/Source/DetourCrowd.cpp
        ${RECAST_SOURCE_DIR}/DetourCrowd/Source/DetourLocalBoundary.cpp
        ${RECAST_SOURCE_DIR}/DetourCrowd/Source/DetourObstacleAvoidance.cpp
        ${RECAST_SOURCE_DIR}/DetourCrowd/Source/DetourPathCorridor.cpp
        ${RECAST_SOURCE_DIR}/DetourCrowd/Source/DetourPathQueue.cpp
        ${RECAST_SOURCE_DIR}/DetourCrowd/Source/DetourProximityGrid.cpp

        ${RECAST_SOURCE_DIR}/DebugUtils/Source/RecastDump.cpp)

set(HEADERS
	RNRecast.h
        RNRecastWorld.h
        RNRecastMesh.h
        RNRecastAgent.h)

set(DEFINES RN_BUILD_RECAST)

set(VENDOR_INCLUDES
        "${RECAST_SOURCE_DIR}/Recast/Include"
        "${RECAST_SOURCE_DIR}/Detour/Include"
        "${RECAST_SOURCE_DIR}/DetourCrowd/Include"
        "${RECAST_SOURCE_DIR}/DebugUtils/Include")

rayne_add_library(RayneRecast "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne)
rayne_set_module_output_directory(RayneRecast)

target_include_directories(RayneRecast SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_compile_definitions(RayneRecast PRIVATE "${DEFINES}")
