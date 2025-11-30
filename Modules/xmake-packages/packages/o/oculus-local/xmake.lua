package("oculus-local")
    set_homepage("https://developer.oculus.com/")
    set_description("Oculus Platform SDK binaries expected under this package's Vendor/ directory")

    on_load(function (package)
        local pkgdir = os.scriptdir()
        local local_root = path.join(pkgdir, "Vendor/OculusPlatform")
        package:data_set("oculus.root", local_root)
    end)

    on_fetch(function (package, opt)
        local root = package:data("oculus.root")
        if not root or not os.isdir(root) then
            wprint("oculus-local: expected Vendor/ directory next to this xmake.lua (OculusPlatform); please drop the Oculus Platform SDK there.")
            return
        end

        local base = root
        local incdir = path.join(base, "Include")
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

        if package:plat() == "android" then
            local libdir = path.join(base, "Android/libs/arm64-v8a")
            table.insert(result.linkdirs, libdir)
            table.insert(result.links, "ovrplatformloader")
        elseif package:plat() == "windows" then
            local libdir = path.join(base, "Windows")
            table.insert(result.linkdirs, libdir)
            table.insert(result.links, "LibOVRPlatform64_1")
        else
            return
        end

        return result
    end)


