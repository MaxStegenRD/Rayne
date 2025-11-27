-- RayneMetal (macOS)
target("RayneMetal")
	set_kind("shared")
	add_deps("Rayne")
	add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_METAL")
	add_includedirs("Sources", "../../Source", "$(builddir)/generated/include", {public = true})
	add_files("Sources/**.cpp")
	add_cxxflags("-xobjective-c++")
	add_frameworks("Metal", "QuartzCore", "Cocoa", {public = true})
	
	on_load(function (target)
		-- Determine which Resources directory to use based on platform
		local resources_dir = "Resources_macos"
		if is_plat("iphoneos") then
			resources_dir = "Resources_ios"
		elseif is_plat("iphonesimulator") then
			resources_dir = "Resources_ios_sim"
		elseif is_plat("applexros") then
			resources_dir = "Resources_visionos"
		end
		
		-- Register resources: destination name "Resources" maps to source "Resources_macos" (or platform variant)
		target:data_set("rayne_module_resources", {Resources = resources_dir})
	end)


