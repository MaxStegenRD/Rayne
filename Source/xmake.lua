-- Generate RayneConfig.h (feature detection + template)
includes("rayne_config.lua")
includes("@builtin/check")

local platform_specific_files = {
	{ plats = {"iphoneos", "iphonesimulator"}, files = {"Input/RNInputIOS.cpp"} },
	{ plats = {"applexros"}, files = {"Input/RNInputIOS.cpp"} },
	{ plats = {"macosx"}, files = {"Input/RNInputOSX.cpp"} },
	{ plats = {"windows", "mingw"}, files = {"Input/RNInputWindows.cpp"} },
	{ plats = {"android"}, files = {"Input/RNInputAndroid.cpp"} },
	{ plats = {"linux"}, files = {"Input/RNInputLinux.cpp"} },
}

target("Rayne")
	set_kind("shared")
	add_defines("RN_BUILD_LIBRARY=1")
	add_includedirs(".", "../Vendor/utf8_v2_3_4/source", "../Vendor/concurrentqueue", "$(builddir)/generated/include", {public = true})
	add_files("**.cpp")

	local filesToRemove = {}
	for _, spec in ipairs(platform_specific_files) do
		if is_plat(table.unpack(spec.plats)) then
			add_files(table.unpack(spec.files))
		else
			table.insert(filesToRemove, table.unpack(spec.files))
		end
	end
	remove_files(table.unpack(filesToRemove))

	add_packages("zlib", "libpng", "libzip", "jansson", {public = true})
	on_load(function (target)
		for _, pkg in ipairs(target:pkgs()) do
			if pkg:name() == "jansson" then
				target:add("includedirs", pkg:installdir("include"))
			end
		end
	end)
	if is_plat("macosx") then
		add_frameworks("Foundation", "Cocoa", "IOKit", {public = true})
		add_cxxflags("-xobjective-c++")
	end
	-- Generate config header via rule (no direct function calls here)
	add_configfiles("RayneConfig.h.in", { filename = "RayneConfig.h", pattern = "%${(.-)}", prefixdir = "generated/include" })
	rayne_apply_config()

