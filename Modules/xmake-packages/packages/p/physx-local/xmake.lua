package("physx-local")
    set_homepage("https://github.com/NVIDIAGameWorks/PhysX")
    set_description("PhysX 4.1 static libraries built via the upstream cmake scripts.")
    set_license("BSD-3-Clause")

    add_urls("https://github.com/SlinDev-GmbH/PhysX.git")
    add_versions("latest", "4.1")

    add_deps("cmake")

    add_configs("mac_universal", {
        type = "boolean",
        default = false,
        description = "Build macOS universal (arm64 + x86_64) static libs"
    })

    local physx_libs = {
        "PhysXFoundation_static_64",
        "PhysXCommon_static_64",
        "PhysX_static_64",
        "PhysXCooking_static_64",
        "PhysXCharacterKinematic_static_64",
        "PhysXVehicle_static_64",
        "PhysXExtensions_static_64",
        "PhysXPvdSDK_static_64"
    }

    add_includedirs("include")
    add_includedirs("include/physx")
    add_includedirs("include/pxshared")
    add_linkdirs("lib")
    for _, name in ipairs(physx_libs) do
        add_links(name)
    end

    on_install(function (package)
        local find_tool = import("lib.detect.find_tool")
        local config = import("core.project.config")
        local cmake_tool = import("package.tools.cmake")
        local repo_dir = path.join(os.curdir(), "physx-local")
        local sourcedir = path.join(repo_dir, "physx", "compiler", "public")
        local buildroot = path.join(repo_dir, "build-rayne")
        local build_config = package:debug() and "Debug" or "Release"
        local config_dir = build_config:lower()
        os.mkdir(buildroot)

        --local linux_include = path.join(repo_dir, "physx", "source", "foundation", "include", "linux")
        --if not os.isdir(linux_include) then
        --    os.cp(path.join(repo_dir, "physx", "source", "foundation", "include", "unix"), linux_include)
        --end

        local function ensure_file(path)
            if not os.isfile(path) then
                os.raise("Expected PhysX artifact missing: " .. path)
            end
            return path
        end

        local function concat_tables(dst, src)
            for _, value in ipairs(src) do
                table.insert(dst, value)
            end
        end

        local function base_cmake_args(libdir, bindir)
            return {
                "-DPX_BUILDSNIPPETS=False",
                "-DPX_BUILDPUBLICSAMPLES=False",
                "-DPX_CMAKE_SUPPRESS_REGENERATION=True",
                "-DPX_GENERATE_STATIC_LIBRARIES=True",
                "-DPHYSX_ROOT_DIR=" .. path.join(repo_dir, "physx"),
                "-DPX_OUTPUT_LIB_DIR=" .. libdir,
                "-DPX_OUTPUT_BIN_DIR=" .. bindir,
                "-DPXSHARED_PATH=" .. path.join(repo_dir, "pxshared"),
                "-DCMAKEMODULES_PATH=" .. path.join(repo_dir, "externals", "cmakemodules"),
                "-DCMAKEMODULES_NAME=CMakeModules",
                "-DCMAKEMODULES_VERSION=1.27"
            }
        end

        local function run_cmake_build(subdir, extra_configs, opt)
            local builddir = path.join(buildroot, subdir)
            os.mkdir(builddir)
            local configs = {}
            concat_tables(configs, base_cmake_args(builddir, builddir))
            table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. build_config)
            if extra_configs then
                concat_tables(configs, extra_configs)
            end
            local build_opt = {builddir = builddir, config = build_config}
            if opt then
                for k, v in pairs(opt) do
                    build_opt[k] = v
                end
            end
            local previous_dir = os.cd(sourcedir)
            cmake_tool.build(package, configs, build_opt)
            os.cd(previous_dir)
            return builddir
        end

        local libs = {}
        local platform = package:plat()
        local arch = package:arch() or os.arch()

        if platform == "macosx" then
            local build_universal = package:config("mac_universal") and os.host() == "macosx"
            if build_universal then
                local function build_arch(subdir, architecture)
                    local extra = {
                        "-DTARGET_BUILD_PLATFORM=mac",
                        "-DPX_OUTPUT_ARCH=x64",
                        "-DCMAKE_OSX_ARCHITECTURES=" .. architecture,
                        "-DCMAKE_CXX_FLAGS=-Wno-atomic-implicit-seq-cst"
                    }
                    return run_cmake_build(subdir, extra)
                end
                local x64_dir = build_arch("x64", "x86_64")
                local arm_dir = build_arch("arm64", "arm64")
                local fatlib_dir = path.join(buildroot, config_dir)
                os.rm(fatlib_dir)
                os.mkdir(fatlib_dir)
                local lipo = find_tool("lipo")
                assert(lipo, "lipo is required to build universal PhysX archives")
                for _, name in ipairs(physx_libs) do
                    local file = "lib" .. name .. ".a"
                    local x64lib = path.join(x64_dir, "bin", "mac.x86_64", config_dir, file)
                    local armlib = path.join(arm_dir, "bin", "mac.x86_64", config_dir, file)
                    local out = path.join(fatlib_dir, file)
                    os.runv(lipo.program, {"-create", "-output", out, x64lib, armlib})
                    table.insert(libs, ensure_file(out))
                end
            else
                local extra = {
                    "-DTARGET_BUILD_PLATFORM=mac",
                    "-DPX_OUTPUT_ARCH=x64",
                    "-DCMAKE_OSX_ARCHITECTURES=" .. arch,
                    "-DCMAKE_CXX_FLAGS=-Wno-atomic-implicit-seq-cst"
                }
                local dir = run_cmake_build("mac", extra)
                for _, name in ipairs(physx_libs) do
                    table.insert(libs, ensure_file(path.join(dir, "bin", "mac.x86_64", config_dir, "lib" .. name .. ".a")))
                end
            end
        elseif platform == "iphoneos" or platform == "iphonesimulator" or platform == "applexros" then
            local extra = {
                "-DTARGET_BUILD_PLATFORM=ios",
                "-DPX_OUTPUT_ARCH=arm",
                "-DCMAKE_SYSTEM_NAME=" .. platform,
                "-DCMAKE_CXX_FLAGS=-Wno-unknown-warning-option -Wno-invalid-noreturn -Wno-unused-private-field -Wno-unused-local-typedef -O3 -DNDEBUG"
            }

            local builddir = run_cmake_build(platform .. "-" .. arch, extra)
            local bindir = path.join(builddir, "physx", "bin", "ios.arm_64", config_dir)
            for _, name in ipairs(physx_libs) do
                table.insert(libs, ensure_file(path.join(bindir, "lib" .. name .. ".a")))
            end
        elseif platform == "android" then
            local android_abi = package:arch() or "arm64-v8a"
            local extra = {
                "-DTARGET_BUILD_PLATFORM=android",
                "-DPX_OUTPUT_ARCH=arm",
                "-DCMAKE_CXX_FLAGS=-Wno-unknown-warning-option -Wno-invalid-noreturn -Wno-unused-private-field -Wno-unused-local-typedef -D__ANDROID__ -DANDROID -O3 -DNDEBUG"
            }
            local builddir = run_cmake_build("android", extra)
            local bindir = path.join(builddir, "bin", "android." .. android_abi .. ".fp-soft", config_dir)
            for _, name in ipairs(physx_libs) do
                table.insert(libs, ensure_file(path.join(bindir, "lib" .. name .. ".a")))
            end
        elseif platform == "linux" then
            local extra = {
                "-DTARGET_BUILD_PLATFORM=linux",
                "-DPX_OUTPUT_ARCH=x86"
            }
            local builddir = run_cmake_build("linux", extra)
            local bindir = path.join(builddir, "physx", "bin", "linux.clang", config_dir)
            for _, name in ipairs(physx_libs) do
                table.insert(libs, ensure_file(path.join(bindir, "lib" .. name .. ".a")))
            end
        elseif platform == "windows" or platform == "mingw" then
            local extra = {
                "-DTARGET_BUILD_PLATFORM=windows",
                "-DPX_OUTPUT_ARCH=x86",
                "-DPHYSX_CXX_FLAGS_DEBUG=/MDd"
            }
			
            local builddir = run_cmake_build("windows", extra)
            local bindir = path.join(builddir, "physx", "bin", "win.x86_64.vc142.md", config_dir)
            for _, name in ipairs(physx_libs) do
                table.insert(libs, ensure_file(path.join(bindir, name .. ".lib")))
            end
        else
            os.raise("PhysX package does not yet support platform: " .. platform)
        end

        local includedir = package:installdir("include")
        os.mkdir(includedir)
        os.cp(path.join(repo_dir, "pxshared", "include"), path.join(includedir, "pxshared"))
        os.cp(path.join(repo_dir, "physx", "include"), path.join(includedir, "physx"))

        local libdir = package:installdir("lib")
        os.mkdir(libdir)
        for _, lib in ipairs(libs) do
            os.cp(lib, libdir)
        end
    end)

    on_test(function (package)
        local defines = {}
        if package:debug() then
            table.insert(defines, "_DEBUG")
        else
            table.insert(defines, "NDEBUG")
        end
        assert(package:check_cxxsnippets({test = [[
            #include <physx/PxPhysicsAPI.h>
            using namespace physx;
            void test() {
                PxVec3 vec(1.0f, 0.0f, 0.0f);
            }
        ]]}, {configs = {languages = "c++20", defines = defines}}))
    end)

