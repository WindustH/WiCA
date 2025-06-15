#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string>
#include <vector>
#include <sstream> // For string stream parsing

// Forward declarations
class Application;
class CellSpace;
class Viewport;
class SnapshotManager;

/**
 * @class CommandParser
 * @brief Parses text commands entered by the user and executes them.
 *
 * It takes a command string, tokenizes it, and then calls appropriate
 * methods on the Application or other relevant components to perform the requested action.
 */
class CommandParser {
private:
    Application& application_; // Reference to the main application.

    /**
     * @brief Splits a string into tokens based on a delimiter, respecting quotes.
     * Example: "set-font \"My Font Name\" 16" -> ["set-font", "My Font Name", "16"]
     * @param s The string to split.
     * @param delimiter The character to split by (default is space).
     * @return A vector of string tokens.
     */
    std::vector<std::string> tokenize(const std::string& s, char delimiter = ' ') const;

    /**
     * @brief Helper to join a range of tokens into a single string.
     * @param tokens Vector of tokens.
     * @param start_index Starting index in the tokens vector.
     * @param end_index Ending index (exclusive) in the tokens vector.
     * @return Joined string.
     */
    std::string joinTokens(const std::vector<std::string>& tokens, size_t start_index, size_t end_index) const;


public:
    /**
     * @brief Constructor for CommandParser.
     * @param app A reference to the main Application object.
     */
    CommandParser(Application& app);

    /**
     * @brief Parses the given command string and executes the corresponding action.
     * The commandString should NOT have the leading '/'
     * @param commandString The full command string entered by the user (e.g., "save my_sim.snapshot").
     * @return True if the command was recognized and successfully (or attempted to be) executed,
     * false if the command is unknown or has invalid syntax/arguments.
     */
    bool parseAndExecute(const std::string& commandString);
};

#endif // COMMAND_PARSER_H
