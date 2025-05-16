#ifndef TRIE_H
#define TRIE_H

#include <map>
#include <vector>
#include <string> // Included for potential debugging, not strictly necessary for core logic

// Constant to indicate no rule was found or an invalid state for lookup
constexpr int NO_RULE_FOUND = -1; // Or some other sentinel value relevant to your state definitions

/**
 * @class Trie
 * @brief Implements a Trie (Prefix Tree) for efficient lookup of cellular automaton rules.
 *
 * The Trie stores sequences of neighbor states (rule prefixes) and maps them
 * to the resulting state of the center cell. Each node in the Trie represents
 * a state in a potential rule sequence.
 */
class Trie {
public:
    /**
     * @struct TrieNode
     * @brief Represents a node in the Trie.
     *
     * Each node can have children representing the next state in a rule sequence.
     * If a node marks the end of a valid rule prefix, it stores the resulting
     * state for the center cell.
     */
    struct TrieNode {
        // Maps a neighbor state to the next TrieNode in the sequence.
        std::map<int, TrieNode*> children;

        // Flag indicating if the path to this node forms a complete rule prefix.
        bool isEndOfRule;

        // If isEndOfRule is true, this stores the new state for the center cell.
        // Initialized to a value indicating no rule ends here by default.
        int nextStateIfEndOfRule;

        // Constructor
        TrieNode() : isEndOfRule(false), nextStateIfEndOfRule(NO_RULE_FOUND) {}

        // Destructor: Recursively delete children.
        // This is handled by the Trie's destructor to ensure proper cleanup.
    };

private:
    // Pointer to the root node of the Trie.
    TrieNode* root;

    /**
     * @brief Recursively deletes a Trie node and all its descendants.
     * @param node The node to delete.
     */
    void deleteNodeRecursive(TrieNode* node);

public:
    /**
     * @brief Constructor for the Trie. Initializes the root node.
     */
    Trie();

    /**
     * @brief Destructor for the Trie. Ensures all dynamically allocated nodes are freed.
     */
    ~Trie();

    // Disable copy constructor and copy assignment operator to prevent shallow copies
    // of the dynamically allocated Trie structure. If copying is needed, a deep copy
    // mechanism should be implemented.
    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;

    // Allow move constructor and move assignment operator for efficient transfer of ownership.
    Trie(Trie&& other) noexcept;
    Trie& operator=(Trie&& other) noexcept;


    /**
     * @brief Inserts a rule into the Trie.
     * @param rulePrefix A vector of integers representing the sequence of neighbor states.
     * The order should match the neighborhood definition.
     * @param resultState The state the center cell should transition to if this rule matches.
     */
    void insertRule(const std::vector<int>& rulePrefix, int resultState);

    /**
     * @brief Finds the resulting state for a given sequence of neighbor states.
     * @param neighborStates A vector of integers representing the states of the neighbors.
     * @return The resulting state for the center cell if a matching rule is found.
     * Returns NO_RULE_FOUND (or your chosen sentinel value) if no rule matches the sequence.
     */
    int findNextState(const std::vector<int>& neighborStates) const;
};

#endif // TRIE_H
