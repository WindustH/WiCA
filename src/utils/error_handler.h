#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <string>
#include <SDL2/SDL_error.h> // For SDL_GetError()

// Provides a centralized way to handle and log errors.
namespace ErrorHandler {

    /**
     * @brief Logs a general error message.
     * @param message The error message to log.
     * @param isFatal If true, the function may choose to terminate the application (e.g., by throwing an exception or calling exit).
     * For now, it just indicates severity in the log.
     */
    void logError(const std::string& message, bool isFatal = false);

    /**
     * @brief Logs an error specific to an SDL function call.
     * It appends the SDL error message obtained via SDL_GetError().
     * @param context A string describing the context in which the SDL error occurred (e.g., "SDL_Init failed").
     * @param isFatal If true, indicates a fatal error.
     */
    void sdlError(const std::string& context, bool isFatal = false);

} // namespace ErrorHandler

#endif // ERROR_HANDLER_H