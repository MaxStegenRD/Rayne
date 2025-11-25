add_requires("openxr-local 1.1.53", {system = false})

target("RayneOpenXR")

    set_kind("shared")
    set_languages("cxx20")
    add_deps("Rayne", "RayneVR")
    add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_OPENXR")
    add_includedirs(".", "../VRWrapper", "../../Source", "$(builddir)/generated/include", {public = true})
    add_files("*.cpp")
    add_headerfiles("*.h")
	add_packages("openxr-local")

    local filesToRemove = {"RNOpenXRD3D12SwapChain.cpp"}
    local headersToRemove = {"RNOpenXRD3D12SwapChain.h"}

    if is_plat("android") then
        add_defines("XR_USE_PLATFORM_ANDROID")
    elseif is_plat("windows", "mingw") then
        add_defines("XR_USE_PLATFORM_WIN32")
    elseif is_plat("linux") then
        add_defines("XR_USE_PLATFORM_XLIB")
    end

    if has_config("rayne_build_vulkan") then
        print("Including OpenXR Vulkan module")
        add_deps("RayneVulkan")
        add_defines("XR_USE_GRAPHICS_API_VULKAN")
        add_includedirs(path.join(os.projectdir(), "Modules/Vulkan/Sources"))
    else
        table.insert(filesToRemove, "RNOpenXRVulkanSwapChain.cpp")
        table.insert(headersToRemove, "RNOpenXRVulkanSwapChain.h")
    end

    if has_config("rayne_build_metal") then
        print("Including OpenXR Metal module")
        add_deps("RayneMetal")
        add_defines("XR_USE_GRAPHICS_API_METAL")
        add_includedirs(path.join(os.projectdir(), "Modules/Metal/Sources"))
        add_cxxflags("-xobjective-c++")
    else
        table.insert(filesToRemove, "RNOpenXRMetalSwapChain.cpp")
        table.insert(headersToRemove, "RNOpenXRMetalSwapChain.h")
    end

    remove_files(table.unpack(filesToRemove))
    remove_headerfiles(table.unpack(headersToRemove))

    if is_plat("linux") then
        add_links("dl", "pthread")
    end

