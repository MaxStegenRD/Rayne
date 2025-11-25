target("RayneEOS")
    set_kind("shared")
    set_languages("cxx20")
    add_deps("Rayne")
    add_defines("RN_BUILD_LIBRARY=1", "RN_BUILD_EOS")
    add_includedirs(".", "../../Source", "$(builddir)/generated/include", {public = true})
    add_files("*.cpp")

    after_build(function (target)
        local bins = target:data("eos_copybins")
        if bins then
            for _, bin in ipairs(bins) do
                if os.isfile(bin) then
                    os.cp(bin, target:targetdir())
                end
            end
        end
    end)

    on_load(function (target)
		local moduledir = os.scriptdir()
		local vendordir = path.join(moduledir, "Vendor")

        local includes = {}
        local libs = {}

        local function ensure_dir(dir, label)
            if not os.isdir(dir) then
                raise("EOS SDK %s directory not found: %s\nPlease unpack the EOS SDK into %s", label, dir, vendordir)
            end
        end

        local function ensure_file(file, label)
            if not os.isfile(file) then
                raise("EOS SDK %s file not found: %s\nPlease unpack the EOS SDK into %s", label, file, vendordir)
            end
        end

        if is_plat("android") then
            local inc = path.join(vendordir, "EOS-SDK-Android", "SDK", "Include")
            local lib = path.join(vendordir, "EOS-SDK-Android", "SDK", "Lib", "libEOSSDK.so")
            ensure_dir(inc, "include")
            ensure_file(lib, "library")
            table.insert(includes, inc)
            table.insert(libs, lib)
        elseif is_plat("iphoneos", "iphonesimulator", "applexros") then
            local inc = path.join(vendordir, "EOS-SDK-IOS", "SDK", "Bin", "IOS", "EOSSDK.framework", "Headers")
            ensure_dir(inc, "framework headers")
            table.insert(includes, inc)
            target:add("frameworks", "EOSSDK")
        elseif is_plat("windows", "mingw") then
            local inc = path.join(vendordir, "EOS-SDK", "SDK", "Include")
            local lib = path.join(vendordir, "EOS-SDK", "SDK", "Lib", "EOSSDK-Win64-Shipping.lib")
            local dll = path.join(vendordir, "EOS-SDK", "SDK", "Bin", "EOSSDK-Win64-Shipping.dll")
            local xaudio = path.join(vendordir, "EOS-SDK", "SDK", "Bin", "x64", "xaudio2_9redist.dll")
            ensure_dir(inc, "include")
            ensure_file(lib, "import lib")
            ensure_file(dll, "DLL")
            ensure_file(xaudio, "xaudio DLL")
            table.insert(includes, inc)
            table.insert(libs, lib)
            target:data_set("eos_copybins", {
                dll,
                xaudio
            })
        else
            local inc = path.join(vendordir, "EOS-SDK", "SDK", "Include")
            local dylib = path.join(vendordir, "EOS-SDK", "SDK", "Bin", "libEOSSDK-Mac-Shipping.dylib")
            ensure_dir(inc, "include")
            ensure_file(dylib, "dylib")
            table.insert(includes, inc)
            table.insert(libs, dylib)
            target:data_set("eos_copybins", {
                dylib
            })
        end

        for _, inc in ipairs(includes) do
            target:add("includedirs", inc, {public = true})
        end

        for _, lib in ipairs(libs) do
            target:add("links", lib)
        end
    end)
