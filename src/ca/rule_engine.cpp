#include "rule_engine.h"
#include "../utils/error_handler.h"
#include <set>
#include <iostream>     // For debugging messages
#include <filesystem>   // For path manipulation (C++17)

// Constructor
RuleEngine::RuleEngine()
    : dllHandle_(nullptr),
      dllRuleFunction_(nullptr),
      currentRuleMode_("trie"), // Default to trie
      defaultState_(0),
      initialized_(false) {
    ErrorHandler::logError("RuleEngine: Constructor called.", false);
}

// Destructor
RuleEngine::~RuleEngine() {
    ErrorHandler::logError("RuleEngine: Destructor called. Unloading library if loaded.", false);
    unloadRuleLibrary();
}

// --- DLL Handling Methods ---

bool RuleEngine::loadRuleLibrary(const std::string& dllPathBaseFromConfig, const std::string& functionName) {
    unloadRuleLibrary(); // Unload any existing library first

    if (dllPathBaseFromConfig.empty() || functionName.empty()) {
        ErrorHandler::logError("RuleEngine: DLL path or function name is empty. Cannot load library.", true);
        return false;
    }

    std::string actualDllPathLoaded; // To store the path that successfully loaded

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

        ErrorHandler::logError("RuleEngine: LoadLibraryA failed for '" + pathAttempt1 + "'. Error: " + std::to_string(GetLastError()) + ". Trying '" + pathAttempt2 + "'.", false);
        dllHandle_ = LoadLibraryA(pathAttempt2.c_str());
        if (dllHandle_) {
            actualDllPathLoaded = pathAttempt2;
        }
    } else {
        actualDllPathLoaded = pathAttempt1;
    }

    if (!dllHandle_) {
        ErrorHandler::logError("RuleEngine: Failed to load DLL (both attempts). Last error code: " + std::to_string(GetLastError()), true);
        return false;
    }

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-function-type"
    dllRuleFunction_ = reinterpret_cast<RuleUpdateFunction>(GetProcAddress(dllHandle_, functionName.c_str()));
    #pragma GCC diagnostic pop

    if (!dllRuleFunction_) {
        ErrorHandler::logError("RuleEngine: Failed to find function '" + functionName + "' in DLL '" + actualDllPathLoaded + "'. Error code: " + std::to_string(GetLastError()), true);
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
        if (dir_str[0] == '/' || (dir_str.length() > 1 && dir_str[1] == ':')) {
            baseNameForPosix = dir_str + "/lib" + filename_str;
        } else {
            baseNameForPosix = "./" + dir_str + "/lib" + filename_str;
        }
    } else {
        baseNameForPosix = "./lib" + filename_str;
    }

    std::string soPath = baseNameForPosix + ".so";
    dllHandle_ = dlopen(soPath.c_str(), RTLD_LAZY);
    if (dllHandle_) {
        actualDllPathLoaded = soPath;
    } else {
        std::string dylibPath = baseNameForPosix + ".dylib";
        ErrorHandler::logError("RuleEngine: Failed to load .so '" + soPath + "': " + dlerror() + ". Trying .dylib '" + dylibPath + "'", false);
        dllHandle_ = dlopen(dylibPath.c_str(), RTLD_LAZY);
        if (dllHandle_) {
            actualDllPathLoaded = dylibPath;
        } else {
             ErrorHandler::logError("RuleEngine: Failed to load .dylib '" + dylibPath + "': " + dlerror(), true);
             return false;
        }
    }

    dlerror();
    dllRuleFunction_ = (RuleUpdateFunction)dlsym(dllHandle_, functionName.c_str());
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        ErrorHandler::logError("RuleEngine: Failed to find function '" + functionName + "' in library '" + actualDllPathLoaded + "': " + dlsym_error, true);
        dlclose(dllHandle_);
        dllHandle_ = nullptr;
        return false;
    }
#endif
    ErrorHandler::logError("RuleEngine: Successfully loaded library '" + actualDllPathLoaded + "' and function '" + functionName + "'.", false);
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
        ErrorHandler::logError("RuleEngine: DLL unloaded.", false);
    }
}

// --- Main Methods ---

bool RuleEngine::initialize(const Rule& config) {
    ErrorHandler::logError("RuleEngine: Initializing...", false);
    initialized_ = false;
    unloadRuleLibrary(); // Ensure any previous DLL is unloaded before setting new mode/rules

    if (!config.isLoaded()) {
        ErrorHandler::logError("RuleEngine: Cannot initialize. Configuration is not loaded.", true);
        return false;
    }

    // Cache common parameters
    currentRuleMode_ = config.getRuleMode();
    neighborhoodDefinition_ = config.getNeighborhood();
    defaultState_ = config.getDefaultState();
    // possibleStates_ = config.getStates(); // Cached but not currently used by logic

    ErrorHandler::logError("RuleEngine: Mode set to '" + currentRuleMode_ + "'.", false);

    if (currentRuleMode_ == "dll") {
        std::string dllPathFromConfig = config.getRuleDllPath();
        std::string funcName = config.getRuleFunctionName();
        ErrorHandler::logError("RuleEngine: Attempting to load DLL: PathBase='" + dllPathFromConfig + "', Function='" + funcName + "'.", false);
        if (!loadRuleLibrary(dllPathFromConfig, funcName)) {
            ErrorHandler::logError("RuleEngine: Failed to initialize in DLL mode. DLL or function not loaded.", true);
            return false;
        }
        ruleTrie_ = Trie(); // Ensure Trie is in a clean default state if not used
    } else if (currentRuleMode_ == "trie") {
        ruleTrie_ = Trie(); // Create a new Trie for "trie" mode
        const auto& rules = config.getStateUpdateRules();
        if (neighborhoodDefinition_.empty() && !rules.empty()) {
            ErrorHandler::logError("RuleEngine: Trie mode - Neighborhood is empty, but rules are defined. This may lead to incorrect behavior.", true);
        }
        unsigned int rulesLoadedCount = 0;
        for (const auto& ruleVector : rules) {
            if (ruleVector.empty()) {
                ErrorHandler::logError("RuleEngine: Encountered an empty rule vector in config (Trie mode). Skipping.", false);
                continue;
            }
            // Each ruleVector should contain N neighbor states + 1 result state.
            // So, its size should be neighborhoodDefinition_.size() + 1.
            if (ruleVector.size() != (neighborhoodDefinition_.size() + 1) ) {
                ErrorHandler::logError("RuleEngine: Rule size mismatch (Trie mode). Expected " +
                                       std::to_string(neighborhoodDefinition_.size() + 1) +
                                       " elements (neighborhood pattern + result), but got " + std::to_string(ruleVector.size()) + ". Skipping rule.", false);
                continue;
            }
            std::vector<int> rulePrefix(ruleVector.begin(), ruleVector.end() - 1); // The N neighbor states
            int resultState = ruleVector.back();
            ruleTrie_.insertRule(rulePrefix, resultState);
            rulesLoadedCount++;
        }
        ErrorHandler::logError("RuleEngine: Initialized in Trie mode. Rules processed/loaded into Trie: " + std::to_string(rulesLoadedCount) + "/" + std::to_string(rules.size()), false);
    } else {
        ErrorHandler::logError("RuleEngine: Unknown rule mode: '" + currentRuleMode_ + "'. Cannot initialize rules.", true);
        return false;
    }

    initialized_ = true;
    ErrorHandler::logError("RuleEngine: Initialization successful.", false);
    return true;
}

std::vector<std::pair<Point, int>> RuleEngine::calculateNextGeneration(const CellSpace& currentCellSpace) const {
    std::vector<std::pair<Point, int>> cellsToUpdate;

    if (!initialized_) {
        ErrorHandler::logError("RuleEngine: Cannot calculate next generation. Not initialized.", true);
        return cellsToUpdate;
    }

    std::set<Point> cellsToEvaluate;
    const auto& activeCells = currentCellSpace.getActiveCells();

    // Determine the set of cells to evaluate: active cells and their neighbors
    for (const auto& pair : activeCells) {
        cellsToEvaluate.insert(pair.first); // Add the active cell itself
        for (const Point& offset : neighborhoodDefinition_) {
            // The neighborhoodDefinition_ from config defines the relative positions of neighbors.
            // If it includes [0,0] (like in default_rules.json), the active cell will be added again via offset,
            // which is fine for std::set. Otherwise, it adds only the true neighbors.
            cellsToEvaluate.insert(pair.first + offset);
        }
    }

    // If activeCells is empty, cellsToEvaluate will also be empty.
    // This is generally correct; an empty GOL grid stays empty.

    for (const Point& cellCoord : cellsToEvaluate) {
        int currentCellState = currentCellSpace.getCellState(cellCoord);
        std::vector<int> neighborStatesPattern = currentCellSpace.getNeighborStates(cellCoord, neighborhoodDefinition_);

        int nextState = currentCellState; // Default to no change

        if (currentRuleMode_ == "dll") {
            if (dllRuleFunction_) {
                const int* ns_data = neighborStatesPattern.empty() ? nullptr : neighborStatesPattern.data();
                nextState = dllRuleFunction_(ns_data);
            } else {
                ErrorHandler::logError("RuleEngine: DLL function pointer is null in calculateNextGeneration. Cell state will persist.", true);
                // nextState remains currentCellState
            }
        } else if (currentRuleMode_ == "trie") {
            int nextStateFromRule = ruleTrie_.findNextState(neighborStatesPattern);

            if (nextStateFromRule != NO_RULE_FOUND) {
                nextState = nextStateFromRule;
            } else {
                nextState = currentCellState;
            }
        }

        if (nextState != currentCellState) {
            cellsToUpdate.emplace_back(cellCoord, nextState);
        }
    }
    return cellsToUpdate;
}

bool RuleEngine::isInitialized() const {
    return initialized_;
}
