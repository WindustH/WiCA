#include "utils/logger.h"        // Your new logging system
#include "core/application.h"    // Your main application class
#include "utils/timer.h"

#include <iostream> // Used for emergency output if logger initialization fails
#include <string>
#include <vector>
#include <stdexcept> // For std::exception

int main(int argc, char* argv[]) {
    Logger::initialize("windcell.log", spdlog::level::info);

    Logger::setGlobalLevel(spdlog::level::info);

    auto main_logger = Logger::getLogger(Logger::Module::Main);
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

    // 3. Create and run the application instance
    main_logger->info("Creating Application instance...");
    std::unique_ptr<Application> app;
    try {
        app = std::make_unique<Application>();
        main_logger->info("Application instance created successfully.");
    } catch (const std::exception& e) {
        main_logger->critical("Exception occurred while creating Application instance: {}", e.what());
        Logger::initialize("emergency_shutdown.log", spdlog::level::err); // Try to re-init logger to emergency file
        auto emerg_logger = Logger::getLogger(Logger::Module::Main);
        if(emerg_logger) emerg_logger->critical("Application creation failed: {}", e.what());
        else std::cerr << "EMERGENCY: Application creation failed and emergency logger also failed: " << e.what() << std::endl;
        spdlog::shutdown(); // Attempt to flush pending logs
        return 1; // Exit
    } catch (...) {
        main_logger->critical("Unknown exception type occurred while creating Application instance.");
        Logger::initialize("emergency_shutdown.log", spdlog::level::err);
        auto emerg_logger = Logger::getLogger(Logger::Module::Main);
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

    spdlog::shutdown();
    Timer::printReport();
    return 0;
}