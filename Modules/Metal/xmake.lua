-- RayneMetal (macOS)
target("RayneMetal")
	set_kind("shared")
	add_deps("Rayne")
	add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_METAL")
	add_includedirs("Sources", "../../Source", "$(builddir)/generated/include", {public = true})
	add_files("Sources/**.cpp")
	add_cxxflags("-xobjective-c++")
	add_frameworks("Metal", "QuartzCore", "Cocoa", {public = true})


