package("picoplatform-local")
    set_homepage("https://developer.picoxr.com/")
    set_description("Pico Platform SDK binaries expected under this package's Vendor/ directory")

    on_load(function (package)
        local pkgdir = os.scriptdir()
        local local_root = path.join(pkgdir, "Vendor/PicoPlatform")
        package:data_set("pico.root", local_root)
    end)

    on_fetch(function (package, opt)
        local root = package:data("pico.root")
        if not root or not os.isdir(root) then
            wprint("picoplatform-local: expected Vendor/ directory next to this xmake.lua (PicoPlatform); please drop the Pico Platform SDK there.")
            return
        end

        local base = root
        local incdir = path.join(base, "include")
        local libdir = path.join(base, "lib/arm64-v8a")
        if not os.isdir(incdir) or not os.isdir(libdir) then
            return
        end

        local result = {
            includedirs = {incdir},
            linkdirs    = {libdir},
            links       = {"pxrplatformloader"},
            syslinks    = {},
            rundirs     = {},
            rpathdirs   = {},
            files       = {}
        }

        return result
    end)


