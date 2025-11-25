package("kalligraph-local")
    set_homepage("https://github.com/Slin/Kalligraph")
    set_description("Kalligraph UI text renderer")
    set_license("MIT")

    add_urls("https://github.com/Slin/Kalligraph.git")
    add_versions("latest", "master")

    add_deps("cmake")

    add_includedirs("include")
    add_includedirs("include/Kalligraph")
    add_linkdirs("lib")
    add_links("Kalligraph")

    on_install(function (package)
        import("package.tools.cmake").install(package)
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            #include <Kalligraph/KGMeshGeneratorLoopBlinn.h>
            void test() { KG::MeshGeneratorLoopBlinn meshGenerator; }
        ]]}, {configs = {languages = "c++20"}}))
    end)

