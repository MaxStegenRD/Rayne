target("RayneBHaptics")
    set_kind("shared")
    set_languages("cxx20")
    add_deps("Rayne")

    add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_BHAPTICS")
    add_includedirs(".", "../../Source", "$(builddir)/generated/include", {public = true})

    add_files("*.cpp")
    add_headerfiles("*.h")

    if is_plat("windows", "mingw") then
        local moduledir = os.scriptdir()
        local bh_vendor = path.join(moduledir, "Vendor", "BhapticsCPP", "Win64")

        add_includedirs(path.join(moduledir, "Vendor", "BhapticsCPP"), {public = true})
        add_linkdirs(bh_vendor)
        add_links("bhaptics_library")
    end


