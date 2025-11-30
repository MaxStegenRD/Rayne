-- RayneVR
target("RayneVR")
	set_kind("shared")
	add_deps("Rayne")
	add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_VR")
	add_includedirs(".", "../../Source", "$(builddir)/generated/include", {public = true})
	add_files("*.cpp")


