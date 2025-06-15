set_project("WiCA")
set_version("1.0")
set_description("WiCA")
set_languages("cxx20")


set_warnings("all", "extra")
if is_plat("msvc") then
    add_cxflags("/W4")
end


add_requires("sdl2", "sdl2_image", "sdl2_ttf", "nlohmann_json", "spdlog", "fmt", "tbb")

target("WiCA")
    set_kind("binary")
    add_files(
        "src/main.cpp",
        "src/core/application.cpp",
        "src/core/rule.cpp",
        "src/ca/cell_space.cpp",
        "src/ca/rule_engine.cpp",
        "src/render/renderer.cpp",
        "src/render/viewport.cpp",
        "src/input/input_handler.cpp",
        "src/input/command_parser.cpp",
        "src/snap/snapshot.cpp",
        "src/snap/huffman_coding.cpp",
        "src/utils/error_handler.cpp",
        "src/utils/logger.cpp",
        "src/utils/timer.cpp"
    )
    add_includedirs("src")
    add_packages("sdl2", "sdl2_image", "sdl2_ttf", "nlohmann_json", "spdlog", "fmt", "tbb")
    after_build(function (target)
        print("Copying assets and rules to build directory...")
        os.cp("assets", target:targetdir())
        os.cp("rules", target:targetdir())
    end)
	set_targetdir("$(buildir)")

rule("plugin")
    on_load(
		function(target)
			target:set("targetdir","$(buildir)/plugins")
    	end
	)

target("life_plugin")
    set_kind("shared")
    set_rules("plugin")
    set_filename("life")
    add_files("plugins/life.cpp")
    add_defines("LIFE_PLUGIN_EXPORTS")

target("rgb_plugin")
    set_kind("shared")
    set_rules("plugin")
    set_filename("rgb")
    add_files("plugins/rgb.cpp")
    add_defines("RGB_PLUGIN_EXPORTS")
