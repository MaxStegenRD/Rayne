cmake_minimum_required(VERSION 3.28)
project(Vulkan)

find_package(Python COMPONENTS Interpreter)

include(../../CMake/LibraryTarget.cmake)

FetchContent_Declare(spirvcross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG refs/tags/2020-04-03
        GIT_SHALLOW ON
        GIT_PROGRESS ON)
FetchContent_GetProperties(spirvcross)
if(NOT spirvcross_POPULATED)
        FetchContent_Populate(spirvcross)
endif()
FetchContent_GetProperties(spirvcross SOURCE_DIR SPIRVCROSS_SOURCE_DIR)

rayne_fetch_content(VulkanHeaders
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG refs/tags/v1.4.348
        GIT_SHALLOW TRUE
        GIT_PROGRESS ON)
FetchContent_GetProperties(VulkanHeaders SOURCE_DIR VULKAN_HEADERS_SOURCE_DIR)
set(vulkanheaders_SOURCE_DIR "${VULKAN_HEADERS_SOURCE_DIR}" CACHE PATH "Path to Vulkan-Headers sources" FORCE)

FetchContent_Declare(vulkanmemoryallocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG b8e57472fffa3bd6e0a0b675f4615bf0a823ec4d
        GIT_PROGRESS ON)
FetchContent_GetProperties(vulkanmemoryallocator)
if(NOT vulkanmemoryallocator_POPULATED)
        FetchContent_Populate(vulkanmemoryallocator)
endif()
FetchContent_GetProperties(vulkanmemoryallocator SOURCE_DIR VULKAN_MEMORY_ALLOCATOR_SOURCE_DIR)

set(VMA_SOURCE_DIR "${VULKAN_MEMORY_ALLOCATOR_SOURCE_DIR}")
set(RAYNE_VULKAN_VMA_INCLUDE_DIRECTORY "${VMA_SOURCE_DIR}/include" CACHE INTERNAL "RayneVulkan private VMA include directory")


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
        ${SPIRVCROSS_SOURCE_DIR}/spirv_cfg.cpp
        ${SPIRVCROSS_SOURCE_DIR}/spirv_cross_parsed_ir.cpp
        ${SPIRVCROSS_SOURCE_DIR}/spirv_parser.cpp
        ${SPIRVCROSS_SOURCE_DIR}/spirv_cross.cpp)

set(HEADERS
        Sources/RNVulkan.h
        Sources/RNVulkanDevice.h
        Sources/RNVulkanDispatchTable.h
        Sources/RNVulkanInstance.h
        Sources/RNVulkanRenderer.h
        Sources/RNVulkanRendererDescriptor.h
        Sources/RNVulkanSwapChain.h
        Sources/RNVulkanWindow.h)

set(PRIVATE_HEADERS
        Sources/RNVulkanDebug.h
        Sources/RNVulkanFramebuffer.h
        Sources/RNVulkanGPUBuffer.h
        Sources/RNVulkanInternals.h
        Sources/RNVulkanShader.h
        Sources/RNVulkanShaderLibrary.h
        Sources/RNVulkanStateCoordinator.h
        Sources/RNVulkanTexture.h
        Sources/RNVulkanTextureInfo.h
        Sources/RNVulkanDynamicGPUBuffer.h
        Sources/RNVulkanStaticGPUBuffer.h)

set(RESOURCES
        Resources)

set(PRIVATE_DEFINES RN_BUILD_VULKAN)
set(PUBLIC_DEFINES)
set(PRIVATE_LIBRARIES)

if(WIN32)
    list(APPEND PUBLIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(ANDROID)
    list(APPEND PUBLIC_DEFINES VK_USE_PLATFORM_ANDROID_KHR)
    list(APPEND PRIVATE_LIBRARIES android)
elseif(UNIX AND NOT APPLE)
    list(APPEND PUBLIC_DEFINES VK_USE_PLATFORM_XCB_KHR)
endif()

rayne_add_library(RayneVulkan "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_LIBRARIES ${PRIVATE_LIBRARIES} PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneVulkan)
rayne_set_module_resources(RayneVulkan ${RESOURCES})

add_dependencies(RayneVulkan VulkanGenerateDispatchTable)

target_include_directories(RayneVulkan SYSTEM PRIVATE
    ${SPIRVCROSS_SOURCE_DIR}
    ${RAYNE_VULKAN_VMA_INCLUDE_DIRECTORY})
rayne_add_public_header_directory(RayneVulkan
    BUILD_DIRECTORY "${vulkanheaders_SOURCE_DIR}/include"
    INSTALL_DIRECTORY "include/Modules/RayneVulkan/ThirdParty/Vulkan-Headers")
target_compile_definitions(RayneVulkan
    PRIVATE "${PRIVATE_DEFINES}"
    PUBLIC "${PUBLIC_DEFINES}")
