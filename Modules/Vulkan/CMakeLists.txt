cmake_minimum_required(VERSION 3.23)
project(Vulkan)

find_package(Python COMPONENTS Interpreter)

include(../../CMake/LibraryTarget.cmake)
include(ExternalProject)
include(FetchContent)

ExternalProject_Add(spirvcross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG refs/tags/2020-04-03
        GIT_SHALLOW ON
        GIT_PROGRESS ON
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        BUILD_BYPRODUCTS
            ${CMAKE_CURRENT_BINARY_DIR}/spirvcross-prefix/src/spirvcross/spirv_cfg.cpp
            ${CMAKE_CURRENT_BINARY_DIR}/spirvcross-prefix/src/spirvcross/spirv_cross_parsed_ir.cpp
            ${CMAKE_CURRENT_BINARY_DIR}/spirvcross-prefix/src/spirvcross/spirv_parser.cpp
            ${CMAKE_CURRENT_BINARY_DIR}/spirvcross-prefix/src/spirvcross/spirv_cross.cpp
        INSTALL_COMMAND "")

FetchContent_Declare(VulkanHeaders
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG refs/tags/v1.4.348
        GIT_SHALLOW TRUE
        GIT_PROGRESS ON)
FetchContent_MakeAvailable(VulkanHeaders)
set(vulkanheaders_SOURCE_DIR "${vulkanheaders_SOURCE_DIR}" CACHE PATH "Path to Vulkan-Headers sources" FORCE)

set(VMA_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/VulkanMemoryAllocator-prefix/src/VulkanMemoryAllocator)
set(RAYNE_VULKAN_VMA_INCLUDE_DIRECTORY "${VMA_SOURCE_DIR}/include" CACHE INTERNAL "RayneVulkan private VMA include directory")
ExternalProject_Add(VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG b8e57472fffa3bd6e0a0b675f4615bf0a823ec4d #origin/master is usually fine, but broke at the time of changing this, with a pull request waiting to be merged
        #GIT_SHALLOW ON #not allowed with commit hashes...
        GIT_PROGRESS ON
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND "")


set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/generators/dispatchTableParsedHeader.py ${CMAKE_CURRENT_SOURCE_DIR}/Sources/RNVulkanDispatchTable.h ${CMAKE_CURRENT_SOURCE_DIR}/Sources/RNVulkanDispatchTable.cpp
        COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/generators/generate-dispatch-table parse ${vulkanheaders_SOURCE_DIR}/include/vulkan/vulkan.h
        COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/generators/generate-dispatch-table ${CMAKE_CURRENT_SOURCE_DIR}/Sources/RNVulkanDispatchTable.h
        COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/generators/generate-dispatch-table ${CMAKE_CURRENT_SOURCE_DIR}/Sources/RNVulkanDispatchTable.cpp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/generators/generate-dispatch-table ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/generators/dispatchTableClasses.py ${vulkanheaders_SOURCE_DIR}/include/vulkan/vulkan.h
        COMMENT "Generate Vulkan Dispatch Table")

add_custom_target(VulkanGenerateDispatchTable DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Sources/RNVulkanDispatchTable.h ${CMAKE_CURRENT_SOURCE_DIR}/Sources/RNVulkanDispatchTable.cpp)


set(SOURCES
        Sources/RNVulkanDebug.cpp
        Sources/RNVulkanDevice.cpp
        Sources/RNVulkanDispatchTable.cpp
        Sources/RNVulkanFramebuffer.cpp
        Sources/RNVulkanInstance.cpp
        Sources/RNVulkanRenderer.cpp
        Sources/RNVulkanRendererDescriptor.cpp
        Sources/RNVulkanTexture.cpp
        Sources/RNVulkanTextureInfo.cpp
        Sources/RNVulkanWindow.cpp
        Sources/RNVulkanSwapChain.cpp
        Sources/RNVulkanInit.cpp
        Sources/RNVulkanInternals.cpp
        Sources/RNVulkanShader.cpp
        Sources/RNVulkanShaderLibrary.cpp
        Sources/RNVulkanStateCoordinator.cpp
        Sources/RNVulkanGPUBuffer.cpp
        Sources/RNVulkanDynamicGPUBuffer.cpp
        Sources/RNVulkanStaticGPUBuffer.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/spirvcross-prefix/src/spirvcross/spirv_cfg.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/spirvcross-prefix/src/spirvcross/spirv_cross_parsed_ir.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/spirvcross-prefix/src/spirvcross/spirv_parser.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/spirvcross-prefix/src/spirvcross/spirv_cross.cpp)

set(HEADERS
        Sources/RNVulkan.h
        Sources/RNVulkanDebug.h
        Sources/RNVulkanDevice.h
        Sources/RNVulkanDispatchTable.h
        Sources/RNVulkanInstance.h
        Sources/RNVulkanRenderer.h
        Sources/RNVulkanRendererDescriptor.h
        Sources/RNVulkanWindow.h)

set(PRIVATE_HEADERS
        Sources/RNVulkanFramebuffer.h
        Sources/RNVulkanGPUBuffer.h
        Sources/RNVulkanInternals.h
        Sources/RNVulkanShader.h
        Sources/RNVulkanShaderLibrary.h
        Sources/RNVulkanStateCoordinator.h
        Sources/RNVulkanSwapChain.h
        Sources/RNVulkanTexture.h
        Sources/RNVulkanTextureInfo.h
        Sources/RNVulkanDynamicGPUBuffer.h
        Sources/RNVulkanStaticGPUBuffer.h)

set(RESOURCES
        Resources)

set(PRIVATE_DEFINES RN_BUILD_VULKAN)
set(PUBLIC_DEFINES)

if(WIN32)
    list(APPEND PUBLIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(ANDROID)
    list(APPEND PUBLIC_DEFINES VK_USE_PLATFORM_ANDROID_KHR)
elseif(UNIX AND NOT APPLE)
    list(APPEND PUBLIC_DEFINES VK_USE_PLATFORM_XCB_KHR)
endif()

rayne_add_library(RayneVulkan "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneVulkan)
rayne_set_module_resources(RayneVulkan ${RESOURCES})

add_dependencies(RayneVulkan VulkanGenerateDispatchTable)
add_dependencies(RayneVulkan spirvcross)
add_dependencies(RayneVulkan VulkanMemoryAllocator)

target_include_directories(RayneVulkan SYSTEM PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/spirvcross-prefix/src/spirvcross
    ${RAYNE_VULKAN_VMA_INCLUDE_DIRECTORY})
target_include_directories(RayneVulkan SYSTEM PUBLIC
    ${vulkanheaders_SOURCE_DIR}/include
    ${vulkanheaders_SOURCE_DIR}/include/vulkan)
target_compile_definitions(RayneVulkan
    PRIVATE "${PRIVATE_DEFINES}"
    PUBLIC "${PUBLIC_DEFINES}")

rayne_install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../../Build/RayneVulkan/. DESTINATION lib/Rayne/RayneVulkan)
