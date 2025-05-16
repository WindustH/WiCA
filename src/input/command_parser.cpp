#include "command_parser.h"
#include "../core/application.h"
#include "../ca/cell_space.h"
#include "../render/viewport.h"
#include "../snap/snapshot.h"
#include "../utils/logger.h" // New logger

#include <algorithm>
#include <vector> // Required for std::vector
#include <string> // Required for std::string, std::stoi, std::stof
#include <stdexcept> // Required for std::invalid_argument, std::out_of_range


CommandParser::CommandParser(Application& app) : application_(app) {}

std::vector<std::string> CommandParser::tokenize(const std::string& s, char delimiter) const {
    std::vector<std::string> tokens;
    std::string current_token;
    std::istringstream stream(s);
    bool in_quotes = false;
    char c;

    while (stream.get(c)) {
        if (c == '"') {
            in_quotes = !in_quotes;
            // Decide whether to include quotes in the token or not.
            // For set-font, we'll strip them later if they are at the very start/end of the path arg.
            // For now, let's keep them to correctly delimit tokens with spaces.
            // current_token += c; // Option 1: Keep quotes in token temporarily
            if (!in_quotes && !current_token.empty() && current_token.front() == '"') {
                // This means a quoted token just ended.
            } else if (in_quotes && current_token.empty()){
                 current_token +=c; // Start of a quoted token.
            } else {
                 current_token +=c;
            }

        } else if (c == delimiter && !in_quotes) {
            if (!current_token.empty()) {
                if (current_token.length() >= 2 && current_token.front() == '"' && current_token.back() == '"') {
                     tokens.push_back(current_token.substr(1, current_token.length() - 2));
                } else {
                    tokens.push_back(current_token);
                }
                current_token.clear();
            }
        } else {
            current_token += c;
        }
    }
    if (!current_token.empty()) {
        if (current_token.length() >= 2 && current_token.front() == '"' && current_token.back() == '"') {
             tokens.push_back(current_token.substr(1, current_token.length() - 2));
        } else {
            tokens.push_back(current_token);
        }
    }
    // Clean up any remaining empty tokens that might have been added if string ends with delimiter
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [](const std::string& tok){ return tok.empty(); }), tokens.end());
    return tokens;
}

std::string CommandParser::joinTokens(const std::vector<std::string>& tokens, size_t start_index, size_t end_index) const {
    std::string result;
    for (size_t i = start_index; i < end_index && i < tokens.size(); ++i) {
        if (i > start_index) {
            result += " ";
        }
        result += tokens[i];
    }
    return result;
}

bool CommandParser::parseAndExecute(const std::string& commandString,
                                   CellSpace& cellSpace,
                                   Viewport& viewport,
                                   SnapshotManager& snapshotManager) {
    if (commandString.empty()) {
        return true;
    }

    std::vector<std::string> tokens = tokenize(commandString);
    if (tokens.empty()) {
        return true;
    }

    std::string command = tokens[0];
    std::transform(command.begin(), command.end(), command.begin(),
                   [](unsigned char c){ return std::tolower(c); });


    if (command == "save") {
        if (tokens.size() >= 2) {
            std::string filename = joinTokens(tokens, 1, tokens.size());
            if (filename.rfind(".snapshot") == std::string::npos && filename.rfind('.') == std::string::npos) {
                filename += ".snapshot";
            }
            application_.saveSnapshot(filename);
        } else {
            application_.postMessageToUser("Usage: save <filename>");
        }
        return true;
    } else if (command == "load") {
        if (tokens.size() >= 2) {
            std::string filename = joinTokens(tokens, 1, tokens.size());
            application_.loadSnapshot(filename);
        } else {
            application_.postMessageToUser("Usage: load <filename>");
        }
        return true;
    } else if (command == "load-rule") { // New command
        if (tokens.size() >= 2) {
            std::string configPath = joinTokens(tokens, 1, tokens.size());
            application_.loadRule(configPath);
        } else {
            application_.postMessageToUser("Usage: load-rule <filepath>");
        }
        return true;
    }else if (command == "brush-state") {
        if (tokens.size() == 2) {
            try {
                int state_value = std::stoi(tokens[1]);
                application_.setBrushState(state_value);
            } catch (const std::invalid_argument& ia) {
                application_.postMessageToUser("Error: Brush state must be an integer.");
            } catch (const std::out_of_range& oor) {
                 application_.postMessageToUser("Error: Brush state value out of range.");
            }
        } else {
            application_.postMessageToUser("Usage: brush-state <state_value>");
        }
        return true;
    } else if (command == "brush-size") {
        if (tokens.size() == 2) {
            try {
                int size_value = std::stoi(tokens[1]);
                application_.setBrushSize(size_value);
            } catch (const std::invalid_argument& ia) {
                application_.postMessageToUser("Error: Brush size must be an integer.");
            } catch (const std::out_of_range& oor) {
                application_.postMessageToUser("Error: Brush size value out of range.");
            }
        } else {
            application_.postMessageToUser("Usage: brush-size <size>");
        }
        return true;
    } else if (command == "font-size") {
        if (tokens.size() == 2) {
            try {
                int size_value = std::stoi(tokens[1]);
                application_.setAppFontSize(size_value);
            } catch (const std::invalid_argument& ia) {
                application_.postMessageToUser("Error: Font size must be an integer.");
            } catch (const std::out_of_range& oor) {
                application_.postMessageToUser("Error: Font size value out of range.");
            }
        } else {
            application_.postMessageToUser("Usage: font-size <points>");
        }
        return true;
    } else if (command == "set-font") {
        if (tokens.size() >= 2) {
            std::string fontPath = joinTokens(tokens, 1, tokens.size());
            application_.setAppFontPath(fontPath);
        } else {
            application_.postMessageToUser("Usage: set-font <font_path>");
        }
        return true;
    } else if (command == "set-grid-display") {
        if (tokens.size() == 2) {
            std::string mode = tokens[1];
            std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
            application_.setGridDisplayMode(mode);
        } else {
            application_.postMessageToUser("Usage: set-grid-display <auto|on|off>");
        }
        return true;
    } else if (command == "set-grid-threshold") {
        if (tokens.size() == 2) {
            try {
                int threshold_value = std::stoi(tokens[1]);
                application_.setGridHideThreshold(threshold_value);
            } catch (const std::invalid_argument& ia) {
                application_.postMessageToUser("Error: Grid threshold must be an integer.");
            } catch (const std::out_of_range& oor) {
                application_.postMessageToUser("Error: Grid threshold value out of range.");
            }
        } else {
            application_.postMessageToUser("Usage: set-grid-threshold <value>");
        }
        return true;
    } else if (command == "set-grid-width") { // New command
        if (tokens.size() == 2) {
            try {
                int width = std::stoi(tokens[1]);
                application_.setGridLineWidth(width);
            } catch (const std::invalid_argument& ia) {
                application_.postMessageToUser("Error: Grid width must be an integer.");
            } catch (const std::out_of_range& oor) {
                application_.postMessageToUser("Error: Grid width value out of range.");
            }
        } else {
            application_.postMessageToUser("Usage: set-grid-width <pixels>");
        }
        return true;
    } else if (command == "set-grid-color") { // New command
        if (tokens.size() == 4 || tokens.size() == 5) { // r g b [a]
            try {
                int r = std::stoi(tokens[1]);
                int g = std::stoi(tokens[2]);
                int b = std::stoi(tokens[3]);
                int a = 255;
                if (tokens.size() == 5) {
                    a = std::stoi(tokens[4]);
                }
                application_.setGridLineColor(r, g, b, a);
            } catch (const std::invalid_argument& ia) {
                application_.postMessageToUser("Error: Color components must be integers.");
            } catch (const std::out_of_range& oor) {
                application_.postMessageToUser("Error: Color component value out of range.");
            }
        } else {
            application_.postMessageToUser("Usage: set-grid-color <r> <g> <b> [alpha]");
        }
        return true;
    }else if (command == "pause") {
        application_.pauseSimulation();
        return true;
    } else if (command == "resume" || command == "start" || command == "run") {
        application_.resumeSimulation();
        return true;
    } else if (command == "autofit") {
        if (tokens.size() == 2) {
            std::string mode = tokens[1];
            std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
            if (mode == "on") {
                application_.setAutoFitView(true);
            } else if (mode == "off") {
                application_.setAutoFitView(false);
            } else {
                application_.postMessageToUser("Usage: autofit <on|off>");
            }
        } else {
            application_.setAutoFitView(!application_.isViewportAutoFitEnabled());
        }
        return true;
    } else if (command == "center") {
        application_.centerViewOnGrid();
        return true;
    } else if (command == "clear-grid" || command == "reset-grid" || command == "clear") {
        application_.clearSimulation();
        return true;
    } else if (command == "set-sim-speed" || command == "speed"){
         if (tokens.size() == 2) {
            try {
                float speed_value = std::stof(tokens[1]);
                application_.setSimulationSpeed(speed_value);
            } catch (const std::invalid_argument& ia) {
                application_.postMessageToUser("Error: Speed must be a number.");
            } catch (const std::out_of_range& oor) {
                application_.postMessageToUser("Error: Speed value out of range.");
            }
        } else {
            application_.postMessageToUser("Usage: speed <updates_per_second>");
        }
        return true;
    } else if (command == "toggle-brush-info" || command == "brushinfo") {
        application_.toggleBrushInfoDisplay();
        return true;
    } else if (command == "help" || command == "h" || command == "?") {
        application_.displayHelp();
        return true;
    } else if (command == "quit" || command == "exit") {
        application_.quit();
        return true;
    }
    auto logger = Logging::GetLogger(Logging::Module::CommandParser);
    if (logger) logger->error("Unknown command: " + commandString);
    application_.postMessageToUser("Unknown command: " + tokens[0] + ". Type 'help'.");
    return false;
}
