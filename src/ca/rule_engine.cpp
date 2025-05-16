#include "rule_engine.h"
#include <set>
#include "../utils/logger.h" // New logger
#include <filesystem>   // For path manipulation (C++17)
#include <unordered_map> // Make sure it's included

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

bool RuleEngine::loadRuleLibrary(const std::string& dllPathBaseFromConfig, const std::string& functionName) {
    auto logger = Logging::GetLogger(Logging::Module::RuleEngine);
    unloadRuleLibrary();
    if (logger) logger->info("Start to load DLL.");

    if (dllPathBaseFromConfig.empty() || functionName.empty()) {
        if (logger) logger->error("DLL path or function name is empty. Cannot load library.");
        return false;
    }

    std::string actualDllPathLoaded;

#ifdef _WIN32
    std::string pathAttempt1 = dllPathBaseFromConfig + ".dll";
    dllHandle_ = LoadLibraryA(pathAttempt1.c_str());

    if (!dllHandle_) {
        std::filesystem::path p(dllPathBaseFromConfig);
        std::string dir = p.has_parent_path() ? p.parent_path().generic_string() : "";
        std::string filename = p.filename().string();
        std::string pathAttempt2;
        if (!dir.empty()) {
            pathAttempt2 = dir + "/lib" + filename + ".dll";
        } else {
            pathAttempt2 = "lib" + filename + ".dll";
        }

        dllHandle_ = LoadLibraryA(pathAttempt2.c_str());
        if (dllHandle_) {
            actualDllPathLoaded = pathAttempt2;
        }
    } else {
        actualDllPathLoaded = pathAttempt1;
    }

    if (!dllHandle_) {
        if (logger) logger->error("Failed to load DLL (both attempts). Last error code: " + std::to_string(GetLastError()));
        return false;
    }

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-function-type"
    dllRuleFunction_ = reinterpret_cast<RuleUpdateFunction>(GetProcAddress(dllHandle_, functionName.c_str()));
    #pragma GCC diagnostic pop

    if (!dllRuleFunction_) {
        if (logger) logger->error("Failed to find function '" + functionName + "' in DLL '" + actualDllPathLoaded + "'. Error code: " + std::to_string(GetLastError()));
        FreeLibrary(dllHandle_);
        dllHandle_ = nullptr;
        return false;
    }

#else // POSIX (Linux, macOS)
    std::filesystem::path p(dllPathBaseFromConfig);
    std::string dir_str = p.has_parent_path() ? p.parent_path().generic_string() : "";
    std::string filename_str = p.filename().string();

    std::string baseNameForPosix;
    if (!dir_str.empty()) {
        if (dir_str[0] == '/' || (dir_str.length() > 1 && dir_str[1] == ':')) { // Absolute path
            baseNameForPosix = dir_str + "/lib" + filename_str;
        } else { // Relative path
            baseNameForPosix = "./" + dir_str + "/lib" + filename_str;
        }
    } else { // No directory part
        baseNameForPosix = "./lib" + filename_str;
    }

    std::string soPath = baseNameForPosix + ".so";
    dllHandle_ = dlopen(soPath.c_str(), RTLD_LAZY);
    if (dllHandle_) {
        actualDllPathLoaded = soPath;
    } else {
        std::string dylibPath = baseNameForPosix + ".dylib";
        dllHandle_ = dlopen(dylibPath.c_str(), RTLD_LAZY);
        if (dllHandle_) {
            actualDllPathLoaded = dylibPath;
        } else {
             if (logger) logger->error("Failed to load .dylib '" + dylibPath + "': " + dlerror());
             return false;
        }
    }

    dlerror(); // Clear any existing error
    dllRuleFunction_ = (RuleUpdateFunction)dlsym(dllHandle_, functionName.c_str());
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        if (logger) logger->error("Failed to find function '" + functionName + "' in library '" + actualDllPathLoaded + "': " + dlsym_error);
        dlclose(dllHandle_);
        dllHandle_ = nullptr;
        return false;
    }
#endif

    if (logger) logger->info("DLL loaded.");
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
    auto logger = Logging::GetLogger(Logging::Module::RuleEngine);
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
    auto logger = Logging::GetLogger(Logging::Module::RuleEngine);
    std::unordered_map<Point, int> cellsToUpdate;

    if (!initialized_) {
        if (logger) logger->error("Cannot calculate next generation. Not initialized.");
        return cellsToUpdate; // Return empty map
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
    return cellsToUpdate;
}

bool RuleEngine::isInitialized() const {
    return initialized_;
}
