cmake_minimum_required(VERSION 3.23)
project(RayneEOS)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)
include(ExternalProject)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNEOSInit.cpp
        RNEOSWorld.cpp
        RNEOSLobbyManager.cpp
        RNEOSHost.cpp
        RNEOSClient.cpp
        RNEOSP2PClient.cpp
        RNEOSServer.cpp)

set(HEADERS
        RNEOS.h
        RNEOSWorld.h
        RNEOSLobbyManager.h
        RNEOSHost.h
        RNEOSClient.h
        RNEOSP2PClient.h
        RNEOSServer.h)

set(DEFINES RN_BUILD_EOS)

set(EOSSDK_INCLUDE_DIR)
add_library(EOSSDK SHARED IMPORTED)
if(ANDROID)
        set(EOSSDK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK-Android/SDK/Include/)
        set_property(TARGET EOSSDK PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK-Android/SDK/Lib/libEOSSDK.so)
elseif(IOS OR VISIONOS)
        set(EOSSDK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK-IOS/SDK/Bin/IOS/EOSSDK.framework/Headers)
        set_property(TARGET EOSSDK PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK-IOS/SDK/Bin/IOS/EOSSDK.framework)
elseif(WIN32)
        set(EOSSDK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK/SDK/Include/)
        set_property(TARGET EOSSDK PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK/SDK/Bin/EOSSDK-Win64-Shipping.dll)
        set_property(TARGET EOSSDK PROPERTY IMPORTED_IMPLIB ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK/SDK/Lib/EOSSDK-Win64-Shipping.lib)
        set(EOSSDK_DYNAMIC_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK/SDK/Bin/EOSSDK-Win64-Shipping.dll)
else()
        set(EOSSDK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK/SDK/Include/)
        set_property(TARGET EOSSDK PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK/SDK/Bin/libEOSSDK-Mac-Shipping.dylib)
        set(EOSSDK_DYNAMIC_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK/SDK/Bin/libEOSSDK-Mac-Shipping.dylib)
endif()

set(RAYNE_LIBRARIES Rayne)


rayne_add_library(RayneEOS "${SOURCES}" "${HEADERS}" "${RAYNE_LIBRARIES}" "${VERSION_STRING}" "${VERSION_ABI}")
rayne_set_module_output_directory(RayneEOS)

target_link_libraries(RayneEOS PRIVATE EOSSDK)
target_include_directories(RayneEOS SYSTEM PRIVATE ${EOSSDK_INCLUDE_DIR})
target_compile_definitions(RayneEOS PRIVATE "${DEFINES}")

if(ANDROID OR IOS OR VISIONOS)
        #Nothing to do!?
else()
        add_custom_command(TARGET RayneEOS POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${EOSSDK_DYNAMIC_LIBRARY}" "$<TARGET_FILE_DIR:RayneEOS>")

        if(WIN32)
                add_custom_command(TARGET RayneEOS POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/EOS-SDK/SDK/Bin/x64/xaudio2_9redist.dll" "$<TARGET_FILE_DIR:RayneEOS>")
        endif()
endif()

rayne_install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../../Build/RayneEOS/. DESTINATION lib/Rayne/RayneEOS)
