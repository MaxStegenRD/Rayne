cmake_minimum_required(VERSION 3.21)
project(RayneFlexUI)

include(../../CMake/LibraryTarget.cmake)
include(FetchContent)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNFlexInit.cpp
        RNFlexView.cpp
        RNFlexNode.cpp
        RNFlexLayout.cpp
        RNFlexText.cpp
        RNFlexImage.cpp
        RNFlexButton.cpp)

set(HEADERS
        RNFlexStyle.h
        RNFlexMeasure.h
        RNFlexNode.h
        RNFlexLayout.h
        RNFlexText.h
        RNFlexImage.h
        RNFlexButton.h
        RNFlexHelpers.h
        RNFlexView.h
        RNFlexConfig.h
        RNFlexUI.h)

set(DEFINES "RN_BUILD_FLEXUI")
set(RAYNE_LIBRARIES Rayne RayneUI)
set(INCLUDE_DIRECTORIES ${Rayne_BINARY_DIR}/include)

rayne_add_library(RayneFlexUI "${SOURCES}" "${HEADERS}" "${RAYNE_LIBRARIES}" "${VERSION_STRING}" "${VERSION_ABI}")
rayne_set_module_output_directory(RayneFlexUI)

if(NOT TARGET RayneUI)
    message(FATAL_ERROR "RayneFlexUI requires RayneUI. Enable RN_BUILD_UI_MODULE.")
endif()

FetchContent_Declare(Yoga
    GIT_REPOSITORY https://github.com/facebook/yoga.git
    GIT_TAG v3.2.1
    GIT_SHALLOW ON
    GIT_PROGRESS ON
)
FetchContent_MakeAvailable(Yoga)

target_link_libraries(RayneFlexUI PUBLIC yogacore)

if(DEFINED Yoga_SOURCE_DIR AND EXISTS "${Yoga_SOURCE_DIR}")
    target_include_directories(RayneFlexUI SYSTEM PUBLIC "${Yoga_SOURCE_DIR}")
endif()

target_include_directories(RayneFlexUI SYSTEM PRIVATE "${INCLUDE_DIRECTORIES}")
target_compile_definitions(RayneFlexUI PRIVATE "${DEFINES}")

target_include_directories(RayneFlexUI SYSTEM PUBLIC "${RayneUI_BINARY_DIR}/include" PUBLIC "${CMAKE_BINARY_DIR}/Modules/UI/include")

rayne_install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../../Build/RayneFlexUI/. DESTINATION lib/Rayne/RayneFlexUI)
