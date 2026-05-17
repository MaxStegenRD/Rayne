cmake_minimum_required(VERSION 3.28)
project(RayneSteamAudio)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)
include(ExternalProject)

ExternalProject_Add(libsoundio
        GIT_REPOSITORY https://github.com/andrewrk/libsoundio.git
        GIT_TAG refs/tags/2.0.0
        GIT_SHALLOW ON
        GIT_PROGRESS ON
        UPDATE_COMMAND ""
        CMAKE_CACHE_ARGS
                "-DBUILD_STATIC_LIBS:BOOL=True"
                "-DBUILD_DYNAMIC_LIBS:BOOL=False"
                "-DBUILD_EXAMPLE_PROGRAMS:BOOL=False"
                "-DBUILD_TESTS:BOOL=False"
                "-DENABLE_JACK:BOOL=True"
                "-DENABLE_PULSEAUDIO:BOOL=True"
                "-DENABLE_ALSA:BOOL=True"
                "-DENABLE_COREAUDIO:BOOL=True"
                "-DENABLE_WASAPI:BOOL=True"
                "-DCMAKE_SYSTEM_VERSION:STRING=${CMAKE_SYSTEM_VERSION}"
        BUILD_BYPRODUCTS
                "${CMAKE_CURRENT_BINARY_DIR}/libsoundio-prefix/src/libsoundio-build/Debug/libsoundio.a"
                "${CMAKE_CURRENT_BINARY_DIR}/libsoundio-prefix/src/libsoundio-build/Release/libsoundio.a"
        INSTALL_COMMAND "")


set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNSteamAudioInit.cpp
        RNSteamAudioWorld.cpp
        RNSteamAudioSampler.cpp
        RNSteamAudioSource.cpp
        RNSteamAudioPlayer.cpp
        RNSteamAudioInternals.cpp)

set(HEADERS
	RNSteamAudio.h
        RNSteamAudioWorld.h
        RNSteamAudioSampler.h
        RNSteamAudioSource.h
        RNSteamAudioPlayer.h)

set(PRIVATE_HEADERS
        RNSteamAudioInternals.h)

set(DEFINES RN_BUILD_STEAMAUDIO SOUNDIO_STATIC_LIBRARY)
set(STEAM_AUDIO_LIBRARIES)
if(APPLE)
        find_package(CoreAudio)
        find_library(AUDIOUNIT_LIBRARY NAMES AudioUnit)
        set(STEAM_AUDIO_LIBRARIES ${COREAUDIO_LIB} ${AUDIOUNIT_LIBRARY} ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/lib/OSX/libphonon.dylib)
elseif(WIN32)
        set(STEAM_AUDIO_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/lib/Windows/x64/phonon.lib)
elseif(ANDROID)
        set(STEAM_AUDIO_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/lib/Android/libphonon.so)
endif()

set(VENDOR_INCLUDES
        "${CMAKE_CURRENT_BINARY_DIR}/libsoundio-prefix/src/libsoundio"
        "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/include")

rayne_add_library(RayneSteamAudio "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneSteamAudio)

target_include_directories(RayneSteamAudio SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_link_libraries(RayneSteamAudio PRIVATE ${STEAM_AUDIO_LIBRARIES})
target_compile_definitions(RayneSteamAudio PRIVATE "${DEFINES}")

add_dependencies(RayneSteamAudio libsoundio)
target_link_libraries(RayneSteamAudio PRIVATE
        debug "${CMAKE_CURRENT_BINARY_DIR}/libsoundio-prefix/src/libsoundio-build/Debug/libsoundio.a"
        optimized "${CMAKE_CURRENT_BINARY_DIR}/libsoundio-prefix/src/libsoundio-build/Release/libsoundio.a")

if(APPLE)
        rayne_add_runtime_dependency(RayneSteamAudio "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/lib/OSX/libphonon.dylib")
elseif(WIN32)
        rayne_add_runtime_dependency(RayneSteamAudio "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/bin/Windows/x64/phonon.dll")
elseif(ANDROID)
        rayne_add_runtime_dependency(RayneSteamAudio "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/lib/Android/libphonon.so")
endif()
