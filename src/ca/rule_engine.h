#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include <vector>
#include <string>
#include <unordered_map> // Added for the return type
#include "../core/rule.h"
#include "../utils/trie.h"
#include "cell_space.h"
#include "../utils/point.h" // For Point, and std::hash<Point> via cell_space.h or directly

// Platform-specific includes for dynamic library loading
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Define a function pointer type for the rule update function from the DLL
typedef int (*RuleUpdateFunction)(const int* neighborStates);

class RuleEngine {
private:
    // Trie-based members (used if ruleMode is "trie")
    Trie ruleTrie_;

    // DLL-based members (used if ruleMode is "dll")
#ifdef _WIN32
    HINSTANCE dllHandle_;
#else
    void* dllHandle_;
#endif
    RuleUpdateFunction dllRuleFunction_;

    // Common members
    std::string currentRuleMode_;
    std::vector<Point> neighborhoodDefinition_;
    int defaultState_;
    bool initialized_;

    // Helper methods for DLL handling
    bool loadRuleLibrary(const std::string& dllPathBaseFromConfig, const std::string& functionName);
    void unloadRuleLibrary();

public:
    RuleEngine();
    ~RuleEngine();

    RuleEngine(const RuleEngine&) = delete;
    RuleEngine& operator=(const RuleEngine&) = delete;

    /**
     * @brief Initializes the RuleEngine with configuration data.
     * Sets the mode (Trie or DLL) and populates rules accordingly.
     * @param config A constant reference to the Rule object.
     * @return True if initialization was successful, false otherwise.
     */
    bool initialize(const Rule& config);

    /**
     * @brief Calculates the next generation of cell states based on the current mode.
     * @param currentCellSpace A constant reference to the current state of the CellSpace.
     * @return An unordered_map of (Point, int) for cells that change state.
     * The Point is the coordinate, and int is the new state.
     */
    std::unordered_map<Point, int> calculateNextGeneration(const CellSpace& currentCellSpace) const;

    /**
     * @brief Checks if the RuleEngine has been successfully initialized.
     * @return True if initialized, false otherwise.
     */
    bool isInitialized() const;
};

#endif // RULE_ENGINE_H
