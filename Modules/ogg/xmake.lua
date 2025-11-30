local moduledir = os.scriptdir()

target("RayneOgg")
	set_kind("shared")
	set_languages("cxx20")
	add_deps("Rayne")
	add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_OGG")
	add_includedirs(".", "../../Source", "$(builddir)/generated/include", {public = true})
	add_includedirs(path.join(moduledir, "Vendor"))
	add_files("*.cpp")


