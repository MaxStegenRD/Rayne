set_project("Rayne")

set_languages("cxx20")
add_rules("mode.debug", "mode.release")

if not is_plat("windows", "mingw") then
	add_cflags("-fvisibility=hidden")
	add_cxxflags("-fvisibility-ms-compat", "-fvisibility-inlines-hidden")
end

option("rayne_build_jolt")
	set_default(true)
option_end()

option("rayne_build_vulkan")
	set_default(true)
	after_check(function (option)
        if is_plat("macosx", "iphoneos", "iphonesimulator", "applexros") then
            option:enable(false)
        end
    end)
option_end()

option("rayne_build_metal")
	set_default(is_host("macosx"))
	after_check(function (option)
        if not is_plat("macosx", "iphoneos", "iphonesimulator", "applexros") then
            option:enable(false)
        end
    end)
option_end()

option("rayne_build_openxr")
	set_default(true)

option("rayne_build_openal")
	set_default(true)
option_end()

option("rayne_build_ogg")
	set_default(true)
option_end()

option("rayne_build_ui")
	set_default(true)
option_end()

option("rayne_build_eos")
	set_default(true)
option_end()

option("rayne_build_physx")
	set_default(true)
option_end()

option("rayne_build_bhaptics")
	set_default(true)
option_end()

-- Use external packages instead of vendored builds for simplicity
add_requires("zlib", "libpng", "libzip", "jansson")
-- add_requires("python >=3.*") -- Could maybe make this a host thing, to ensure it actually exist without user having to install it


-- Rule for processing and copying resources
-- Usage: add_rules("rayne_copy_resources")
--       target:set("rayne_resources", {"Resources", "manifest.json", "vr_splash.png"})
--       target:set("rayne_additional_pack_params", {"--is-demo"})  -- optional
rule("rayne_copy_resources")
    before_build(function (target)
        local resources = target:get("rayne_resources") or {}
        local additional_pack_params = target:get("rayne_additional_pack_params") or {}

		print("Processing resources for target: " .. target:name())
        
        if #resources == 0 then
            return
        end

        local find_program = import("lib.detect.find_program")
        local python = find_program("python3") or find_program("python")
        if not python then
            raise("Python not found - required for resource packing")
        end
        
        -- Get the source directory from the target (where the target's xmake.lua is)
        -- target:scriptdir() gives us the directory where the target is defined
        local source_dir = target:scriptdir()
        if not source_dir or source_dir == "" then
            -- Fallback to project directory if scriptdir is not available
            source_dir = os.projectdir()
        end
        
        -- Get Rayne directory (where this rule is defined)
        local rayne_dir = path.absolute(os.scriptdir())
        local pack_script = path.join(rayne_dir, "Tools", "ResourcePacker", "pack.py")
        
        if not os.isfile(pack_script) then
            raise("Resource packer script not found: " .. pack_script)
        end
        
        -- Determine output directory based on platform
        local outdir = target:targetdir()
        local resource_dir = outdir
        
        if is_plat("macosx") then
            -- For macOS, resources go into the bundle's Resources directory
            -- Bundle structure: AppName.app/Contents/Resources/
            local target_name = target:name()
            resource_dir = path.join(outdir, target_name .. ".app", "Contents", "Resources")
        elseif is_plat("iphoneos", "iphonesimulator", "applexros") then
            -- For iOS/VisionOS, resources go into ResourceFiles
            local target_name = target:name()
            resource_dir = path.join(outdir, target_name .. ".app", "Contents", "ResourceFiles")
        elseif is_plat("android") then
            -- For Android, resources go into assets directory
            -- This would need to be configured based on Android project structure
            resource_dir = outdir
        end
        
        -- Ensure output directory exists
        if not os.isdir(resource_dir) then
            os.mkdir(resource_dir)
        end
        
        -- Determine platform string for pack.py
        local platform_str = "linux"
        if is_plat("windows") then
            platform_str = "windows"
        elseif is_plat("macosx") then
            platform_str = "macos"
        elseif is_plat("android") then
            platform_str = "android"
        elseif is_plat("iphoneos") then
            platform_str = "ios"
        elseif is_plat("iphonesimulator") then
            platform_str = "ios_sim"
        elseif is_plat("applexros") then
            platform_str = "visionos"
        end
        
        -- Process each resource
        for _, resource in ipairs(resources) do
            local src_path = path.join(source_dir, resource)
            local dst_path = path.join(resource_dir, resource)

			print("Processing resource: " .. src_path .. " -> " .. dst_path)
            
            if os.isdir(src_path) then
                -- Directory: use pack.py
                local args = {pack_script, src_path, dst_path, platform_str}
                for _, param in ipairs(additional_pack_params) do
                    table.insert(args, param)
                end
                os.execv(python, args)
            else
                -- File: simple copy
                if os.isfile(src_path) then
                    os.cp(src_path, dst_path)
                end
            end
        end
    end)

-- Rule to copy module resources to application Resources folder
-- This is automatically applied when modules are used via add_deps
rule("rayne_copy_engine_files")
    after_build(function (target)
        local module_resources = {}
        local project = import("core.project.project")
        
        -- Find all Rayne module dependencies with registered resources
        for _, dep_name in ipairs(target:get("deps") or {}) do
			local dep_target = project.target(dep_name)
			if dep_target then
				local resources = dep_target:data("rayne_module_resources")
				if resources and type(resources) == "table" then
					-- Check if table has any entries
					for _ in pairs(resources) do
						module_resources[dep_name] = {
							target = dep_target,
							resources = resources
						}
						break
					end
				end
			end
        end
        
        -- Check if any module resources were found
        local has_resources = false
        for _ in pairs(module_resources) do
            has_resources = true
            break
        end
        if not has_resources then
            return
        end
        
        -- Determine output directory based on platform
        local outdir = target:targetdir()
        local resource_dir = outdir
        
        if is_plat("macosx") then
            local bundle_path = path.join(outdir, target:name() .. ".app", "Contents", "Resources")
            resource_dir = os.isdir(bundle_path) and bundle_path or outdir
        elseif is_plat("iphoneos", "iphonesimulator", "applexros") then
            local bundle_path = path.join(outdir, target:name() .. ".app", "Contents", "ResourceFiles")
            resource_dir = os.isdir(bundle_path) and bundle_path or outdir
        end
        
        local modules_dir = path.join(resource_dir, "Resources", "Modules")

        -- Copy resources from each module
        for module_name, module_info in pairs(module_resources) do
            local module_outdir = module_info.target:scriptdir()
            local module_dest_dir = path.join(modules_dir, module_name)
            os.mkdir(module_dest_dir)
            
            print("Copying resources from module: " .. module_name)
            for key, value in pairs(module_info.resources) do
                local src_name, dst_name
                if type(key) == "number" then
                    src_name = value
                    dst_name = value
                else
                    dst_name = key
                    src_name = value
                end
                
                local src_path = path.join(module_outdir, src_name)
                local dst_path = path.join(module_dest_dir, dst_name)

                if os.isdir(src_path) then
                    if os.isdir(dst_path) then
                        os.rm(dst_path)
                    end
                    os.cp(src_path, dst_path)
                elseif os.isfile(src_path) then
                    local dst_parent = path.directory(dst_path)
                    if not os.isdir(dst_parent) then
                        os.mkdir(dst_parent)
                    end
                    os.cp(src_path, dst_path)
                end
            end
        end

        -- For macOS bundles, dylibs need to go in Contents/MacOS
        if is_plat("macosx") then
            local bundle_path = path.join(outdir, target:name() .. ".app", "Contents", "Frameworks")
            if os.isdir(bundle_path) then
                outdir = bundle_path
            end
        end

         -- Loop through all dependencies
        for _, dep_name in ipairs(target:get("deps") or {}) do
            local dep_target = project.target(dep_name)
            local files = dep_target:data("rayne_copy_libs") or {}
            for _, f in ipairs(files) do
                print(f)
                if os.isfile(f) then
                    os.cp(f, outdir)
                end
            end
        end

        -- Loop through all packages
        for _, pkg_name in ipairs(target:get("packages") or {}) do
            local pkg = project.required_package(pkg_name)
            if pkg then
                local files = pkg:get("rayne_copy_libs") or {}
                for _, f in ipairs(files) do
                    print(f)
                    if os.isfile(f) then
                        os.cp(f, outdir)
                    end
                end
            end
        end
    end)

-- Delegate targets to per-directory xmake.lua to mirror CMake layout
includes("Source/xmake.lua")
includes("Modules/xmake.lua")
