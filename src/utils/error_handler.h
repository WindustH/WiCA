#pragma once

#include <string>

// Provides a centralized way to handle and log errors using the new Logging system.
namespace ErrorHandler {

    /**
     * @brief Logs a general error message. Uses Logging::Module::ErrorHandler.
     * If isFatal is true, logs at CRITICAL level and then calls exit(EXIT_FAILURE).
     * Otherwise, logs at ERROR level.
     * @param message The error message to log.
     */
    void failure(const std::string& message);

} // namespace ErrorHandler
