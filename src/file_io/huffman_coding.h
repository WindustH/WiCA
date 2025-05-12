#ifndef HUFFMAN_CODING_H
#define HUFFMAN_CODING_H

#include <vector>
#include <string>
#include <map>
#include <queue> // For priority_queue
#include <cstdint> // For uint8_t, etc.

/**
 * @namespace HuffmanCoding
 * @brief Provides static methods for Huffman compression and decompression.
 *
 * This namespace contains the logic to build Huffman trees, generate codes,
 * encode data into a compressed bitstream, and decode that bitstream back
 * into the original data.
 */
namespace HuffmanCoding {

    // Node structure for the Huffman tree
    struct HuffmanNode {
        std::uint8_t data;      // Character (byte)
        unsigned frequency;     // Frequency of the character
        HuffmanNode *left, *right; // Left and right children

        HuffmanNode(std::uint8_t data, unsigned frequency)
            : data(data), frequency(frequency), left(nullptr), right(nullptr) {}

        // Null node constructor (for internal nodes)
        HuffmanNode() : data(0), frequency(0), left(nullptr), right(nullptr) {}

        ~HuffmanNode() {
            // The tree cleanup is handled by a dedicated function to avoid deep recursion issues
            // if ~HuffmanNode calls delete on children directly in very large trees.
        }
    };

    // Comparison structure for the priority queue (min-heap)
    struct CompareNodes {
        bool operator()(HuffmanNode* l, HuffmanNode* r) {
            return l->frequency > r->frequency;
        }
    };

    /**
     * @brief Compresses the input data using Huffman coding.
     * @param dataToCompress A vector of bytes representing the data to be compressed.
     * @return A vector of bytes representing the compressed data.
     * The compressed data includes the Huffman tree (or frequency table) and the encoded bitstream.
     * Format: [Frequency Table Size (uint32_t)] [Frequency Table Entries (char, uint32_t)...]
     * [Padded Bits Count (uint8_t)] [Compressed Data Bits...]
     */
    std::vector<std::uint8_t> compress(const std::vector<std::uint8_t>& dataToCompress);

    /**
     * @brief Decompresses the input data that was compressed using Huffman coding.
     * @param compressedData A vector of bytes representing the compressed data stream
     * (including the Huffman tree/frequency table).
     * @param decompressedSize The expected size of the decompressed data. This is often stored
     * separately or as part of the compressed stream metadata by the caller. For this implementation,
     * we will try to embed it or infer it if possible, or require it.
     * Let's refine: The compressed data will store the original size.
     * Format: [Original Data Size (uint64_t)] [Frequency Table Size (uint32_t)]
     * [Frequency Table Entries (char, uint32_t)...]
     * [Padded Bits Count (uint8_t)] [Compressed Data Bits...]
     * @return A vector of bytes representing the original, decompressed data.
     * Returns an empty vector if decompression fails.
     */
    std::vector<std::uint8_t> decompress(const std::vector<std::uint8_t>& compressedData);


    // Helper function to recursively delete the Huffman tree
    void deleteHuffmanTree(HuffmanNode* root);

} // namespace HuffmanCoding

#endif // HUFFMAN_CODING_H
