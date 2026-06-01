-- Rayne config generation (clean, target-aware)
includes("@builtin/check")

local function set_version()
	set_configvar("VERSION_MAJOR", 2)
	set_configvar("VERSION_MINOR", 0)
	set_configvar("VERSION_PATCH", 0)
	set_configvar("VERSION_ABI", 10)
end

local function detect_headers()
	configvar_check_cincludes("HAVE_STDINT_H", "stdint.h")
	configvar_check_cincludes("HAVE_STDDEF_H", "stddef.h")
end

local function set_cxx20_basics()
	-- C++20 guarantees these
	set_configvar("RAYNE_ALIGNAS", "alignas(x)", {quote = false})
	set_configvar("RAYNE_NOEXCEPT", "noexcept", {quote = false})
	set_configvar("RAYNE_UNUSED", "[[maybe_unused]]", {quote = false})
	set_configvar("RAYNE_CONSTEXPR", "constexpr", {quote = false})
	set_configvar("RAYNE_NORETURN", "[[noreturn]]", {quote = false})
	set_configvar("RAYNE_SUPPORTS_TRIVIALLY_COPYABLE", 1)

	-- Fixed-width types (from <cstdint>)
	set_configvar("RAYNE_INT8", "int8_t", {quote = false})
	set_configvar("RAYNE_UINT8", "uint8_t", {quote = false})
	set_configvar("RAYNE_INT16", "int16_t", {quote = false})
	set_configvar("RAYNE_UINT16", "uint16_t", {quote = false})
	set_configvar("RAYNE_INT32", "int32_t", {quote = false})
	set_configvar("RAYNE_UINT32", "uint32_t", {quote = false})
	set_configvar("RAYNE_INT64", "int64_t", {quote = false})
	set_configvar("RAYNE_UINT64", "uint64_t", {quote = false})
end

local function detect_attributes()
	-- Prefer vendor attributes when available; otherwise fall back to portable forms
	set_configvar("RAYNE_INLINE", "inline", {quote = false})
	set_configvar("RAYNE_NOINLINE", "", {quote = false})
	configvar_check_cxxsnippets(
		"RAYNE_NOINLINE=__attribute__((noinline))",
		"__attribute__((noinline)) void f() {} int main(){ f(); return 0; }",
		{quote = false, name = "__rayne_noinline_gnu"}
	)
	if not has_config("__rayne_noinline_gnu") then
		configvar_check_cxxsnippets(
			"RAYNE_NOINLINE=__declspec(noinline)",
			"__declspec(noinline) void f() {} int main(){ f(); return 0; }",
			{quote = false, name = "__rayne_noinline_msvc"}
		)
	end
end

local function detect_signature_and_expect()
	-- function signature macro
	-- Prefer __PRETTY_FUNCTION__ as it includes namespace information needed for MetaClass
	set_configvar("RAYNE_FUNCTION_SIGNATURE", "__func__", {quote = false})
	configvar_check_cxxsnippets(
		"RAYNE_FUNCTION_SIGNATURE=__PRETTY_FUNCTION__",
		"int main(){ (void)__PRETTY_FUNCTION__; return 0; }",
		{quote = false, name = "__rayne_funcsig_pretty"}
	)
	if has_config("__rayne_funcsig_pretty") then
		set_configvar("RAYNE_FUNCTION_SIGNATURE", "__PRETTY_FUNCTION__", {quote = false})
	else
		configvar_check_cxxsnippets(
			"RAYNE_FUNCTION_SIGNATURE=__FUNCTION__",
			"int main(){ (void)__FUNCTION__; return 0; }",
			{quote = false, name = "__rayne_funcsig_function"}
		)
		if has_config("__rayne_funcsig_function") then
			set_configvar("RAYNE_FUNCTION_SIGNATURE", "__FUNCTION__", {quote = false})
		end
		-- Otherwise keep __func__ as fallback
	end

	-- branch prediction hints
	configvar_check_cxxsnippets(
		"RAYNE_EXPECT_TRUE=__builtin_expect(!!(x), 1)",
		"int f(int x){ return __builtin_expect(!!x, 0) ? 1 : 0; } int main(){ return f(0); }",
		{quote = false, default = "(x)"}
	)
	configvar_check_cxxsnippets(
		"RAYNE_EXPECT_FALSE=__builtin_expect(!!(x), 0)",
		"int f(int x){ return __builtin_expect(!!x, 0) ? 1 : 0; } int main(){ return f(0); }",
		{quote = false, default = "(x)"}
	)
end

local function set_exports_for_platform()
	if is_plat("windows", "mingw") then
		val_export = "__declspec(dllexport)"
		val_import = "__declspec(dllimport)"
	else
		val_export = "__attribute__((visibility(\"default\")))"
		val_import = "__attribute__((visibility(\"default\")))"
	end
	set_configvar("RAYNE_RNAPI_EXPORT", val_export, {quote = false})
	set_configvar("RAYNE_RNAPI_IMPORT", val_import, {quote = false})
end

local function set_platform_flags()
	local is_macos = is_plat("macosx")
	local is_ios = is_plat("iphoneos", "iphonesimulator")
	local is_visionos = is_plat("applexros")
	local is_android = is_plat("android")
	local is_linux = is_plat("linux")
	local is_win = is_plat("windows", "mingw")

	set_configvar("RAYNE_PLATFORM_OSX", is_macos and 1 or 0)
	set_configvar("RAYNE_PLATFORM_IOS", is_ios and 1 or 0)
	set_configvar("RAYNE_PLATFORM_VISIONOS", is_visionos and 1 or 0)
	set_configvar("RAYNE_PLATFORM_WINDOWS", is_win and 1 or 0)
	set_configvar("RAYNE_PLATFORM_LINUX", is_linux and 1 or 0)
	set_configvar("RAYNE_PLATFORM_ANDROID", is_android and 1 or 0)
	set_configvar("RAYNE_PLATFORM_POSIX", (is_macos or is_ios or is_visionos or is_linux or is_android) and 1 or 0)
end

function rayne_apply_config()
	set_version()
	set_cxx20_basics()
	detect_headers()
	detect_attributes()
	detect_signature_and_expect()
	set_exports_for_platform()
	set_platform_flags()
	set_configvar("RAYNE_HAS_VTUNE", 0)
	set_configvar("RAYNE_ENABLE_VTUNE", 0)
end

