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

-- Delegate targets to per-directory xmake.lua to mirror CMake layout
includes("Source/xmake.lua")
includes("Modules/xmake.lua")
