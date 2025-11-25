add_requires("kalligraph-local latest", {system = false})

target("RayneUI")
	set_kind("shared")
	set_languages("cxx20")
	add_deps("Rayne")
	add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_UI", "SK_RELEASE")
	add_includedirs(".", "../../Source", "$(builddir)/generated/include", {public = true})
	
	add_files("*.cpp")
	add_packages("kalligraph-local")

	local moduledir = os.scriptdir()
	local depsdir = path.join(os.projectdir(), "build", "_deps")
	local artery_dir = path.join(depsdir, "artery-font-format")
	
	add_includedirs(path.join(moduledir, "Vendor"))
	add_includedirs(path.join(artery_dir, "artery-font"), {public = true})

	on_load(function (target)
		local git = import("devel.git")
		os.mkdir(depsdir)

		if not os.isdir(artery_dir) then
			git.clone("https://github.com/Chlumsky/artery-font-format.git", {
				depth = 1,
				tag = "888674220216d1d326c6f29cf89165b545279c1f",
				outputdir = artery_dir
			})
		end

	end)


