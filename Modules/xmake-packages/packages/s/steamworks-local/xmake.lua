package("steamworks-local")
    set_homepage("https://partner.steamgames.com/")
    set_description("Steamworks SDK binaries expected under this package's Vendor/ directory")

    on_load(function (package)
        local pkgdir = os.scriptdir()
        local local_root = path.join(pkgdir, "Vendor/sdk")
        package:data_set("steamworks.root", local_root)
    end)

    on_fetch(function (package, opt)
        local root = package:data("steamworks.root")
        if not root or not os.isdir(root) then
            wprint("steamworks-local: expected Vendor/ directory next to this xmake.lua (Steamworks sdk/ folder); please drop the Steamworks SDK there.")
            return
        end

        local sdkroot = root
        local incdir = path.join(sdkroot, "public/steam")
        if not os.isdir(incdir) then
            return
        end

        local result = {
            includedirs = {incdir},
            linkdirs    = {},
            links       = {},
            syslinks    = {},
            rundirs     = {},
            rpathdirs   = {},
            files       = {}
        }

        if package:plat() == "windows" then
            local libdir = path.join(sdkroot, "redistributable_bin/win64")
            table.insert(result.linkdirs, libdir)
            table.insert(result.links, "steam_api64")
            table.insert(result.files, path.join(libdir, "steam_api64.dll"))
        elseif package:plat() == "macosx" then
            local dylib = path.join(sdkroot, "redistributable_bin/osx/libsteam_api.dylib")
            table.insert(result.files, dylib)
            table.insert(result.linkdirs, path.directory(dylib))
            table.insert(result.links, "steam_api")
        elseif package:plat() == "linux" then
            local libdir = path.join(sdkroot, "redistributable_bin/linux64")
            table.insert(result.linkdirs, libdir)
            table.insert(result.links, "steam_api")
            table.insert(result.files, path.join(libdir, "libsteam_api.so"))
        else
            return
        end

        return result
    end)


