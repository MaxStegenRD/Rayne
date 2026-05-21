cmake_minimum_required(VERSION 3.28)
project(RayneUI)

include(../../CMake/LibraryTarget.cmake)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNUIPath.cpp
        RNUIView.cpp
        RNUIAttributedString.cpp
        RNUILabel.cpp
        RNUIDebugTextBackend.cpp
        RNUIWindow.cpp
        RNUIServer.cpp
        RNUIInit.cpp
        RNUIEdgeInsets.h
        RNUIFontManager.cpp
        RNUIImageView.cpp
        RNUIButton.cpp
        RNUIScrollView.cpp
        RNUIGridView.cpp
        RNUIStackView.cpp
        RNUISlider.cpp)

set(RESOURCES
        Resources)

set(HEADERS
        RNUIPath.h
        RNUIView.h
        RNUIAttributedString.h
        RNUILabel.h
        RNUIDebugTextBackend.h
        RNUIWindow.h
        RNUIServer.h
        RNUIConfig.h
        RNUIEdgeInsets.h
        RNUIFontManager.h
        RNUIImageView.h
        RNUIButton.h
        RNUIScrollView.h
        RNUIGridView.h
        RNUIStackView.h
        RNUISlider.h
        RNUI.h)

set(DEFINES
        "RN_BUILD_UI"
        "SK_RELEASE")

rayne_fetch_content(ArteryFont
        GIT_REPOSITORY https://github.com/Chlumsky/artery-font-format.git
        GIT_TAG 888674220216d1d326c6f29cf89165b545279c1f)
rayne_fetch_content(Kalligraph
        GIT_REPOSITORY https://github.com/Slin/Kalligraph.git
        GIT_TAG origin/master
        GIT_SHALLOW ON
        GIT_PROGRESS ON)
FetchContent_GetProperties(ArteryFont SOURCE_DIR ARTERYFONT_SOURCE_DIR)

set(VENDOR_INCLUDES
        "${ARTERYFONT_SOURCE_DIR}/artery-font"
        "${CMAKE_CURRENT_SOURCE_DIR}/Vendor")

rayne_add_library(RayneUI "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_LIBRARIES Kalligraph)
rayne_set_module_output_directory(RayneUI)

target_include_directories(RayneUI SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_compile_definitions(RayneUI PRIVATE "${DEFINES}")
