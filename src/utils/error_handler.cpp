#include "error_handler.h"
#include "logger.h" // Use the new logger
#include <cstdlib> // For exit, EXIT_FAILURE
#include <SDL2/SDL_error.h> // For SDL_GetError()
#include <iostream>

namespace ErrorHandler {

    void failure(const std::string& message) {
        // Get the dedicated logger for ErrorHandler messages
        auto logger = Logging::GetLogger(Logging::Module::ErrorHandler);

        if (!logger) {
            // Fallback to std::cerr if logger is somehow unavailable (should not happen after Init)
            std::cerr << "FATAL ERROR (logger unavailable): " << message << std::endl;
            exit(EXIT_FAILURE);
            return;
        }
        logger->critical(message);
        exit(EXIT_FAILURE);
    }

} // namespace ErrorHandler
