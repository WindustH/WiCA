#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include <vector>
#include <string>
#include <map>
#include "../core/config.h" // Needs Config to get rules, neighborhood, etc.
#include "../utils/trie.h"   // The Trie for rule lookup
#include "cell_space.h"     // Needs CellSpace to get cell states
#include "../utils/point.h" // For Point

// Platform-specific includes for dynamic library loading
#ifdef _WIN32
#include <windows.h> // For HMODULE, LoadLibrary, GetProcAddress, FreeLibrary
#else
#include <dlfcn.h>   // For void*, dlopen, dlsym, dlclose
#endif

// Define a function pointer type for the rule update function from the DLL
typedef int (*RuleUpdateFunction)(const int* neighborStates);

class RuleEngine {
private:
    // Trie-based members (used if ruleMode is "trie")
    Trie ruleTrie_;

    // DLL-based members (used if ruleMode is "dll")
#ifdef _WIN32
    HINSTANCE dllHandle_; // HMODULE is a typedef for HINSTANCE
#else
    void* dllHandle_;
#endif
    RuleUpdateFunction dllRuleFunction_;

    // Common members
    std::string currentRuleMode_;
    std::vector<Point> neighborhoodDefinition_; // Cached neighborhood definition from Config.
    int defaultState_;                          // Cached default state from Config.
    // std::vector<int> possibleStates_;        // Cached from config, but not used by current logic. Can be removed if not planned for use.
    bool initialized_;                          // Flag to check if the engine has been initialized.

    // Helper methods for DLL handling
    bool loadRuleLibrary(const std::string& dllPathBaseFromConfig, const std::string& functionName);
    void unloadRuleLibrary();

public:
    RuleEngine();
    ~RuleEngine(); // Destructor to unload DLL

    // Disable copy constructor and assignment operator
    RuleEngine(const RuleEngine&) = delete;
    RuleEngine& operator=(const RuleEngine&) = delete;

    /**
     * @brief Initializes the RuleEngine with configuration data.
     * Sets the mode (Trie or DLL) and populates rules accordingly.
     * @param config A constant reference to the Config object.
     * @return True if initialization was successful, false otherwise.
     */
    bool initialize(const Config& config);

    /**
     * @brief Calculates the next generation of cell states based on the current mode.
     * @param currentCellSpace A constant reference to the current state of the CellSpace.
     * @return A vector of pairs (Point, int) for cells that change state.
     */
    std::vector<std::pair<Point, int>> calculateNextGeneration(const CellSpace& currentCellSpace) const;

    /**
     * @brief Checks if the RuleEngine has been successfully initialized.
     * @return True if initialized, false otherwise.
     */
    bool isInitialized() const;
};

#endif // RULE_ENGINE_H
