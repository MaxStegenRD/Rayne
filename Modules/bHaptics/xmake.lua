target("RayneBHaptics")
    set_kind("shared")
    set_languages("cxx20")
    add_deps("Rayne")

    add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_BHAPTICS")
    add_includedirs(".", "../../Source", "$(builddir)/generated/include", {public = true})

    add_files("*.cpp")
    add_headerfiles("*.h")

    on_load(function (target)
        if is_plat("windows", "mingw") then
            local moduledir = os.scriptdir()
            local bh_vendor = path.join(moduledir, "Vendor", "BhapticsCPP", "Win64")
            target:data_set("rayne_copy_libs", { path.join(bh_vendor, "bhaptics_library.dll") })
            target:add("includedirs", path.join(moduledir, "Vendor", "BhapticsCPP"), {public = true})
            target:add("linkdirs", bh_vendor)
            target:add("links", "bhaptics_library")
        end
    end)

