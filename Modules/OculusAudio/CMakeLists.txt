cmake_minimum_required(VERSION 3.28)
project(RayneOculusAudio)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)

if(ANDROID)
        rayne_fetch_content(oboe
                GIT_REPOSITORY https://github.com/google/oboe.git
                GIT_TAG refs/tags/1.4.2
                GIT_SHALLOW ON
                GIT_PROGRESS ON)
        FetchContent_GetProperties(oboe SOURCE_DIR OBOE_SOURCE_DIR)
else()
        set(BUILD_SHARED_LIBS OFF)
        set(RTAUDIO_BUILD_SHARED_LIBS OFF)
        set(RTAUDIO_BUILD_STATIC_LIBS ON)
        rayne_fetch_content(rtaudio
                GIT_REPOSITORY https://github.com/thestk/rtaudio.git
                GIT_TAG refs/tags/5.1.0
                GIT_SHALLOW ON
                GIT_PROGRESS ON)
        FetchContent_GetProperties(rtaudio SOURCE_DIR RTAUDIO_SOURCE_DIR)
endif()

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNOculusAudioInit.cpp
        RNOculusAudioWorld.cpp
        RNOculusAudioSystem.cpp
        RNOculusAudioSampler.cpp
        RNOculusAudioSource.cpp
        RNOculusAudioPlayer.cpp
        RNOculusAudioInternals.cpp)

set(HEADERS
	RNOculusAudio.h
        RNOculusAudioWorld.h
        RNOculusAudioSystem.h
        RNOculusAudioSampler.h
        RNOculusAudioSource.h
        RNOculusAudioPlayer.h)

set(PRIVATE_HEADERS
        RNOculusAudioInternals.h)

set(DEFINES RN_BUILD_OCULUS_AUDIO)

set(OCULUS_AUDIO_LIBRARIES)

if(APPLE)
        find_library(COREAUDIO_LIB CoreAudio)
        find_library(COREFOUNDATION_LIB CoreFoundation)
        set(OCULUS_AUDIO_LIBRARIES ${COREAUDIO_LIB} ${COREFOUNDATION_LIB} ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/OculusAudio/Lib/macub/OVRAudio.framework/Versions/A/OVRAudio)
        set(VENDOR_INCLUDES "${RTAUDIO_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/OculusAudio/Include")
elseif(WIN32)
        set(OCULUS_AUDIO_LIBRARIES winmm ole32 ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/OculusAudio/Lib/x64/ovraudio64.lib)
        set(VENDOR_INCLUDES "${RTAUDIO_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/OculusAudio/Include")
elseif(ANDROID)
        set(OCULUS_AUDIO_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/OculusAudio/Lib/Android/arm64-v8a/libovraudiostatic64.a)
        set(VENDOR_INCLUDES "${OBOE_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/OculusAudio/Include")
endif()

rayne_add_library(RayneOculusAudio "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneOculusAudio)

target_include_directories(RayneOculusAudio SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_link_libraries(RayneOculusAudio PRIVATE ${OCULUS_AUDIO_LIBRARIES})
target_compile_definitions(RayneOculusAudio PRIVATE "${DEFINES}")

if(ANDROID)
        target_link_libraries(RayneOculusAudio PRIVATE oboe)
else()
        target_link_libraries(RayneOculusAudio PRIVATE rtaudio)
endif()

if(APPLE)
        rayne_add_runtime_dependency(RayneOculusAudio "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/OculusAudio/Lib/macub/OVRAudio.framework")
endif()
if(ANDROID)
        rayne_add_runtime_dependency(RayneOculusAudio "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/OculusAudio/Lib/Android/arm64-v8a/libovraudio64.so")
endif()
