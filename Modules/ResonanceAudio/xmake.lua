-- RayneResonanceAudio
-- Mirror the module's CMake: fetch Resonance Audio sources and build them directly.
-- This avoids relying on upstream `cmake --install` targets (fixes ninja 'install' errors)
-- and uses xmake built-ins and devel.git for fetching dependencies.

target("RayneResonanceAudio")
	set_kind("shared")
	set_languages("cxx20", "c99")
	add_deps("Rayne")
	add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_RESONANCE_AUDIO", "EIGEN_MPL2_ONLY")
	add_includedirs(".", "../../Source", "$(builddir)/generated/include", {public = true})
	add_files("*.cpp")

	local deps_root = path.join(os.projectdir(), "build", "_deps")
	local resdir = path.join(deps_root, "resonance-audio")
	local madir = path.join(deps_root, "miniaudio")

	-- Wire the known include directories and source globs. The install hook below populates them.
	add_includedirs(resdir, { public = true })
	add_includedirs(path.join(resdir, "resonance_audio"), { public = true })
	add_includedirs(path.join(resdir, "third_party", "eigen"), { public = true })
	add_includedirs(path.join(resdir, "third_party", "pffft"))
	add_includedirs(madir)

	add_files(path.join(resdir, "third_party", "pffft", "*.c"))
	add_files(path.join(resdir, "third_party", "SADIE_hrtf_database", "generated", "*.cc"))
	add_files(path.join(resdir, "platforms", "common", "*.cc"))
	add_files(path.join(resdir, "resonance_audio", "**.cc"))

	remove_files(path.join(resdir, "**/*_test.cc"))
	remove_files(path.join(resdir, "**/*test.cc"))
	remove_files(path.join(resdir, "**/*tests.cc"))
	remove_files(path.join(resdir, "**/test*"))
	remove_files(path.join(resdir, "resonance_audio", "geometrical_acoustics", "**.cc"))
	remove_files(path.join(resdir, "resonance_audio", "utils", "*vorbis_*"))

	on_load(function ()
		import("devel.git")
		os.mkdir(deps_root)

		if not os.isdir(resdir) then
			git.clone("https://github.com/Slin/resonance-audio.git", { depth = 1, outputdir = resdir })
			local deps = path.join(resdir, "third_party", "clone_core_deps.sh")
			if os.isfile(deps) then
				os.runv("bash", { deps })
			end
		end

		if not os.isdir(madir) then
			git.clone("https://github.com/mackron/miniaudio.git", { depth = 1, outputdir = madir })
		end
	end)

	if is_plat("macosx") then
		add_frameworks("CoreAudio", "CoreFoundation", "AVFAudio", "AudioToolbox", "AVFoundation", {public = true})
		add_cxxflags("-xobjective-c++")
	end


