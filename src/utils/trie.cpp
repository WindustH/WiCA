#include "trie.h"
#include <utility> // For std::move

// Constructor: Initializes the root node.
Trie::Trie() {
    root = new TrieNode();
}

// Destructor: Cleans up all allocated Trie nodes.
Trie::~Trie() {
    deleteNodeRecursive(root);
}

// Move Constructor
Trie::Trie(Trie&& other) noexcept : root(other.root) {
    other.root = nullptr; // Prevent double deletion
}

// Move Assignment Operator
Trie& Trie::operator=(Trie&& other) noexcept {
    if (this != &other) {
        deleteNodeRecursive(root); // Clean up existing resources
        root = other.root;
        other.root = nullptr; // Prevent double deletion
    }
    return *this;
}


/**
 * @brief Recursively deletes a Trie node and all its descendants.
 * This is a helper function for the destructor.
 * @param node The current node to process for deletion.
 */
void Trie::deleteNodeRecursive(TrieNode* node) {
    if (!node) {
        return;
    }
    // Recursively delete all children
    for (auto const& [key, val] : node->children) {
        deleteNodeRecursive(val);
    }
    // Delete the node itself
    delete node;
}

/**
 * @brief Inserts a rule into the Trie.
 * A rule consists of a prefix (sequence of neighbor states) and a result state.
 * @param rulePrefix The sequence of neighbor states.
 * @param resultState The state the center cell transitions to if this rule matches.
 */
void Trie::insertRule(const std::vector<int>& rulePrefix, int resultState) {
    TrieNode* currentNode = root;
    // Traverse the Trie for each state in the rule prefix
    for (int state : rulePrefix) {
        // If a path for the current state doesn't exist, create it
        if (currentNode->children.find(state) == currentNode->children.end()) {
            currentNode->children[state] = new TrieNode();
        }
        // Move to the next node in the path
        currentNode = currentNode->children[state];
    }
    // Mark the end of the rule and store the result state
    currentNode->isEndOfRule = true;
    currentNode->nextStateIfEndOfRule = resultState;
}

/**
 * @brief Finds the resulting state for a given sequence of neighbor states.
 * Traverses the Trie based on the sequence of neighbor states.
 * @param neighborStates The sequence of neighbor states to look up.
 * @return The resulting state if a rule matches, otherwise NO_RULE_FOUND.
 */
int Trie::findNextState(const std::vector<int>& neighborStates) const {
    TrieNode* currentNode = root;
    // Traverse the Trie for each state in the input sequence
    for (int state : neighborStates) {
        // If a path for the current state doesn't exist, the rule is not found
        if (currentNode->children.find(state) == currentNode->children.end()) {
            return NO_RULE_FOUND; // Or your chosen default/sentinel value
        }
        // Move to the next node
        currentNode = currentNode->children[state];
    }

    // If the final node marks the end of a rule, return its result state
    if (currentNode != nullptr && currentNode->isEndOfRule) {
        return currentNode->nextStateIfEndOfRule;
    }

    // If the sequence is a prefix of a rule but not a complete rule itself,
    // or if the sequence doesn't match any rule.
    return NO_RULE_FOUND;
}
