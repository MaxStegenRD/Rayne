package("physx-local")
    set_homepage("https://github.com/NVIDIAGameWorks/PhysX")
    set_description("PhysX 4.1 static libraries built via the upstream cmake scripts.")
    set_license("BSD-3-Clause")

    add_urls("https://github.com/SlinDev-GmbH/PhysX.git")
    add_versions("latest", "c3d5537bdebd6f5cd82fcaf87474b838fe6fd5fa")

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
        local repo_dir = os.curdir()
        local linux_include = path.join(repo_dir, "physx", "source", "foundation", "include", "linux")
        if not os.isdir(linux_include) then
            os.cp(path.join(repo_dir, "physx", "source", "foundation", "include", "unix"), linux_include)
        end

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

        local function run_cmake(cmake, args)
            os.runv(cmake.program, args)
        end

        local function patch_file(filepath, mutator)
            if not os.isfile(filepath) then
                return
            end
            local fh = io.open(filepath, "rb")
            if not fh then
                return
            end
            local data = fh:read("*a")
            fh:close()
            if not data then
                return
            end
            local newdata, count = mutator(data)
            if newdata and count and count > 0 then
                local wf = io.open(filepath, "wb")
                if wf then
                    wf:write(newdata)
                    wf:close()
                end
            end
        end

        local function patch_mac()
            local mac_cmakelists = path.join(repo_dir, "physx", "source", "compiler", "cmake", "mac", "CMakeLists.txt")
            patch_file(mac_cmakelists, function(data)
                local changed = false
                local function remove(pattern)
                    local newdata, count = data:gsub(pattern, "")
                    if count > 0 then
                        data = newdata
                        changed = true
                    end
                end
                remove("[^\n]*CMAKE_OSX_ARCHITECTURES[^\n]*\n")
                remove("[^\n]*%-arch[^\n]*\n")
                local newdata, count = data:gsub("%-Werror", "")
                if count > 0 then
                    data = newdata
                    changed = true
                end
                newdata, count = data:gsub("%-msse2", "")
                if count > 0 then
                    data = newdata
                    changed = true
                end
                newdata, count = data:gsub("%-gdwarf%-2\"", "-gdwarf-2 -Wno-suggest-override -Wno-suggest-destructor-override\"")
                if count > 0 then
                    data = newdata
                    changed = true
                end
                return data, changed and 1 or 0
            end)
        end

        local function patch_android()
            local android_cmakelists = path.join(repo_dir, "physx", "source", "compiler", "cmake", "android", "CMakeLists.txt")
            patch_file(android_cmakelists, function(data)
                local newdata, count = data:gsub("%-Werror", "")
                return newdata, count
            end)
            local neon_header = path.join(repo_dir, "physx", "source", "foundation", "include", "unix", "neon", "PsUnixNeonInlineAoS.h")
            patch_file(neon_header, function(data)
                local newdata, count = data:gsub("(PX_FORCE_INLINE Vec4V V4SplatElement%(Vec4V a%)%s*{%s*)#if[^\n]*", "%1#if 1", 1)
                return newdata, count
            end)
        end

        local function patch_linux()
            local linux_cmakelists = path.join(repo_dir, "physx", "source", "compiler", "cmake", "linux", "CMakeLists.txt")
            patch_file(linux_cmakelists, function(data)
                local newdata, count = data:gsub("%-Werror", "")
                return newdata, count
            end)
            local gjk_header = path.join(repo_dir, "physx", "source", "geomutils", "src", "gjk", "GuGJKType.h")
            patch_file(gjk_header, function(data)
                local newdata, count = data:gsub("PX_FORCE_INLINE Ps::aos::PsMatTransformV& getRelativeTransform", "PX_FORCE_INLINE const Ps::aos::PsMatTransformV& getRelativeTransform")
                return newdata, count
            end)
        end

        local function patch_windows()
            local win_cmakelists = path.join(repo_dir, "physx", "source", "compiler", "cmake", "windows", "CMakeLists.txt")
            patch_file(win_cmakelists, function(data)
                local changed = false
                local newdata, count = data:gsub("/WX", "")
                if count > 0 then
                    data = newdata
                    changed = true
                end
                newdata, count = data:gsub("SET%(PHYSX_WINDOWS_DEBUG_COMPILE_DEFS   \"PX_DEBUG=1;PX_CHECKED=1;${NVTX_FLAG};PX_SUPPORT_PVD=1\"", "SET(PHYSX_WINDOWS_DEBUG_COMPILE_DEFS   \"PX_PROFILE=1;${NVTX_FLAG};PX_SUPPORT_PVD=1\"")
                if count > 0 then
                    data = newdata
                    changed = true
                end
                return data, changed and 1 or 0
            end)
        end

        local function patch_neon_splat()
            local neon_header = path.join(repo_dir, "physx", "source", "foundation", "include", "unix", "neon", "PsUnixNeonInlineAoS.h")
            local bsplat_old = [[template <int index>
PX_FORCE_INLINE BoolV BSplatElement(BoolV a)
{
	if(index < 2)
	{
		return vdupq_lane_u32(vget_low_u32(a), index);
	}
	else if(index == 2)
	{
		return vdupq_lane_u32(vget_high_u32(a), 0);
	}
	else if(index == 3)
	{
		return vdupq_lane_u32(vget_high_u32(a), 1);
	}
}

template <int index>
PX_FORCE_INLINE VecU32V V4U32SplatElement(VecU32V a)
{
	if(index < 2)
	{
		return vdupq_lane_u32(vget_low_u32(a), index);
	}
	else if(index == 2)
	{
		return vdupq_lane_u32(vget_high_u32(a), 0);
	}
	else if(index == 3)
	{
		return vdupq_lane_u32(vget_high_u32(a), 1);
	}
}

template <int index>
PX_FORCE_INLINE Vec4V V4SplatElement(Vec4V a)
{
#if PX_UWP
	if(index == 0)
	{
		return vdupq_lane_f32(vget_low_f32(a), 0);
	}
	else if (index == 1)
	{
		return vdupq_lane_f32(vget_low_f32(a), 1);
	}
#else
	if(index < 2)
	{
		return vdupq_lane_f32(vget_low_f32(a), index);
	}
#endif
	else if(index == 2)
	{
		return vdupq_lane_f32(vget_high_f32(a), 0);
	}
	else if(index == 3)
	{
		return vdupq_lane_f32(vget_high_f32(a), 1);
	}
}
]]

            local bsplat_new = [[template <int index>
PX_FORCE_INLINE BoolV BSplatElement(BoolV a);

template <>
PX_FORCE_INLINE BoolV BSplatElement<0>(BoolV a)
{
	return vdupq_lane_u32(vget_low_u32(a), 0);
}

template <>
PX_FORCE_INLINE BoolV BSplatElement<1>(BoolV a)
{
	return vdupq_lane_u32(vget_low_u32(a), 1);
}

template <>
PX_FORCE_INLINE BoolV BSplatElement<2>(BoolV a)
{
	return vdupq_lane_u32(vget_high_u32(a), 0);
}

template <>
PX_FORCE_INLINE BoolV BSplatElement<3>(BoolV a)
{
	return vdupq_lane_u32(vget_high_u32(a), 1);
}

template <int index>
PX_FORCE_INLINE VecU32V V4U32SplatElement(VecU32V a);

template <>
PX_FORCE_INLINE VecU32V V4U32SplatElement<0>(VecU32V a)
{
	return vdupq_lane_u32(vget_low_u32(a), 0);
}

template <>
PX_FORCE_INLINE VecU32V V4U32SplatElement<1>(VecU32V a)
{
	return vdupq_lane_u32(vget_low_u32(a), 1);
}

template <>
PX_FORCE_INLINE VecU32V V4U32SplatElement<2>(VecU32V a)
{
	return vdupq_lane_u32(vget_high_u32(a), 0);
}

template <>
PX_FORCE_INLINE VecU32V V4U32SplatElement<3>(VecU32V a)
{
	return vdupq_lane_u32(vget_high_u32(a), 1);
}

template <int index>
PX_FORCE_INLINE Vec4V V4SplatElement(Vec4V a);

template <>
PX_FORCE_INLINE Vec4V V4SplatElement<0>(Vec4V a)
{
	return vdupq_lane_f32(vget_low_f32(a), 0);
}

template <>
PX_FORCE_INLINE Vec4V V4SplatElement<1>(Vec4V a)
{
	return vdupq_lane_f32(vget_low_f32(a), 1);
}

template <>
PX_FORCE_INLINE Vec4V V4SplatElement<2>(Vec4V a)
{
	return vdupq_lane_f32(vget_high_f32(a), 0);
}

template <>
PX_FORCE_INLINE Vec4V V4SplatElement<3>(Vec4V a)
{
	return vdupq_lane_f32(vget_high_f32(a), 1);
}
]]

            io.replace(neon_header, bsplat_old, bsplat_new, {plain = true})
        end

        local function patch_allocator_include()
            local allocator_header = path.join(repo_dir, "physx", "source", "foundation", "include", "PsAllocator.h")
            patch_file(allocator_header, function(data)
                if data:find("#include <malloc/malloc.h>") then
                    return data, 0
                end
                local replacement = [[#if defined(__APPLE__)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif]]
                local newdata, count = data:gsub("#include <malloc.h>", replacement, 1)
                return newdata, count
            end)
        end

        local cmake = find_tool("cmake")
        assert(cmake, "cmake is required to build PhysX")

        local libs = {}
        local platform = package:plat()
        local arch = (config.get and config.get("arch")) or package:arch() or os.arch()

        patch_neon_splat()
        patch_allocator_include()

        if platform == "macosx" then
            patch_mac()
            local sourcedir = path.join(repo_dir, "physx", "compiler", "public")
            local buildroot = path.join(repo_dir, "build-rayne")
            os.mkdir(buildroot)
            local build_universal = package:config("mac_universal") and os.host() == "macosx"
            if build_universal then
                local function build_arch(subdir, architecture)
                    local dir = path.join(buildroot, subdir)
                    local args = {"-S", sourcedir, "-B", dir, "-DCMAKE_BUILD_TYPE=Release"}
                    concat_tables(args, base_cmake_args(dir, dir))
                    concat_tables(args, {
                        "-DTARGET_BUILD_PLATFORM=mac",
                        "-DPX_OUTPUT_ARCH=x64",
                        "-DCMAKE_OSX_ARCHITECTURES=" .. architecture,
                        "-DCMAKE_CXX_FLAGS=-Wno-atomic-implicit-seq-cst"
                    })
                    run_cmake(cmake, args)
                    run_cmake(cmake, {"--build", dir, "--config", "Release"})
                    return dir
                end
                local x64_dir = build_arch("x64", "x86_64")
                local arm_dir = build_arch("arm64", "arm64")
                local release_dir = path.join(buildroot, "release")
                os.rm(release_dir)
                os.mkdir(release_dir)
                local lipo = find_tool("lipo")
                assert(lipo, "lipo is required to build universal PhysX archives")
                for _, name in ipairs(physx_libs) do
                    local file = "lib" .. name .. ".a"
                    local x64lib = path.join(x64_dir, "bin", "mac.x86_64", "release", file)
                    local armlib = path.join(arm_dir, "bin", "mac.x86_64", "release", file)
                    local out = path.join(release_dir, file)
                    os.runv(lipo.program, {"-create", "-output", out, x64lib, armlib})
                    table.insert(libs, ensure_file(out))
                end
            else
                local dir = path.join(buildroot, "mac")
                local args = {"-S", sourcedir, "-B", dir, "-DCMAKE_BUILD_TYPE=Release"}
                concat_tables(args, base_cmake_args(dir, dir))
                concat_tables(args, {
                    "-DTARGET_BUILD_PLATFORM=mac",
                    "-DPX_OUTPUT_ARCH=x64",
                    "-DCMAKE_OSX_ARCHITECTURES=" .. arch,
                    "-DCMAKE_CXX_FLAGS=-Wno-atomic-implicit-seq-cst"
                })
                run_cmake(cmake, args)
                run_cmake(cmake, {"--build", dir, "--config", "Release"})
                for _, name in ipairs(physx_libs) do
                    table.insert(libs, ensure_file(path.join(dir, "bin", "mac.x86_64", "release", "lib" .. name .. ".a")))
                end
            end
        elseif platform == "iphoneos" or platform == "iphonesimulator" or platform == "applexros" then
            patch_mac()
            local sourcedir = path.join(repo_dir, "physx", "compiler", "public")
            local builddir = path.join(repo_dir, "build-rayne", platform .. "-" .. arch)
            os.mkdir(builddir)
            local args = {"-S", sourcedir, "-B", builddir, "-DCMAKE_BUILD_TYPE=Release"}
            concat_tables(args, base_cmake_args(builddir, builddir))
            concat_tables(args, {
                "-DTARGET_BUILD_PLATFORM=ios",
                "-DPX_OUTPUT_ARCH=arm",
                "-DCMAKE_TOOLCHAIN_FILE=" .. (config.get("toolchain") or ""),
                "-DCMAKE_OSX_SYSROOT=" .. (config.get("sdk") or ""),
                "-DCMAKE_SYSTEM_NAME=" .. platform,
                "-DCMAKE_CXX_FLAGS=-Wno-unknown-warning-option -Wno-invalid-noreturn -Wno-unused-private-field -Wno-unused-local-typedef -O3 -DNDEBUG"
            })
            run_cmake(cmake, args)
            run_cmake(cmake, {"--build", builddir, "--config", "Release"})
            local bindir = path.join(builddir, "physx", "bin", "ios.arm_64", "release")
            for _, name in ipairs(physx_libs) do
                table.insert(libs, ensure_file(path.join(bindir, "lib" .. name .. ".a")))
            end
        elseif platform == "android" then
            patch_android()
            local sourcedir = path.join(repo_dir, "physx", "compiler", "public")
            local builddir = path.join(repo_dir, "build-rayne", "android")
            os.mkdir(builddir)
            local args = {"-S", sourcedir, "-B", builddir, "-DCMAKE_BUILD_TYPE=Release"}
            concat_tables(args, base_cmake_args(builddir, builddir))
            concat_tables(args, {
                "-DTARGET_BUILD_PLATFORM=android",
                "-DPX_OUTPUT_ARCH=arm",
                "-DCMAKE_TOOLCHAIN_FILE=" .. (config.get("toolchain") or ""),
                "-DANDROID_NATIVE_API_LEVEL=" .. (config.get("android_api") or "24"),
                "-DANDROID_ABI=arm64-v8a",
                "-DANDROID_NDK=" .. (config.get("ndk") or ""),
                "-DANDROID_STL=" .. (config.get("ndk_cxxstl") or "c++_static"),
                "-DCMAKE_CXX_FLAGS=-Wno-unknown-warning-option -Wno-invalid-noreturn -Wno-unused-private-field -Wno-unused-local-typedef -D__ANDROID__ -O3 -DNDEBUG"
            })
            run_cmake(cmake, args)
            run_cmake(cmake, {"--build", builddir, "--config", "Release"})
            local bindir = path.join(builddir, "physx", "bin", "android.arm64-v8a.fp-soft", "release")
            for _, name in ipairs(physx_libs) do
                table.insert(libs, ensure_file(path.join(bindir, "lib" .. name .. ".a")))
            end
        elseif platform == "linux" then
            patch_linux()
            local sourcedir = path.join(repo_dir, "physx", "compiler", "public")
            local builddir = path.join(repo_dir, "build-rayne", "linux")
            os.mkdir(builddir)
            local args = {"-S", sourcedir, "-B", builddir, "-DCMAKE_BUILD_TYPE=Release"}
            concat_tables(args, base_cmake_args(builddir, builddir))
            concat_tables(args, {
                "-DTARGET_BUILD_PLATFORM=linux",
                "-DPX_OUTPUT_ARCH=x86",
                "-DCMAKE_C_COMPILER=" .. (config.get("cc") or ""),
                "-DCMAKE_CXX_COMPILER=" .. (config.get("cxx") or "")
            })
            if config.get("libarch") then
                table.insert(args, "-DCMAKE_LIBRARY_ARCHITECTURE=" .. config.get("libarch"))
            end
            run_cmake(cmake, args)
            run_cmake(cmake, {"--build", builddir, "--config", "Release"})
            local bindir = path.join(builddir, "physx", "bin", "linux.clang", "release")
            for _, name in ipairs(physx_libs) do
                table.insert(libs, ensure_file(path.join(bindir, "lib" .. name .. ".a")))
            end
        elseif platform == "windows" or platform == "mingw" then
            patch_windows()
            local sourcedir = path.join(repo_dir, "physx", "compiler", "public")
            local builddir = path.join(repo_dir, "build-rayne", "windows")
            os.mkdir(builddir)
            local args = {"-S", sourcedir, "-B", builddir, "-DCMAKE_BUILD_TYPE=Release"}
            concat_tables(args, base_cmake_args(builddir, builddir))
            concat_tables(args, {
                "-DTARGET_BUILD_PLATFORM=windows",
                "-DPX_OUTPUT_ARCH=x86",
                "-DPHYSX_CXX_FLAGS_DEBUG=/MDd",
                "-DCMAKE_SYSTEM_VERSION=" .. (config.get("windows_sdkver") or "")
            })
            run_cmake(cmake, args)
            run_cmake(cmake, {"--build", builddir, "--config", "Release"})
            local bindir = path.join(builddir, "physx", "bin", "win.x86_64.vc142.md", "release")
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

