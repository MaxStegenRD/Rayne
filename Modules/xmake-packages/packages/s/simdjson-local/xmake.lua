package("simdjson-local")
    set_homepage("https://github.com/simdjson/simdjson")
    set_description("Fast JSON parser for modern C++ (local, Android-friendly build)")
    set_license("Apache-2.0")

    add_urls("https://github.com/simdjson/simdjson.git")
    add_versions("v4.2.0", "v4.2.0")

    add_deps("cmake")

    on_install(function (package)
        import("package.tools.cmake")
        local configs = {
            "-DSIMDJSON_BUILD_TESTS=OFF",
            "-DSIMDJSON_ENABLE_THREADS=ON",
            "-DSIMDJSON_SANITIZE=OFF"
        }
        cmake.install(package, configs)
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            #include <simdjson.h>
            void test() {
                simdjson::ondemand::parser parser;
            }
        ]]}, {configs = {languages = "c++20"}}))
    end)


