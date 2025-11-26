set_project("Rayne")

set_languages("cxx20")
add_rules("mode.debug", "mode.release")

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
            local bundle_path = path.join(outdir, target_name .. ".app", "Contents", "Resources")
            if os.isdir(bundle_path) then
                resource_dir = bundle_path
            else
                -- If bundle doesn't exist yet, use targetdir/Resources
                resource_dir = outdir
            end
        elseif is_plat("iphoneos", "iphonesimulator", "applexros") then
            -- For iOS/VisionOS, resources go into ResourceFiles
            local target_name = target:name()
            local bundle_path = path.join(outdir, target_name .. ".app", "Contents", "ResourceFiles")
            if os.isdir(bundle_path) then
                resource_dir = bundle_path
            else
                resource_dir = path.join(outdir, "ResourceFiles")
            end
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
rule("rayne_copy_module_resources")
    after_build(function (target)
        -- Get all dependencies that are Rayne modules
        local module_resources = {}
        local project = import("core.project.project")
        
        -- Iterate through all targets to find Rayne modules this target depends on
        for _, dep_name in ipairs(target:get("deps") or {}) do
            -- Check if this is a Rayne module (starts with "Rayne")
            if dep_name:match("^Rayne") then
                local dep_target = project.target(dep_name)
                if dep_target then
                    local resources = dep_target:get("rayne_module_resources") or {}
                    if type(resources) == "table" and #resources > 0 then
                        module_resources[dep_name] = {
                            target = dep_target,
                            resources = resources
                        }
                    end
                end
            end
        end
        
        if next(module_resources) == nil then
            return -- No module resources to copy
        end
        
        -- Determine output directory based on platform
        local outdir = target:targetdir()
        local resource_dir = outdir
        
        if is_plat("macosx") then
            local target_name = target:name()
            local bundle_path = path.join(outdir, target_name .. ".app", "Contents", "Resources")
            if os.isdir(bundle_path) then
                resource_dir = bundle_path
            else
                resource_dir = path.join(outdir, "Resources")
            end
        elseif is_plat("iphoneos", "iphonesimulator", "applexros") then
            local target_name = target:name()
            local bundle_path = path.join(outdir, target_name .. ".app", "Contents", "ResourceFiles")
            if os.isdir(bundle_path) then
                resource_dir = bundle_path
            else
                resource_dir = path.join(outdir, "ResourceFiles")
            end
        end
        
        local modules_dir = path.join(resource_dir, "Modules")
        
        -- Copy resources from each module
        for module_name, module_info in pairs(module_resources) do
            local module_target = module_info.target
            local resources = module_info.resources
            local module_outdir = module_target:targetdir()
            local module_dest_dir = path.join(modules_dir, module_name)
            
            -- Ensure destination directory exists
            if not os.isdir(module_dest_dir) then
                os.mkdir(module_dest_dir)
            end
            
            for _, resource in ipairs(resources) do
                local src_path = path.join(module_outdir, resource)
                local dst_path = path.join(module_dest_dir, resource)
                
                if os.isdir(src_path) then
                    -- Copy directory
                    if os.isdir(dst_path) then
                        os.rm(dst_path)
                    end
                    os.cp(src_path, dst_path)
                elseif os.isfile(src_path) then
                    -- Copy file - ensure parent directory exists
                    local dst_parent = path.directory(dst_path)
                    if not os.isdir(dst_parent) then
                        os.mkdir(dst_parent)
                    end
                    os.cp(src_path, dst_path)
                end
            end
        end
    end)

-- Delegate targets to per-directory xmake.lua to mirror CMake layout
includes("Source/xmake.lua")
includes("Modules/xmake.lua")
