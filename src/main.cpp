#include "core/application.h"
#include "utils/error_handler.h" // For logging any early errors
#include <iostream> // For standard output

#ifdef _WIN32
#include <windows.h> // For FreeConsole if we want to hide console on Windows release
#endif

// Standard main entry point
int main(int argc, char* argv[]) {

    // Basic command line argument parsing (optional)
    std::string configFilePath = "rules/rgb.json"; // Default config path
    if (argc > 1) {
        // Example: Allow specifying a config file as the first argument
        // For more complex args, use a library like cxxopts or getopt
        configFilePath = argv[1];
        std::cout << "Using custom configuration file: " << configFilePath << std::endl;
    }


    // On Windows, for a release build, you might want to hide the console window
    // that appears by default with MinGW or some MSVC setups for GUI apps.
    // #if defined(_WIN32) && !defined(_DEBUG) // Only for Windows Release builds
    //  FreeConsole();
    // #endif


    ErrorHandler::logError("Application starting...", false); // Log start

    Application app;

    if (app.initialize(configFilePath)) {
        app.run();
    } else {
        ErrorHandler::logError("Application failed to initialize. Exiting.", true);
        // SDL_ShowSimpleMessageBox might be useful here if SDL video is up
        // but for early init failures, console/log is primary.
        #ifdef _WIN32
             MessageBoxA(NULL, "Application failed to initialize. Check logs.", "Fatal Error", MB_OK | MB_ICONERROR);
        #else
             std::cerr << "FATAL ERROR: Application failed to initialize. Check logs." << std::endl;
        #endif
        return 1; // Indicate an error
    }

    ErrorHandler::logError("Application exited normally.", false); // Log normal exit
    return 0; // Indicate success
}
