cmake_minimum_required(VERSION 3.28)
project(RayneSteamAudio)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)

set(BUILD_STATIC_LIBS ON)
set(BUILD_DYNAMIC_LIBS OFF)
set(BUILD_EXAMPLE_PROGRAMS OFF)
set(BUILD_TESTS OFF)
set(ENABLE_JACK ON)
set(ENABLE_PULSEAUDIO ON)
set(ENABLE_ALSA ON)
set(ENABLE_COREAUDIO ON)
set(ENABLE_WASAPI ON)

rayne_fetch_content(libsoundio
        GIT_REPOSITORY https://github.com/andrewrk/libsoundio.git
        GIT_TAG refs/tags/2.0.0
        GIT_SHALLOW ON
        GIT_PROGRESS ON)
FetchContent_GetProperties(libsoundio SOURCE_DIR LIBSOUNDIO_SOURCE_DIR)

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
if(APPLE OR WIN32 OR ANDROID)
        add_library(SteamAudioPhonon SHARED IMPORTED)
endif()
if(APPLE)
        find_package(CoreAudio)
        find_library(AUDIOUNIT_LIBRARY NAMES AudioUnit)
        set(STEAM_AUDIO_LIBRARIES ${COREAUDIO_LIB} ${AUDIOUNIT_LIBRARY})
        set_property(TARGET SteamAudioPhonon PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/lib/OSX/libphonon.dylib)
elseif(WIN32)
        set_property(TARGET SteamAudioPhonon PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/bin/Windows/x64/phonon.dll)
        set_property(TARGET SteamAudioPhonon PROPERTY IMPORTED_IMPLIB ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/lib/Windows/x64/phonon.lib)
elseif(ANDROID)
        set_property(TARGET SteamAudioPhonon PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/lib/Android/libphonon.so)
endif()

set(VENDOR_INCLUDES
        "${LIBSOUNDIO_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/steamaudio/include")

rayne_add_library(RayneSteamAudio "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_LIBRARIES libsoundio_static PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneSteamAudio)

target_include_directories(RayneSteamAudio SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_link_libraries(RayneSteamAudio PRIVATE ${STEAM_AUDIO_LIBRARIES})
target_compile_definitions(RayneSteamAudio PRIVATE "${DEFINES}")

if(APPLE OR WIN32 OR ANDROID)
        rayne_add_runtime_dependency(RayneSteamAudio SteamAudioPhonon)
endif()
