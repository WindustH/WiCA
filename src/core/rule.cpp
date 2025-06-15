#include "rule.h"
#include "../utils/logger.h" // New logger
#include <fstream>
#include <stdexcept>
#include <algorithm> // Required for std::transform

using json = nlohmann::json;

Rule::Rule()
    : defaultState_(0),
      loadedSuccessfully_(false)
{
}

bool Rule::loadFromFile(const std::string& filePath) {
    auto logger = Logger::getLogger(Logger::Module::Rule);
    loadedSuccessfully_ = false;

    std::ifstream configFileStream(filePath);
    if (!configFileStream.is_open()) {
        if (logger) logger->error("Failed to open configuration file: " + filePath);
        return false;
    }

    json ruleJson;
    try {
        configFileStream >> ruleJson;
    } catch (json::parse_error& e) {
        if (logger) logger->error("JSON parsing error in file " + filePath + " - " + e.what());
        return false;
    } catch (const std::exception& e) { // Catch generic std::exception
        if (logger) logger->error("Generic error reading file " + filePath + " - " + std::string(e.what()));
        return false;
    }

    if (!parseStates(ruleJson)) return false;
    if (!parseDefaultState(ruleJson)) return false;
    if (!parseNeighborhood(ruleJson)) return false;
    if (!parseRuleSettings(ruleJson)) return false;
    if (!parseStateColorMap(ruleJson)) return false;

    loadedSuccessfully_ = true;
    if (logger) logger->info("Configuration loaded successfully from " + filePath);
    return true;
}

bool Rule::isLoaded() const {
    return loadedSuccessfully_;
}

bool Rule::parseStates(const json& j) {
    auto logger = Logger::getLogger(Logger::Module::Rule);
    try {
        if (!j.contains("states") || !j["states"].is_array()) {
            if (logger) logger->error("'states' field is missing or not an array.");
            return false;
        }
        states_ = j["states"].get<std::vector<int>>();
        if (states_.empty()) {
            if (logger) logger->error("'states' array cannot be empty.");
            return false;
        }
    } catch (const json::exception& e) {
        if (logger) logger->error("Error parsing 'states': " + std::string(e.what()));
        return false;
    }
    return true;
}

bool Rule::parseDefaultState(const json& j) {
    auto logger = Logger::getLogger(Logger::Module::Rule);
    try {
        if (!j.contains("default_state") || !j["default_state"].is_number_integer()) {
            if (logger) logger->error("'default_state' field is missing or not an integer.");
            return false;
        }
        defaultState_ = j["default_state"].get<int>();
        bool found = false;
        if (states_.empty() && defaultState_ == 0) { // Special case: if states is empty, allow default 0
            found = true;
        } else {
            for(int s : states_) {
                if (s == defaultState_) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            if (logger) logger->error("'default_state' (" + std::to_string(defaultState_) + ") is not in the list of defined 'states'.");
            return false;
        }
    } catch (const json::exception& e) {
        if (logger) logger->error("Error parsing 'default_state': " + std::string(e.what()));
        return false;
    }
    return true;
}

bool Rule::parseNeighborhood(const json& j) {
    auto logger = Logger::getLogger(Logger::Module::Rule);
    try {
        if (!j.contains("neighborhood") || !j["neighborhood"].is_array()) {
            if (logger) logger->error("'neighborhood' field is missing or not an array. This is required for both Trie and DLL (for neighbor count) modes.");
            return false;
        }
        neighborhood_.clear();
        for (const auto& item : j["neighborhood"]) {
            if (!item.is_array() || item.size() != 2 || !item[0].is_number_integer() || !item[1].is_number_integer()) {
                if (logger) logger->error("Invalid neighborhood entry. Each entry must be an array of two integers [dx, dy].");
                return false;
            }
            neighborhood_.emplace_back(item[0].get<int>(), item[1].get<int>());
        }
        // Neighborhood can be empty if the DLL logic doesn't use neighbors,
        // but the DLL function signature still needs neighborCount.
        // If it's empty for Trie mode, it's usually an issue if rules are present.
    } catch (const json::exception& e) {
        if (logger) logger->error("Error parsing 'neighborhood': " + std::string(e.what()));
        return false;
    }
    return true;
}

bool Rule::parseRuleSettings(const json& j) {
    auto logger = Logger::getLogger(Logger::Module::Rule);
    // Reset member variables related to rules before parsing
    rules_.clear();
    ruleDllPath_.clear();
    ruleFunctionName_.clear();

    try {
        if (logger) logger->info("Processing as DLL mode.");
        if (!j.contains("rule_dll_path") || !j["rule_dll_path"].is_string()) {
            if (logger) logger->error("'rule_dll_path' is missing or not a string for DLL rule mode.");
            return false;
        }
        ruleDllPath_ = j["rule_dll_path"].get<std::string>();

        if (!j.contains("rule_function_name") || !j["rule_function_name"].is_string()) {
            if (logger) logger->error("'rule_function_name' is missing or not a string for DLL rule mode.");
            return false;
        }
        ruleFunctionName_ = j["rule_function_name"].get<std::string>();
        // rules_ was already cleared at the start of the function
        if (logger) logger->info("DLL rule settings parsed. Path: " + ruleDllPath_ + ", Function: " + ruleFunctionName_);
        return true; // Successfully parsed DLL settings
    }
    catch (const json::exception& e) {
        if (logger) logger->error("JSON exception during parsing rule settings: " + std::string(e.what()));
        return false;
    }
}

// Helper function for default color assignment (to avoid repetition)
void assignDefaultColors(std::map<int, Color>& mapToFill, const std::vector<int>& states) {
    auto logger = Logger::getLogger(Logger::Module::Rule);
    mapToFill.clear(); // Ensure we start fresh
    if (logger) logger->info("Assigning default colors for states."); // Informative message
    for (int s : states) {
        // Using the specific default logic from your original "missing field" handling
        if (s == 0) {
            mapToFill[s] = Color(220, 220, 220, 255); // Default for state 0
        } else if (s == 1) {
            mapToFill[s] = Color(30, 30, 30, 255);    // Default for state 1
        } else {
            // Calculated debug color for other states
            mapToFill[s] = Color( (s * 30) % 256, (s * 50) % 256, (s * 70) % 256, 255);
        }
    }
}

bool Rule::parseStateColorMap(const nlohmann::json& j) {
    auto logger = Logger::getLogger(Logger::Module::Rule);
    try {
        stateColorMap_.clear(); // Start with an empty map
        bool format_valid = true; // Assume format is valid initially

        if (!j.contains("state_color_map")) {
            if (logger) logger->warn("'state_color_map' field is missing.");
            format_valid = false;
        } else {
            const auto& color_map_json = j["state_color_map"];

            if (!color_map_json.is_array()) {
                if (logger) logger->warn("'state_color_map' field is not a JSON array.");
                format_valid = false;
            } else {
                // It's an array, now validate its contents
                for (size_t i = 0; i < color_map_json.size(); ++i) {
                    const auto& color_array_json = color_map_json[i];
                    int current_state = static_cast<int>(i); // State ID is the index

                    // Check 1: Is the element itself an array?
                    if (!color_array_json.is_array()) {
                        if (logger) logger->warn("Element at index " + std::to_string(i) + " in 'state_color_map' is not a color array.");
                        format_valid = false;
                        break; // Stop processing this array on first error
                    }

                    // Check 2: Is the color array size valid (3 or 4)?
                    if (color_array_json.size() < 3 || color_array_json.size() > 4) {
                        if (logger) logger->warn("Color array for state " + std::to_string(current_state) +
                                               " has invalid size (" + std::to_string(color_array_json.size()) + "). Must be 3 (RGB) or 4 (RGBA).");
                        format_valid = false;
                        break; // Stop processing
                    }

                    // Check 3: Are the components valid integers (0-255)?
                    std::vector<unsigned char> c_vals;
                    bool components_valid = true;
                    for(const auto& comp : color_array_json) {
                        if (!comp.is_number_integer() || comp.get<int>() < 0 || comp.get<int>() > 255) {
                             if (logger) logger->info("Invalid color component found for state " + std::to_string(current_state) +
                                                    ". Components must be integers between 0-255.");
                             components_valid = false;
                             format_valid = false;
                             break; // Stop processing components for this color
                        }
                        c_vals.push_back(static_cast<unsigned char>(comp.get<int>()));
                    }

                    if (!components_valid) {
                        break; // Stop processing the outer array
                    }

                    // If all checks passed for this color array, store it
                    unsigned char alpha = (c_vals.size() == 4) ? c_vals[3] : 255;
                    stateColorMap_[current_state] = Color(c_vals[0], c_vals[1], c_vals[2], alpha);
                }
            }
        }

        // --- Decision Point ---
        if (!format_valid) {
            // If the format was invalid at any point, discard partial results and use defaults
            assignDefaultColors(stateColorMap_, states_);
        } else {
            // Format was valid, but the array might not cover all states defined in `states_`
            // Assign debug colors for any remaining states from the `states_` list
            for (int s : states_) {
                if (stateColorMap_.find(s) == stateColorMap_.end()) {
                    if (logger) logger->warn("State " + std::to_string(s) + " defined in 'states' but not covered by 'state_color_map' array. Assigning debug color.");
                     // Use the calculated debug color part of the default logic
                     stateColorMap_[s] = Color( (s * 30) % 256, (s * 50) % 256, (s * 70) % 256, 255);
                }
            }
        }

    } catch (const nlohmann::json::exception& e) {
        // Catch potential errors during JSON access/parsing (e.g., malformed JSON file)
        if (logger) logger->error("JSON parsing error while accessing 'state_color_map': " + std::string(e.what()));
        // Assign default colors as a safe fallback even for JSON errors
        assignDefaultColors(stateColorMap_, states_);
        // Return true because we handled it with defaults, or return false if this should be fatal
        return true; // Or consider false if a json exception should halt config loading
    } catch (const std::exception& e) {
        // Catch other potential standard exceptions
         if (logger) logger->error("Standard error during 'state_color_map' processing: " + std::string(e.what()));
         assignDefaultColors(stateColorMap_, states_);
         return true; // Or false
    }

    return true; // Parsing completed (either successfully via JSON or using defaults)
}


const std::vector<int>& Rule::getStates() const {
    return states_;
}

int Rule::getDefaultState() const {
    return defaultState_;
}

const std::vector<Point>& Rule::getNeighborhood() const {
    return neighborhood_;
}

const std::vector<std::vector<int>>& Rule::getStateUpdateRules() const {
    return rules_;
}

const std::string& Rule::getRuleDllPath() const {
    return ruleDllPath_;
}

const std::string& Rule::getRuleFunctionName() const {
    return ruleFunctionName_;
}

const std::map<int, Color>& Rule::getStateColorMap() const {
    return stateColorMap_;
}

Color Rule::getColorForState(int state) const {
    auto logger = Logger::getLogger(Logger::Module::Rule);
    auto it = stateColorMap_.find(state);
    if (it != stateColorMap_.end()) {
        return it->second;
    }
    if (logger) logger->info("Requested color for unmapped state: " + std::to_string(state) + ". Returning debug color.");
    return Color(255, 0, 255, 255);
}
