#include "utils/logger.h"        // Your new logging system
#include "core/application.h"    // Your main application class
#include "utils/error_handler.h" // Still used for some SDL and fatal error handling

#include <iostream> // Used for emergency output if logger initialization fails
#include <string>
#include <vector>
#include <stdexcept> // For std::exception

// Map module names to Logging::Module enum values, useful for setting log levels from command line or config
std::unordered_map<std::string, Logging::Module> g_moduleNameMap = {
    {"core", Logging::Module::Core},
    {"renderer", Logging::Module::Renderer},
    {"input", Logging::Module::Input},
    {"ui", Logging::Module::UI},
    {"commandparser", Logging::Module::CommandParser},
    {"cellspace", Logging::Module::CellSpace},
    {"ruleengine", Logging::Module::RuleEngine},
    {"snapshotmanager", Logging::Module::SnapshotManager},
    {"fileio", Logging::Module::FileIO},
    {"utils", Logging::Module::Utils},
    {"main", Logging::Module::Main},
    {"errorhandler", Logging::Module::ErrorHandler},
    {"config", Logging::Module::Rule},
    {"huffman", Logging::Module::Huffman}
};

// Map string level names to spdlog level enums
std::unordered_map<std::string, spdlog::level::level_enum> g_logLevelNameMap = {
    {"trace", spdlog::level::trace},
    {"debug", spdlog::level::debug},
    {"info", spdlog::level::info},
    {"warn", spdlog::level::warn},
    {"warning", spdlog::level::warn}, // alias
    {"error", spdlog::level::err},
    {"critical", spdlog::level::critical},
    {"off", spdlog::level::off}
};

/**
 * @brief Parses command-line arguments to set log levels.
 * Example arguments:
 * -loglevel:global=debug
 * -loglevel:renderer=trace
 * -loglevel:cellspace=info
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @param main_logger Main logger for logging during parsing
 */
void ParseCommandLineLogLevels(int argc, char* argv[], std::shared_ptr<spdlog::logger> main_logger) {
    const std::string argPrefix = "-loglevel:";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind(argPrefix, 0) == 0) { // Check if argument starts with "-loglevel:"
            std::string moduleAndLevel = arg.substr(argPrefix.length());
            size_t separatorPos = moduleAndLevel.find('=');
            if (separatorPos != std::string::npos) {
                std::string moduleName = moduleAndLevel.substr(0, separatorPos);
                std::string levelName = moduleAndLevel.substr(separatorPos + 1);

                // Convert to lowercase for case-insensitive matching
                std::transform(moduleName.begin(), moduleName.end(), moduleName.begin(), ::tolower);
                std::transform(levelName.begin(), levelName.end(), levelName.begin(), ::tolower);

                auto levelIt = g_logLevelNameMap.find(levelName);
                if (levelIt == g_logLevelNameMap.end()) {
                    if (main_logger) main_logger->warn("Invalid log level name '{}' from command line argument '{}'.", levelName, arg);
                    continue;
                }
                spdlog::level::level_enum logLevel = levelIt->second;

                if (moduleName == "global") {
                    Logging::SetGlobalLevel(logLevel);
                    if (main_logger) main_logger->info("Global log level set via command line to: {}.", levelName);
                } else {
                    auto moduleIt = g_moduleNameMap.find(moduleName);
                    if (moduleIt != g_moduleNameMap.end()) {
                        Logging::SetLevel(moduleIt->second, logLevel);
                        if (main_logger) main_logger->info("Log level for module '{}' set via command line to: {}.", moduleName, levelName);
                    } else {
                        if (main_logger) main_logger->warn("Unknown module name '{}' from command line argument '{}'.", moduleName, arg);
                    }
                }
            } else {
                if (main_logger) main_logger->warn("Malformed log level argument: '{}'. Expected format: -loglevel:module_name=level_name", arg);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    Logging::Init("windcell.log", spdlog::level::info);
    auto main_logger = Logging::GetLogger(Logging::Module::Main);
    if (!main_logger) {
        std::cerr << "Critical error: Failed to initialize main logger. Exiting." << std::endl;
        return 1;
    }

    main_logger->info("Application startup...");
    main_logger->debug("Number of arguments passed: {}", argc);
    if (main_logger->should_log(spdlog::level::trace)) { // Only log all args at trace level
        for(int i=0; i < argc; ++i) {
            main_logger->trace("argv[{}]: {}", i, argv[i]);
        }
    }

    // 2. Parse command line for log level settings
    ParseCommandLineLogLevels(argc, argv, main_logger);

    // 3. Create and run the application instance
    main_logger->info("Creating Application instance...");
    std::unique_ptr<Application> app;
    try {
        app = std::make_unique<Application>();
        main_logger->info("Application instance created successfully.");
    } catch (const std::exception& e) {
        main_logger->critical("Exception occurred while creating Application instance: {}", e.what());
        Logging::Init("emergency_shutdown.log", spdlog::level::err); // Try to re-init logger to emergency file
        auto emerg_logger = Logging::GetLogger(Logging::Module::Main);
        if(emerg_logger) emerg_logger->critical("Application creation failed: {}", e.what());
        else std::cerr << "EMERGENCY: Application creation failed and emergency logger also failed: " << e.what() << std::endl;
        spdlog::shutdown(); // Attempt to flush pending logs
        return 1; // Exit
    } catch (...) {
        main_logger->critical("Unknown exception type occurred while creating Application instance.");
        Logging::Init("emergency_shutdown.log", spdlog::level::err);
        auto emerg_logger = Logging::GetLogger(Logging::Module::Main);
        if(emerg_logger) emerg_logger->critical("Application creation failed due to unknown exception type.");
        else std::cerr << "EMERGENCY: Application creation failed (unknown exception) and emergency logger also failed." << std::endl;
        spdlog::shutdown();
        return 1;
    }

    std::string configFilePath="rules/rgb.json";
    if (app) {
        app->initialize(configFilePath);
        app->run();
    } else {
        main_logger->critical("Application instance was not created. Cannot run.");
    }

    // spdlog will automatically flush and clean up registered loggers on program exit,
    // but calling spdlog::shutdown() explicitly is a good practice, especially after app->run()
    spdlog::shutdown();

    return 0;
}