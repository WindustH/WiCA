#include "huffman_coding.h"
#include "../utils/error_handler.h" // For logging errors
#include <algorithm> // For std::sort, std::copy
#include <iterator>  // For std::back_inserter

namespace HuffmanCoding {

    // --- Helper Functions for Compression ---

    /**
     * @brief Builds the frequency table for each byte in the input data.
     * @param data The input data vector.
     * @return A map where keys are bytes and values are their frequencies.
     */
    std::map<std::uint8_t, unsigned> buildFrequencyTable(const std::vector<std::uint8_t>& data) {
        std::map<std::uint8_t, unsigned> freqTable;
        for (std::uint8_t byte : data) {
            freqTable[byte]++;
        }
        return freqTable;
    }

    /**
     * @brief Builds the Huffman tree from the frequency table.
     * @param freqTable The frequency table.
     * @return The root node of the Huffman tree. Returns nullptr if freqTable is empty.
     */
    HuffmanNode* buildHuffmanTree(const std::map<std::uint8_t, unsigned>& freqTable) {
        if (freqTable.empty()) {
            return nullptr;
        }

        std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, CompareNodes> pq;

        // Create a leaf node for each character and add it to the priority queue.
        for (auto const& [byte, freq] : freqTable) {
            pq.push(new HuffmanNode(byte, freq));
        }

        // Special case: if only one unique character, create a dummy node for the tree structure
        if (pq.size() == 1) {
            HuffmanNode* single = pq.top();
            pq.pop();
            HuffmanNode* internal = new HuffmanNode(); // Internal node
            internal->frequency = single->frequency;
            internal->left = single; // Or right, convention dependent
            internal->right = new HuffmanNode(single->data, 0); // Dummy node to make a path
            pq.push(internal);
        }


        // Iterate while size of priority queue is more than 1
        while (pq.size() > 1) {
            // Extract the two nodes with the minimum frequency from the priority queue.
            HuffmanNode* left = pq.top(); pq.pop();
            HuffmanNode* right = pq.top(); pq.pop();

            // Create a new internal node with these two nodes as children and
            // with frequency equal to the sum of the two nodes' frequencies.
            // Add the new node to the priority queue.
            HuffmanNode* internalNode = new HuffmanNode(); // Internal node, data is irrelevant
            internalNode->frequency = left->frequency + right->frequency;
            internalNode->left = left;
            internalNode->right = right;
            pq.push(internalNode);
        }

        // The remaining node is the root node; the tree is complete.
        return pq.top();
    }

    /**
     * @brief Generates Huffman codes by traversing the Huffman tree.
     * @param root The root of the Huffman tree.
     * @param currentCode The current binary code string being built during traversal.
     * @param huffmanCodesMap A map to store the generated Huffman codes (byte -> code string).
     */
    void generateCodes(HuffmanNode* root, const std::string& currentCode, std::map<std::uint8_t, std::string>& huffmanCodesMap) {
        if (!root) {
            return;
        }

        // If this is a leaf node, it contains one of the input characters.
        // Store its code.
        if (!root->left && !root->right) { // Leaf node
            // Handle the case of a single unique character tree more carefully
            if (currentCode.empty() && huffmanCodesMap.empty() && root->frequency > 0) {
                 // If it's the only character, its code is "0" or "1" by convention.
                 // This case is usually handled by the tree construction (dummy node).
                 // If the tree has a single actual data node and a dummy, this path will be taken.
                 // If the tree is just one node (no dummy), this needs a conventional code.
                 // For a single character source, its code is often '0'.
                huffmanCodesMap[root->data] = "0";
            } else {
                huffmanCodesMap[root->data] = currentCode;
            }
            return;
        }

        // Recur for left and right children
        if (root->left) {
            generateCodes(root->left, currentCode + "0", huffmanCodesMap);
        }
        if (root->right) {
            generateCodes(root->right, currentCode + "1", huffmanCodesMap);
        }
    }

    /**
     * @brief Recursively deletes the Huffman tree to free memory.
     * @param root The root node of the tree to delete.
     */
    void deleteHuffmanTree(HuffmanNode* root) {
        if (!root) {
            return;
        }
        deleteHuffmanTree(root->left);
        deleteHuffmanTree(root->right);
        delete root;
    }


    // --- Compression Main Function ---
    std::vector<std::uint8_t> compress(const std::vector<std::uint8_t>& dataToCompress) {
        if (dataToCompress.empty()) {
            ErrorHandler::logError("HuffmanCoding: Cannot compress empty data.", false);
            // Return a minimal representation for empty data if needed, or just empty.
            // For now, let's store original size = 0.
            std::vector<std::uint8_t> compressedOutput;
            std::uint64_t originalSize = 0;
            for (size_t i = 0; i < sizeof(std::uint64_t); ++i) {
                compressedOutput.push_back((originalSize >> (i * 8)) & 0xFF);
            }
            return compressedOutput;
        }

        std::map<std::uint8_t, unsigned> freqTable = buildFrequencyTable(dataToCompress);
        HuffmanNode* treeRoot = buildHuffmanTree(freqTable);
        if (!treeRoot) { // Should not happen if dataToCompress is not empty
             ErrorHandler::logError("HuffmanCoding: Failed to build Huffman tree for non-empty data.");
             deleteHuffmanTree(treeRoot); // Clean up if partially built
             return {}; // Return empty on failure
        }


        std::map<std::uint8_t, std::string> huffmanCodes;
        generateCodes(treeRoot, "", huffmanCodes);

        // Handle the case where only one unique character exists.
        // generateCodes might produce an empty code for it if not handled in tree construction.
        // Our buildHuffmanTree adds a dummy node to ensure a path.
        if (freqTable.size() == 1 && !freqTable.empty()) {
            auto const& [byte, freq] = *freqTable.begin();
            if (huffmanCodes.find(byte) == huffmanCodes.end() || huffmanCodes[byte].empty()) {
                huffmanCodes[byte] = "0"; // Assign a default code
            }
        }


        std::vector<std::uint8_t> compressedOutput;

        // 1. Store original data size (uint64_t)
        std::uint64_t originalSize = dataToCompress.size();
        for (size_t i = 0; i < sizeof(std::uint64_t); ++i) {
            compressedOutput.push_back((originalSize >> (i * 8)) & 0xFF);
        }

        // 2. Store frequency table
        //    Format: [Number of entries (uint32_t)] [entry1_byte (uint8_t)] [entry1_freq (uint32_t)] ...
        std::uint32_t freqTableSize = freqTable.size();
        for (size_t i = 0; i < sizeof(std::uint32_t); ++i) {
            compressedOutput.push_back((freqTableSize >> (i * 8)) & 0xFF);
        }
        for (auto const& [byte, freq] : freqTable) {
            compressedOutput.push_back(byte);
            for (size_t i = 0; i < sizeof(std::uint32_t); ++i) {
                compressedOutput.push_back((freq >> (i * 8)) & 0xFF);
            }
        }

        // 3. Encode data and store padded bits count
        std::string encodedString = "";
        for (std::uint8_t byte : dataToCompress) {
            auto it = huffmanCodes.find(byte);
            if (it != huffmanCodes.end()) {
                 encodedString += it->second;
            } else {
                // This should not happen if the huffmanCodes map is built correctly from all bytes in data
                ErrorHandler::logError("HuffmanCoding: Compress - No Huffman code found for byte: " + std::to_string(byte));
                // Potentially throw an error or handle gracefully
                deleteHuffmanTree(treeRoot);
                return {}; // Error
            }
        }

        std::uint8_t paddedBitsCount = 0;
        if (!encodedString.empty()) {
            paddedBitsCount = (8 - (encodedString.length() % 8)) % 8;
        }
        compressedOutput.push_back(paddedBitsCount);

        for (int i = 0; i < paddedBitsCount; ++i) {
            encodedString += '0'; // Add padding
        }

        // Convert bit string to bytes
        for (size_t i = 0; i < encodedString.length(); i += 8) {
            std::uint8_t byte = 0;
            for (size_t j = 0; j < 8; ++j) {
                if (encodedString[i + j] == '1') {
                    byte |= (1 << (7 - j));
                }
            }
            compressedOutput.push_back(byte);
        }

        // Cleanup Huffman tree
        deleteHuffmanTree(treeRoot);

        return compressedOutput;
    }


    // --- Decompression Main Function ---
    std::vector<std::uint8_t> decompress(const std::vector<std::uint8_t>& compressedData) {
        if (compressedData.size() < sizeof(std::uint64_t) + sizeof(std::uint32_t) + 1) { // Min size: original_size + freq_table_size + padding_info
            ErrorHandler::logError("HuffmanCoding: Compressed data is too short to be valid.");
            return {};
        }

        std::vector<std::uint8_t> decompressedOutput;
        size_t currentByteIndex = 0;

        // 1. Read original data size
        std::uint64_t originalSize = 0;
        for (size_t i = 0; i < sizeof(std::uint64_t); ++i) {
            originalSize |= (static_cast<std::uint64_t>(compressedData[currentByteIndex++]) << (i * 8));
        }

        if (originalSize == 0) { // Handle empty original data case
             if (compressedData.size() == sizeof(std::uint64_t)) return {}; // Correctly empty
             else {
                 ErrorHandler::logError("HuffmanCoding: Decompress - Original size is 0 but more data exists.");
                 return {}; // Malformed
             }
        }


        // 2. Read frequency table
        std::uint32_t freqTableNumEntries = 0;
        if (currentByteIndex + sizeof(std::uint32_t) > compressedData.size()) {
             ErrorHandler::logError("HuffmanCoding: Decompress - Not enough data for frequency table size."); return {};
        }
        for (size_t i = 0; i < sizeof(std::uint32_t); ++i) {
            freqTableNumEntries |= (static_cast<std::uint32_t>(compressedData[currentByteIndex++]) << (i * 8));
        }

        std::map<std::uint8_t, unsigned> freqTable;
        for (std::uint32_t i = 0; i < freqTableNumEntries; ++i) {
            if (currentByteIndex + 1 + sizeof(std::uint32_t) > compressedData.size()) {
                ErrorHandler::logError("HuffmanCoding: Decompress - Not enough data for frequency table entry."); return {};
            }
            std::uint8_t byte = compressedData[currentByteIndex++];
            unsigned freq = 0;
            for (size_t j = 0; j < sizeof(std::uint32_t); ++j) {
                freq |= (static_cast<std::uint32_t>(compressedData[currentByteIndex++]) << (j * 8));
            }
            freqTable[byte] = freq;
        }

        if (freqTable.empty() && originalSize > 0) {
            ErrorHandler::logError("HuffmanCoding: Decompress - Frequency table is empty but original size > 0.");
            return {};
        }

        HuffmanNode* treeRoot = nullptr;
        if (!freqTable.empty()) {
            treeRoot = buildHuffmanTree(freqTable);
            if (!treeRoot) {
                ErrorHandler::logError("HuffmanCoding: Decompress - Failed to rebuild Huffman tree.");
                deleteHuffmanTree(treeRoot); // Important to clean up if partially built
                return {};
            }
        } else if (originalSize > 0) { // Freq table empty, but data expected.
            ErrorHandler::logError("HuffmanCoding: Decompress - Freq table empty but original size > 0. Data inconsistent.");
            return {};
        }


        // 3. Read padded bits count
        if (currentByteIndex + 1 > compressedData.size()) {
            ErrorHandler::logError("HuffmanCoding: Decompress - Not enough data for padded bits count.");
            deleteHuffmanTree(treeRoot);
            return {};
        }
        std::uint8_t paddedBitsCount = compressedData[currentByteIndex++];

        // 4. Decode data
        std::string bitString = "";
        for (size_t i = currentByteIndex; i < compressedData.size(); ++i) {
            std::uint8_t byte = compressedData[i];
            for (int j = 7; j >= 0; --j) { // MSB first
                bitString += ((byte >> j) & 1) ? '1' : '0';
            }
        }

        if (bitString.length() < paddedBitsCount) {
             ErrorHandler::logError("HuffmanCoding: Decompress - Bit string too short for declared padding.");
             deleteHuffmanTree(treeRoot);
             return {};
        }
        // Remove padding if bitString is not empty
        if (!bitString.empty() && paddedBitsCount > 0 && paddedBitsCount < 8) {
             bitString.erase(bitString.length() - paddedBitsCount);
        }


        HuffmanNode* currentNode = treeRoot;
        // Handle case of single unique character (tree might be just root or root with one child '0')
        if (freqTable.size() == 1 && treeRoot) {
            // The single character is treeRoot->data or treeRoot->left->data depending on dummy node
            std::uint8_t singleChar = 0;
            if(treeRoot->left && !treeRoot->left->left && !treeRoot->left->right) singleChar = treeRoot->left->data; // Assuming left is actual data
            else if (treeRoot->right && !treeRoot->right->left && !treeRoot->right->right) singleChar = treeRoot->right->data; // Or right
            else if (!treeRoot->left && !treeRoot->right) singleChar = treeRoot->data; // Root itself is the char
            else { ErrorHandler::logError("HuffmanCoding: Decompress - Could not determine single char from tree."); deleteHuffmanTree(treeRoot); return {};}


            for (std::uint64_t i = 0; i < originalSize; ++i) {
                decompressedOutput.push_back(singleChar);
            }
        } else if (treeRoot) { // Normal case with multiple characters
            for (char bit : bitString) {
                if (!currentNode) { // Should not happen in a well-formed stream/tree
                    ErrorHandler::logError("HuffmanCoding: Decompress - Reached null node during traversal.");
                    deleteHuffmanTree(treeRoot);
                    return {};
                }
                if (bit == '0') {
                    currentNode = currentNode->left;
                } else { // bit == '1'
                    currentNode = currentNode->right;
                }

                if (!currentNode) { // Path leads to nowhere
                     ErrorHandler::logError("HuffmanCoding: Decompress - Invalid bit sequence, path leads to null.");
                     deleteHuffmanTree(treeRoot);
                     return {};
                }

                if (!currentNode->left && !currentNode->right) { // Leaf node
                    decompressedOutput.push_back(currentNode->data);
                    currentNode = treeRoot; // Reset to root for next character
                    if (decompressedOutput.size() == originalSize) {
                        break; // Stop once all expected bytes are decoded
                    }
                }
            }
        }


        // Cleanup Huffman tree
        deleteHuffmanTree(treeRoot);

        if (decompressedOutput.size() != originalSize) {
            ErrorHandler::logError("HuffmanCoding: Decompression resulted in size mismatch. Expected: " +
                                   std::to_string(originalSize) + ", Got: " + std::to_string(decompressedOutput.size()));
            // This could be due to corrupted data or an issue in padding/EOS handling.
            return {}; // Return empty or partially decompressed data based on policy
        }

        return decompressedOutput;
    }

} // namespace HuffmanCoding
