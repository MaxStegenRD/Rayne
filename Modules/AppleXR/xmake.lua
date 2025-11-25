-- RayneAppleXR (macOS)
target("RayneAppleXR")
	set_kind("shared")
	add_deps("Rayne", "RayneVR")
	add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_APPLEXR", "APPLEXR_BUILD_STATIC", "RN_APPLEXR_SUPPORTS_METAL")
	add_includedirs(".", "../VRWrapper", "../Metal/Sources", "../../Source", "$(builddir)/generated/include", {public = true})
	add_files("*.cpp", "*.mm")
	add_cxxflags("-xobjective-c++")
	add_deps("RayneMetal")
	add_frameworks("ARKit", "CompositorServices", {public = true})


