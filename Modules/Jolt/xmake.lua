-- RayneJolt
add_requires("joltphysics", {optional = false})

target("RayneJolt")
	set_kind("shared")
	add_deps("Rayne")
	add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_JOLT")
	add_includedirs(".", "../../Source", "$(builddir)/generated/include", {public = true})
	add_files("*.cpp")
	add_packages("joltphysics")


