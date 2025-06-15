#include "rule_engine.h"
#include <set>
#include "../utils/logger.h" // New logger
#include <filesystem>   // For path manipulation (C++17)
#include <unordered_map> // Make sure it's included
#include "../utils/timer.h"

// Constructor
RuleEngine::RuleEngine()
    : dllHandle_(nullptr),
      dllRuleFunction_(nullptr),
      defaultState_(0),
      initialized_(false) {
}

// Destructor
RuleEngine::~RuleEngine() {
    unloadRuleLibrary();
}

// --- DLL Handling Methods ---

bool RuleEngine::loadRuleLibrary(const std::string& dllPathFromConfig, const std::string& functionName) {
    auto logger = Logger::getLogger(Logger::Module::RuleEngine);
    unloadRuleLibrary(); // Unload any previously loaded library first

    if (logger) logger->info("Attempting to load dynamic library...");

    // Validate that the path and function name from the configuration are not empty.
    if (dllPathFromConfig.empty() || functionName.empty()) {
        if (logger) logger->error("Library path or function name is empty in configuration. Cannot load library.");
        return false;
    }

#ifdef _WIN32
    // On Windows, directly use the provided path to load the DLL.
    dllHandle_ = LoadLibraryA(dllPathFromConfig.c_str());

    if (!dllHandle_) {
        if (logger) logger->error("Failed to load DLL '" + dllPathFromConfig + "'. Last error code: " + std::to_string(GetLastError()));
        return false;
    }

    // Suppress warning about casting a function pointer type, which is a common practice here.
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-function-type"
    dllRuleFunction_ = reinterpret_cast<RuleUpdateFunction>(GetProcAddress(dllHandle_, functionName.c_str()));
    #pragma GCC diagnostic pop

    if (!dllRuleFunction_) {
        if (logger) logger->error("Failed to find function '" + functionName + "' in DLL '" + dllPathFromConfig + "'. Error code: " + std::to_string(GetLastError()));
        FreeLibrary(dllHandle_); // Clean up by freeing the library if the function isn't found.
        dllHandle_ = nullptr;
        return false;
    }

#else // POSIX (Linux, macOS)
    // On POSIX systems, directly use the provided path to load the shared library (.so, .dylib).
    dllHandle_ = dlopen(dllPathFromConfig.c_str(), RTLD_LAZY);

    if (!dllHandle_) {
        if (logger) logger->error("Failed to load shared library '" + dllPathFromConfig + "'. Error: " + dlerror());
        return false;
    }

    dlerror(); // Clear any existing error before calling dlsym.
    dllRuleFunction_ = (RuleUpdateFunction)dlsym(dllHandle_, functionName.c_str());

    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        if (logger) logger->error("Failed to find function '" + functionName + "' in library '" + dllPathFromConfig + "'. Error: " + dlsym_error);
        dlclose(dllHandle_); // Clean up by closing the handle if the function isn't found.
        dllHandle_ = nullptr;
        return false;
    }
#endif

    if (logger) logger->info("Successfully loaded library '" + dllPathFromConfig + "' and found function '" + functionName + "'.");
    return true;
}

void RuleEngine::unloadRuleLibrary() {
    if (dllHandle_) {
#ifdef _WIN32
        FreeLibrary(dllHandle_);
#else
        dlclose(dllHandle_);
#endif
        dllHandle_ = nullptr;
        dllRuleFunction_ = nullptr;
    }
}


bool RuleEngine::initialize(const Rule& config) {
    auto logger = Logger::getLogger(Logger::Module::RuleEngine);
    if (logger) logger->info("Start to initialize rule engine.");
    initialized_ = false;
    unloadRuleLibrary();

    if (!config.isLoaded()) {
        if (logger) logger->error("Cannot initialize. Configuration is not loaded.");
        return false;
    }

    neighborhood_ = config.getNeighborhood();
    defaultState_ = config.getDefaultState();

    std::string dllPathFromConfig = config.getRuleDllPath();
    std::string funcName = config.getRuleFunctionName();
    if (!loadRuleLibrary(dllPathFromConfig, funcName)) {
        if (logger) logger->error("Failed to initialize in DLL mode. DLL or function not loaded.");
        return false;
    }

    if (logger) logger->info("Rule engine initialized.");
    initialized_ = true;
    return true;
}

std::unordered_map<Point, int> RuleEngine::calculateForUpdate(const CellSpace& currentCellSpace) const {
    auto logger = Logger::getLogger(Logger::Module::RuleEngine);
    auto timer = Timer::getTimer(Timer::Module::calculateForUpdate);
    timer.start();

    std::unordered_map<Point, int> cellsToUpdate;

    if (!initialized_) {
        if (logger) logger->error("Cannot calculate next generation. Not initialized.");
        return cellsToUpdate; // Return empty map
        timer.stop();
    }

    const auto& cellsToEvaluate = currentCellSpace.getCellsToEvaluate();

    for (const Point& cellCoord : cellsToEvaluate) {
        int currentCellState = currentCellSpace.getCellState(cellCoord);
        std::vector<int> neighborStatesPattern = currentCellSpace.getNeighborStates(cellCoord);

        int nextState = currentCellState;

        if (dllRuleFunction_) {
            const int* ns_data = neighborStatesPattern.empty() ? nullptr : neighborStatesPattern.data();
            nextState = dllRuleFunction_(ns_data);
        } else {
            if (logger) logger->error("DLL function pointer is null in calculateNextGeneration. Cell state will persist.");
        }

        if (nextState != currentCellState) {
            cellsToUpdate[cellCoord] = nextState; // Insert/update in the map
        }
    }
    timer.stop();
    return cellsToUpdate;
}

bool RuleEngine::isInitialized() const {
    return initialized_;
}
