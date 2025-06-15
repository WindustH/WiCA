#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h" // For potential custom type logging via operator<<

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Logger {

// Enum for different logger modules, can be expanded
enum class Module {
    Core,
    Renderer,
    Input,
    UI,
    CommandParser,
    CellSpace,
    RuleEngine,
    Snapshot,
    FileIO,
    Utils,
    Main,
    ErrorHandler,
    Rule,
    Huffman
};

/**
 * @brief Initializes the logging system with console and file sinks.
 * Creates named loggers for each module defined in the Module enum.
 * @param logFilePath Path to the output log file.
 * @param globalLevel Initial global log level for all loggers.
 */
void initialize(const std::string& logFilePath = "application.log", spdlog::level::level_enum globalLevel = spdlog::level::trace);

/**
 * @brief Retrieves a logger for a specific module.
 * @param module The logger module.
 * @return A shared_ptr to the spdlog::logger, or nullptr if not found.
 */
std::shared_ptr<spdlog::logger> getLogger(Module module);

/**
 * @brief Sets the logging level for a specific module.
 * @param module The logger module.
 * @param level The spdlog level to set.
 */
void setLevel(Module module, spdlog::level::level_enum level);

/**
 * @brief Sets the logging level for all registered loggers.
 * @param level The spdlog level to set globally.
 */
void setGlobalLevel(spdlog::level::level_enum level);

/**
 * @brief Converts a Module enum to its string representation (logger name).
 * @param module The logger module.
 * @return String name of the module.
 */
std::string ModuleToString(Module module);

} // namespace Logging
