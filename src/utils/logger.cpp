#include "logger.h"
#include "spdlog/sinks/stdout_color_sinks.h" // For console sink
#include "spdlog/sinks/basic_file_sink.h"   // For file sink
#include <iostream> // For std::cerr in case of logging init failure

namespace Logging {

// Static storage for loggers and sinks
static std::unordered_map<Module, std::shared_ptr<spdlog::logger>> s_Loggers;
static std::vector<spdlog::sink_ptr> s_Sinks;

std::string ModuleToString(Module module) {
    switch (module) {
        case Module::Core:            return "Core";
        case Module::Renderer:        return "Renderer";
        case Module::Input:           return "Input";
        case Module::UI:              return "UI";
        case Module::CommandParser:   return "CommandParser";
        case Module::CellSpace:       return "CellSpace";
        case Module::RuleEngine:      return "RuleEngine";
        case Module::Snapshot:        return "Snapshot";
        case Module::FileIO:          return "FileIO";
        case Module::Utils:           return "Utils";
        case Module::Main:            return "Main";
        case Module::ErrorHandler:    return "ErrorHandler";
        case Module::Rule:            return "Rule";
        case Module::Huffman:         return "Huffman";
        default:                      return "Unknown";
    }
}

void Init(const std::string& logFilePath, spdlog::level::level_enum globalLevel) {
    try {
        s_Sinks.clear();
        s_Loggers.clear();

        // Console sink: [Timestamp] [LoggerName] [Level] Message (colored)
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
        s_Sinks.push_back(console_sink);

        // File sink: [Timestamp] [LoggerName] [Level] Message
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true); // true to truncate file on open
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
        s_Sinks.push_back(file_sink);

        // Create loggers for all defined modules
        // This requires manually listing all enum values.
        std::vector<Module> all_modules = {
            Module::Core, Module::Renderer, Module::Input, Module::UI,
            Module::CommandParser, Module::CellSpace, Module::RuleEngine,
            Module::Snapshot, Module::FileIO, Module::Utils, Module::Main,
            Module::ErrorHandler, Module::Rule, Module::Huffman
        };

        for (Module mod : all_modules) {
            std::string logger_name = ModuleToString(mod);
            auto logger = std::make_shared<spdlog::logger>(logger_name, begin(s_Sinks), end(s_Sinks));
            logger->set_level(globalLevel); // Set initial level
            spdlog::register_logger(logger); // Register with spdlog factory
            s_Loggers[mod] = logger;
        }

        // Set global flush level (e.g., flush on error and critical)
        spdlog::flush_on(spdlog::level::err);

        // Initial log message from the Main logger
        auto main_logger = GetLogger(Module::Main);
        if (main_logger) {
            main_logger->info("Logging system initialized. Log file: '{}'. Global level: {}", logFilePath, spdlog::level::to_string_view(globalLevel).data());
        } else {
            // This should ideally not happen if Main module is in all_modules
            spdlog::critical("Failed to get Main logger during Logging::Init. Logging setup might be incomplete.");
        }

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log system initialization failed: " << ex.what() << std::endl;
        // Depending on the application, this could be a critical failure.
        // For now, we just output to cerr.
    }
}

std::shared_ptr<spdlog::logger> GetLogger(Module module) {
    auto it = s_Loggers.find(module);
    if (it != s_Loggers.end()) {
        return it->second;
    }
    // Fallback for when a logger for an unregistered/unknown module is requested.
    // This might happen if Logging::Init wasn't called or a new Module enum value was added
    // but not updated in the `all_modules` vector in `Init`.
    std::string module_name = ModuleToString(module); // Get name even if unknown
    std::cerr << "CRITICAL: Logger for module '" << module_name << "' not found. Logging::Init might be incomplete or module is new." << std::endl;

    // Optionally, create and register a logger on-the-fly for the unknown module,
    // or return a default "unknown" logger. For now, returning nullptr to indicate error.
    // If a logger is created here, ensure s_Sinks is initialized.
    if (s_Sinks.empty()) {
         std::cerr << "CRITICAL: Logging sinks are not initialized. Cannot create fallback logger for '" << module_name << "'." << std::endl;
         return nullptr;
    }
    try {
        std::string unknown_logger_name = "UnknownModule_" + module_name;
        auto logger = std::make_shared<spdlog::logger>(unknown_logger_name, begin(s_Sinks), end(s_Sinks));
        logger->set_level(spdlog::level::warn); // Default to warn for unknown modules
        spdlog::register_logger(logger);
        s_Loggers[module] = logger; // Cache it for future calls
        logger->error("Logger for module '{}' was not pre-initialized. Created on-the-fly.", module_name);
        return logger;
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "CRITICAL: Failed to create fallback logger for module '" << module_name << "': " << ex.what() << std::endl;
        return nullptr;
    }
}

void SetLevel(Module module, spdlog::level::level_enum level) {
    auto logger = GetLogger(module); // GetLogger now handles unknown modules better
    if (logger) {
        logger->set_level(level);
        // Log level change confirmation at info level, respecting the new level.
        // If new level is > info, this message won't show from this specific logger, which is fine.
        logger->log(spdlog::source_loc{}, spdlog::level::info, "Log level set to: {}", spdlog::level::to_string_view(level).data());
    } else {
        // GetLogger already logged an error if it couldn't provide a logger.
        // No need for redundant error message here unless more context is needed.
    }
}

void SetGlobalLevel(spdlog::level::level_enum level) {
    for (auto const& pair : s_Loggers) {
        if (pair.second) {
            pair.second->set_level(level);
        }
    }
    // Use a known logger (e.g., Main) to announce the global change,
    // or iterate and log for each if verbose confirmation is desired.
    auto main_logger = GetLogger(Module::Main);
    if (main_logger) {
         main_logger->info("Global log level set to: {}", spdlog::level::to_string_view(level).data());
    } else {
        // If main logger isn't available, output to cerr.
        std::cerr << "Global log level set to: " << spdlog::level::to_string_view(level).data() << " (Main logger unavailable for announcement)" << std::endl;
    }
}

} // namespace Logging
