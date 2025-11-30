add_requires("openal-soft")

target("RayneOpenAL")
	set_kind("shared")
	set_languages("cxx20")
	add_deps("Rayne")
	add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_OPENAL", "AL_LIBTYPE_STATIC")
	add_includedirs(".", "../../Source", "$(builddir)/generated/include", {public = true})
	add_files("*.cpp")
	add_packages("openal-soft")

	if is_plat("macosx", "iphoneos", "iphonesimulator", "applexros") then
		add_cxxflags("-xobjective-c++")
		add_frameworks("AVFoundation")
	end

	if is_plat("iphoneos", "iphonesimulator", "applexros") then
		add_frameworks("AudioToolbox")
	end

