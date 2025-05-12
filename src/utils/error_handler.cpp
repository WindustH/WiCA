#include "error_handler.h"
#include <cstdlib> // For exit, EXIT_FAILURE
#include <iostream>

namespace ErrorHandler {

    /**
     * @brief Logs a general error message to std::cerr.
     * @param message The error message to log.
     * @param isFatal If true, the function will call exit(EXIT_FAILURE) after logging.
     */
    void logError(const std::string& message, bool isFatal) {
        std::cerr << "ERROR: " << message << std::endl;
        if (isFatal) {
            // Consider more graceful shutdown if resources need cleanup
            exit(EXIT_FAILURE);
        }
    }

    /**
     * @brief Logs an error message related to an SDL function call.
     *
     * This function appends the specific SDL error message obtained via SDL_GetError()
     * to the provided context string.
     * @param context A string describing the context in which the SDL error occurred.
     * @param isFatal If true, the function will call exit(EXIT_FAILURE) after logging.
     */
    void sdlError(const std::string& context, bool isFatal) {
        std::cerr << "SDL_ERROR: " << context << " - " << SDL_GetError() << std::endl;
        if (isFatal) {
            // Consider more graceful shutdown if resources need cleanup
            // SDL_Quit() should ideally be called in a central cleanup location
            // if the program is exiting due to an SDL error after SDL_Init.
            exit(EXIT_FAILURE);
        }
    }

} // namespace ErrorHandler
