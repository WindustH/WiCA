#ifndef RULE_H
#define RULE_H

#include <string>
#include <vector>
#include <map>
#include "../utils/point.h" // For Point (neighborhood definition)
#include "../utils/color.h"  // For Color (state color mapping)
#include <cstdint>
#include <nlohmann/json.hpp>



/**
 * @class Config
 * @brief Manages the configuration for the Cellular Automaton.
 *
 * This class is responsible for loading CA parameters from a JSON file,
 * including states, default state, neighborhood definition, update rules
 * (either Trie-based or via external DLL), and state-color mappings.
 */
class Rule {
private:
    // --- Configuration Parameters ---
    std::vector<int> states_;                     // List of all possible cell states.
    int defaultState_;                            // The default state for cells not explicitly defined.
    std::vector<Point> neighborhood_;             // Defines neighbor offsets (e.g., Moore, Von Neumann).

    // Rule definition mode
    std::string ruleMode_; // "trie" or "dll"

    // For Trie-based rules
    std::vector<std::vector<int>> rules_;         // List of state update rules. Each inner vector is [N1, N2,..., Nk, NextState].

    // For DLL-based rules
    std::string ruleDllPath_;                     // Path to the DLL containing the rule function.
    std::string ruleFunctionName_;                // Name of the function within the DLL.

    std::map<int, Color> stateColorMap_;          // Maps each cell state to a specific color for rendering.
    bool loadedSuccessfully_;                     // Flag to indicate if configuration was loaded without errors.

    // --- Helper methods for parsing JSON ---
    bool parseStates(const nlohmann::json& j);
    bool parseDefaultState(const nlohmann::json& j);
    bool parseNeighborhood(const nlohmann::json& j);
    bool parseRuleSettings(const nlohmann::json& j); // Handles both Trie and DLL rules
    bool parseStateColorMap(const nlohmann::json& j);

public:
    /**
     * @brief Default constructor. Initializes with empty/default values.
     */
    Rule();

    /**
     * @brief Loads the CA configuration from a specified JSON file.
     * @param filePath The path to the JSON configuration file.
     * @return True if loading and parsing were successful, false otherwise.
     * Error details will be logged via ErrorHandler.
     */
    bool loadFromFile(const std::string& filePath);

    /**
     * @brief Checks if the configuration was loaded successfully.
     * @return True if loaded successfully, false otherwise.
     */
    bool isLoaded() const;

    // --- Getters for Configuration Parameters ---

    const std::vector<int>& getStates() const;
    int getDefaultState() const;
    const std::vector<Point>& getNeighborhood() const;

    const std::string& getRuleMode() const;
    const std::vector<std::vector<int>>& getStateUpdateRules() const; // For Trie mode
    const std::string& getRuleDllPath() const;                     // For DLL mode
    const std::string& getRuleFunctionName() const;                // For DLL mode

    const std::map<int, Color>& getStateColorMap() const;

    /**
     * @brief Gets the color for a specific state.
     * @param state The cell state.
     * @return The Color associated with the state. If the state is not found,
     * returns a default color (e.g., magenta or black) and logs an error.
     */
    Color getColorForState(int state) const;
};

#endif // CONFIG_H
