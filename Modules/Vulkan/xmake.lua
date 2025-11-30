target("RayneVulkan")
	local moduledir = os.curdir()
	local depsdir = path.join(os.projectdir(), "build", "_deps")
	
    set_kind("shared")
    set_languages("cxx20", "c99")
    add_deps("Rayne")
    add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_VULKAN")
    add_includedirs("Sources", "../../Source", "$(builddir)/generated/include", {public = true})
    add_files("Sources/**.cpp")

    if is_plat("windows") then
        add_defines("VK_USE_PLATFORM_WIN32_KHR")
    elseif is_plat("android") then
        add_defines("VK_USE_PLATFORM_ANDROID_KHR")
    elseif is_plat("linux") then
        add_defines("VK_USE_PLATFORM_XCB_KHR")
    end

    if is_plat("windows") then
        add_syslinks("user32", "shell32", "setupapi")
    end

    on_load(function (target)
        local git = import("devel.git")
        local find_tool = import("lib.detect.find_tool")

        os.mkdir(depsdir)

        local function ensure_repo(url, dir, opts)
            if not os.isdir(dir) then
                git.clone(url, opts or {depth = 1, outputdir = dir})
            end
        end

        local spirv_dir = path.join(depsdir, "spirv-cross")
        ensure_repo("https://github.com/KhronosGroup/SPIRV-Cross.git", spirv_dir, {
            depth = 1,
            tag = "2020-04-03",
            outputdir = spirv_dir
        })

        local vkheaders_dir = path.join(depsdir, "vulkan-headers")
        ensure_repo("https://github.com/KhronosGroup/Vulkan-Headers.git", vkheaders_dir, {
            depth = 1,
            tag = "v1.3.210",
            outputdir = vkheaders_dir
        })

        local vma_dir = path.join(depsdir, "vulkan-memory-allocator")
        ensure_repo("https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git", vma_dir, {
            outputdir = vma_dir,
            tag = "b8e57472fffa3bd6e0a0b675f4615bf0a823ec4d"
        })

        -- Generate the dispatch table using the vendored python script (same as CMake)
        local python = find_tool("python3") or find_tool("python")
        assert(python, "python is required to generate the Vulkan dispatch table")
        local generator = path.join(moduledir, "Vendor", "generators", "generate-dispatch-table")
        local vulkan_header = path.join(vkheaders_dir, "include", "vulkan", "vulkan.h")
        os.runv(python.program, {generator, "parse", vulkan_header})
        os.runv(python.program, {generator, path.join(moduledir, "Sources", "RNVulkanDispatchTable.h")})
        os.runv(python.program, {generator, path.join(moduledir, "Sources", "RNVulkanDispatchTable.cpp")})

        target:add("includedirs", spirv_dir)
        target:add("includedirs", path.join(vkheaders_dir, "include"), {public = true})
        target:add("includedirs", path.join(vkheaders_dir, "include", "vulkan"), {public = true})
        target:add("includedirs", path.join(vma_dir, "include"), {public = true})

        if not target:data("rayne_spirv_added") then
            target:add("files",
                path.join(spirv_dir, "spirv_cfg.cpp"),
                path.join(spirv_dir, "spirv_cross_parsed_ir.cpp"),
                path.join(spirv_dir, "spirv_parser.cpp"),
                path.join(spirv_dir, "spirv_cross.cpp"))
            target:data_set("rayne_spirv_added", true)
        end
        
        -- Register resources for copying to application
        target:data_set("rayne_module_resources", {"Resources"})
    end)
