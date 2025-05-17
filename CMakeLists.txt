# Minimum CMake version required
cmake_minimum_required(VERSION 3.16)

# Project Name
project(WiCA VERSION 1.0 DESCRIPTION "WiCA")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default compiler warning flags (cross-platform)
if(MSVC)
  # /W4 is a high warning level, consider /Wall for all warnings
  # /WX treats warnings as errors (optional, but good for CI)
  add_compile_options(/W4) # Consider adding /WX
else() # Assuming GCC or Clang compatible
  add_compile_options(-Wall -Wextra -Wpedantic) # Consider adding -Werror
  # For Clang, you might also consider: -Wno-unused-parameter if it's noisy
endif()

# Define the desired output directory for plugins
# This places plugins into build/plugins (relative to CMAKE_BINARY_DIR)
set(PLUGIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/plugins)

# Set default output directories (can be overridden per target)
# Executables go into the root of the build directory (e.g., build/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# For .lib files on Windows (import libraries or static libs), .a on Linux/macOS
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# For .so/.dylib (if not handled by RUNTIME_OUTPUT_DIRECTORY for plugins on Windows)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)


# Find necessary packages
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(TBB REQUIRED)

# Define project root for clarity
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(PROJECT_SOURCES
    ${PROJECT_ROOT}/src/main.cpp
    ${PROJECT_ROOT}/src/core/application.cpp
    ${PROJECT_ROOT}/src/core/rule.cpp
    ${PROJECT_ROOT}/src/ca/cell_space.cpp
    ${PROJECT_ROOT}/src/ca/rule_engine.cpp
    ${PROJECT_ROOT}/src/render/renderer.cpp
    ${PROJECT_ROOT}/src/render/viewport.cpp
    ${PROJECT_ROOT}/src/input/input_handler.cpp
    ${PROJECT_ROOT}/src/input/command_parser.cpp
    ${PROJECT_ROOT}/src/snap/snapshot.cpp
    ${PROJECT_ROOT}/src/snap/huffman_coding.cpp
    ${PROJECT_ROOT}/src/utils/error_handler.cpp
    ${PROJECT_ROOT}/src/utils/logger.cpp
    ${PROJECT_ROOT}/src/utils/timer.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# Set include directories for the main application
# Modern CMake prefers target_include_directories for better encapsulation.
# SDL2::SDL2, SDL2_image::SDL2_image, SDL2_ttf::SDL2_ttf, and nlohmann_json::nlohmann_json
# imported targets should automatically provide their include directories when linked.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_ROOT}/src
    # Explicitly adding ${SDL2_INCLUDE_DIRS}, etc., might be redundant if
    # the find_package modules provide imported targets that carry this information.
    # If linking SDL2::SDL2 (for example) is not enough, uncomment these.
    # ${SDL2_INCLUDE_DIRS}
    # ${SDL2_IMAGE_INCLUDE_DIRS}
    # ${SDL2_TTF_INCLUDE_DIRS}
    # ${nlohmann_json_INCLUDE_DIRS} # Usually provided by nlohmann_json::nlohmann_json target
)

# Set output directory for the main executable
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2::SDL2main
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
    nlohmann_json::nlohmann_json
    fmt::fmt
    spdlog::spdlog
    TBB::tbb
)

# --- Plugin Compilation ---
# Define the source files for the Game of Life plugin
set(LIFE_PLUGIN_SOURCES
    ${PROJECT_ROOT}/plugins/life.cpp
)
add_library(life_plugin SHARED ${LIFE_PLUGIN_SOURCES})
target_compile_definitions(life_plugin PRIVATE LIFE_PLUGIN_EXPORTS)
set_target_properties(life_plugin PROPERTIES
    OUTPUT_NAME "life"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
)

set(RGB_PLUGIN_SOURCES
    ${PROJECT_ROOT}/plugins/rgb.cpp
)
add_library(rgb_plugin SHARED ${RGB_PLUGIN_SOURCES})
target_compile_definitions(rgb_plugin PRIVATE RGB_PLUGIN_EXPORTS)
set_target_properties(rgb_plugin PROPERTIES
    OUTPUT_NAME "rgb"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
)

# On non-Windows platforms, ensure the library prefix is empty if you want "life.so"
# instead of "liblife.so". This is often desired for plugins.
if(NOT WIN32)
    set_target_properties(life_plugin PROPERTIES PREFIX "")
    # You might also want to ensure the correct suffix if not default .so/.dylib
    # set_target_properties(life_plugin PROPERTIES SUFFIX ".plugin") # Example
endif()


# --- Copy Assets (for development build) ---
# These commands copy assets to the build directory when the main executable is built.
# For a proper installation, see the install() commands below.
set(ASSETS_SOURCE_DIR ${PROJECT_ROOT}/assets)
set(ASSETS_DEST_DIR ${CMAKE_BINARY_DIR}/assets) # This is build/assets

if(EXISTS ${ASSETS_SOURCE_DIR})
    # Ensure the base assets destination directory exists
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ASSETS_DEST_DIR}
        COMMENT "Ensuring assets directory exists in build"
    )
    # Copy everything from the source assets directory
    # to the destination assets directory.
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${ASSETS_SOURCE_DIR}/ ${ASSETS_DEST_DIR}/ # Note the trailing slashes
        COMMENT "Copying base assets directory to build directory"
    )
else()
    message(WARNING "Assets source directory not found at ${ASSETS_SOURCE_DIR}")
endif()

# --- Copy Rules (for development build) ---
# These commands copy rules to the build directory when the main executable is built.
# For a proper installation, see the install() commands below.
set(RULES_SOURCE_DIR ${PROJECT_ROOT}/rules)
set(RULES_DEST_DIR ${CMAKE_BINARY_DIR}/rules) # This is build/rules

if(EXISTS ${RULES_SOURCE_DIR})
    # Ensure the base rules destination directory exists
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RULES_DEST_DIR}
        COMMENT "Ensuring rules directory exists in build"
    )
    # Copy everything from the source rules directory
    # to the destination rules directory.
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${RULES_SOURCE_DIR}/ ${RULES_DEST_DIR}/ # Note the trailing slashes
        COMMENT "Copying base rules directory to build directory"
    )
else()
    message(WARNING "Rules source directory not found at ${RULES_SOURCE_DIR}")
endif()

add_custom_target(copy_rules ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RULES_DEST_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${RULES_SOURCE_DIR}/ ${RULES_DEST_DIR}/
    COMMENT "Copying rules to build directory"
)

add_dependencies(${PROJECT_NAME} copy_rules)

message(STATUS "Project Root: ${PROJECT_ROOT}")
message(STATUS "Build Directory: ${CMAKE_BINARY_DIR}")
message(STATUS "Plugin Output Directory: ${PLUGIN_OUTPUT_DIR}")
message(STATUS "Assets Source Directory: ${ASSETS_SOURCE_DIR}")
message(STATUS "Assets Destination (Build): ${ASSETS_DEST_DIR}")
message(STATUS "Rules Source Directory: ${RULES_SOURCE_DIR}")
message(STATUS "Rules Destination (Build): ${RULES_DEST_DIR}")

if(CMAKE_INSTALL_PREFIX)
    message(STATUS "Installation Prefix: ${CMAKE_INSTALL_PREFIX}")
else()
    message(STATUS "Installation Prefix: Not set (defaults will apply if installing)")
endif()