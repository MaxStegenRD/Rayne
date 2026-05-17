cmake_minimum_required(VERSION 3.23)
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

rayne_add_library(RayneFlexUI "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne RayneUI)
rayne_set_module_output_directory(RayneFlexUI)

FetchContent_Declare(Yoga
    GIT_REPOSITORY https://github.com/facebook/yoga.git
    GIT_TAG v3.2.1
    GIT_SHALLOW ON
    GIT_PROGRESS ON
)
FetchContent_MakeAvailable(Yoga)
set_property(DIRECTORY "${yoga_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)

target_link_libraries(RayneFlexUI PRIVATE yogacore)

target_compile_definitions(RayneFlexUI PRIVATE "${DEFINES}")

rayne_add_public_header_directory(RayneFlexUI
    BUILD_DIRECTORY "${yoga_SOURCE_DIR}"
    INSTALL_DIRECTORY "include/Rayne/ThirdParty/Yoga"
    INSTALL_SOURCE "${yoga_SOURCE_DIR}/yoga"
    INSTALL_OPTIONS FILES_MATCHING PATTERN "*.h")
