package("toxmod-local")
    set_homepage("https://github.com/slinDev/GRAB") -- local vendor drop
    set_description("Prebuilt ToxMod SDK binaries expected under this package's Vendor/ directory")

    on_load(function (package)
        local pkgdir = os.scriptdir()
        local local_root = path.join(pkgdir, "Vendor/tox")
        package:data_set("toxmod.root", local_root)
    end)

    on_fetch(function (package, opt)
        local root = package:data("toxmod.root")
        if not root or not os.isdir(root) then
            wprint("toxmod-local: expected vendor/ directory next to this xmake.lua (containing include/ and lib/); please drop the ToxMod SDK there.")
            return
        end

        local incdir = path.join(root, "include")
        local libdir = path.join(root, "lib")
        if not os.isdir(incdir) or not os.isdir(libdir) then
            return
        end

        local result = {
            includedirs = {incdir},
            linkdirs    = {},
            links       = {},
            syslinks    = {},
            rundirs     = {},
            rpathdirs   = {},
            files       = {},
            rayne_copy_libs = {}
        }

        if package:plat() == "windows" then
            local cfg = package:debug() and "Debug" or "Release"
            local winlib = path.join(libdir, "x64", cfg)
            table.insert(result.linkdirs, winlib)
            table.insert(result.links, "libtox")
            table.insert(result.files, path.join(winlib, "libtox.dll"))
            table.insert(result.files, path.join(winlib, "opus.dll"))
            table.insert(result.files, path.join(winlib, "opusenc.dll"))
            table.insert(result.files, path.join(winlib, "fvad.dll"))
            table.insert(result.files, path.join(winlib, cfg == "Debug" and "libcurl-d.dll" or "libcurl.dll"))
            table.insert(result.files, path.join(winlib, cfg == "Debug" and "zlibd1.dll" or "zlib1.dll"))
        elseif package:plat() == "macosx" then
            table.insert(result.links, path.join(libdir, "libtox.dylib"))
            table.insert(result.rayne_copy_libs, path.join(libdir, "libtox.dylib"))
            table.insert(result.rayne_copy_libs, path.join(libdir, "libopus.0.dylib"))
            table.insert(result.rayne_copy_libs, path.join(libdir, "libopusenc.0.dylib"))
            table.insert(result.rayne_copy_libs, path.join(libdir, "libfvad.0.dylib"))
        elseif package:plat() == "android" then
            table.insert(result.linkdirs, libdir)
            table.insert(result.links, "tox")
            table.insert(result.syslinks, "opus")
            table.insert(result.syslinks, "opusenc")
            table.insert(result.syslinks, "fvad")
            table.insert(result.syslinks, "curl")
            table.insert(result.syslinks, "ssl")
            table.insert(result.syslinks, "crypto")
        else
            return
        end

        return result
    end)
