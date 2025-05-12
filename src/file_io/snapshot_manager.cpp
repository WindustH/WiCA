#include "snapshot_manager.h"
#include "../ca/cell_space.h"
#include "huffman_coding.h"
#include "../utils/error_handler.h"
#include "../utils/point.h" // For Point struct used in CellSpace

#include <fstream>   // For file I/O (std::ofstream, std::ifstream)
#include <iterator>  // For std::istreambuf_iterator

// Constructor
SnapshotManager::SnapshotManager() {}

// Helper to write a 32-bit integer to a byte vector (little-endian)
void SnapshotManager::writeInt32(std::vector<std::uint8_t>& buffer, std::int32_t value) const {
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

// Helper to read a 32-bit integer from a byte vector (little-endian)
std::int32_t SnapshotManager::readInt32(const std::vector<std::uint8_t>& buffer, size_t& offset) const {
    if (offset + sizeof(std::int32_t) > buffer.size()) {
        ErrorHandler::logError("SnapshotManager: ReadInt32 - Read out of bounds.");
        throw std::out_of_range("ReadInt32 out of bounds");
    }
    std::int32_t value = 0;
    value |= static_cast<std::int32_t>(buffer[offset++]);
    value |= static_cast<std::int32_t>(buffer[offset++]) << 8;
    value |= static_cast<std::int32_t>(buffer[offset++]) << 16;
    value |= static_cast<std::int32_t>(buffer[offset++]) << 24;
    return value;
}


/**
 * @brief Serializes CellSpace data.
 */
std::vector<std::uint8_t> SnapshotManager::serializeCellSpace(const CellSpace& cellSpace) const {
    std::vector<std::uint8_t> serializedData;

    Point minBounds = cellSpace.getMinBounds();
    Point maxBounds = cellSpace.getMaxBounds();
    bool boundsValid = cellSpace.areBoundsInitialized();

    // If bounds are not valid (e.g. empty grid), store some default/marker values.
    // For simplicity, we'll store them anyway; loadActiveCells can handle empty maps.
    writeInt32(serializedData, boundsValid ? minBounds.x : 0);
    writeInt32(serializedData, boundsValid ? minBounds.y : 0);
    writeInt32(serializedData, boundsValid ? maxBounds.x : 0);
    writeInt32(serializedData, boundsValid ? maxBounds.y : 0);


    const auto& activeCells = cellSpace.getActiveCells();
    std::uint32_t numActiveCells = static_cast<std::uint32_t>(activeCells.size());
    // Write numActiveCells as int32 for consistency with other int32 fields, though it's unsigned.
    writeInt32(serializedData, static_cast<std::int32_t>(numActiveCells));

    for (const auto& pair : activeCells) {
        const Point& p = pair.first;
        int state = pair.second;
        writeInt32(serializedData, p.x);
        writeInt32(serializedData, p.y);
        writeInt32(serializedData, state); // Assuming state fits in int32_t
    }
    return serializedData;
}

/**
 * @brief Deserializes data into CellSpace.
 */
bool SnapshotManager::deserializeCellSpace(const std::vector<std::uint8_t>& data, CellSpace& cellSpace) const {
    cellSpace.clear(); // Start with a clean slate

    size_t offset = 0;
    try {
        Point minBounds, maxBounds;
        minBounds.x = readInt32(data, offset);
        minBounds.y = readInt32(data, offset);
        maxBounds.x = readInt32(data, offset);
        maxBounds.y = readInt32(data, offset);

        std::uint32_t numActiveCells = static_cast<std::uint32_t>(readInt32(data, offset));

        std::map<Point, int> loadedCells;
        for (std::uint32_t i = 0; i < numActiveCells; ++i) {
            Point p;
            p.x = readInt32(data, offset);
            p.y = readInt32(data, offset);
            int state = readInt32(data, offset);
            loadedCells[p] = state;
        }

        // Check if we consumed exactly the right amount of data (optional, good for validation)
        if (offset != data.size()) {
            ErrorHandler::logError("SnapshotManager: Deserialization - Trailing data or incomplete read. Offset: " +
                                   std::to_string(offset) + ", Data size: " + std::to_string(data.size()));
            // Depending on strictness, this could be an error.
        }

        // If numActiveCells is 0, min/maxBounds might be defaults (e.g., 0,0,0,0)
        // CellSpace::loadActiveCells should correctly initialize bounds if loadedCells is empty.
        // If loadedCells is not empty, these bounds should be the actual bounds of the loaded set.
        cellSpace.loadActiveCells(loadedCells, minBounds, maxBounds);

    } catch (const std::out_of_range& e) {
        ErrorHandler::logError("SnapshotManager: Deserialization error - " + std::string(e.what()));
        cellSpace.clear(); // Ensure cellspace is empty on error
        return false;
    } catch (...) {
        ErrorHandler::logError("SnapshotManager: Unknown deserialization error.");
        cellSpace.clear();
        return false;
    }

    return true;
}


/**
 * @brief Saves the CellSpace state to a file.
 */
bool SnapshotManager::saveState(const std::string& filePath, const CellSpace& cellSpace) {
    std::string actualFilePath = filePath;
    // Ensure .snapshot extension (optional, but good practice from goal.md)
    if (actualFilePath.length() < 10 || actualFilePath.substr(actualFilePath.length() - 9) != ".snapshot") {
        actualFilePath += ".snapshot";
    }

    // 1. Serialize CellSpace data
    std::vector<std::uint8_t> serialized_data = serializeCellSpace(cellSpace);
    if (serialized_data.empty() && !cellSpace.getActiveCells().empty()) { // Check if serialization itself failed for non-empty
        ErrorHandler::logError("SnapshotManager: Serialization produced empty data for non-empty cell space during save.");
        // This check might be too simplistic if serializeCellSpace can legitimately return empty for some valid states.
        // The current serializeCellSpace always returns at least bounds + num_cells.
    }


    // 2. Compress the serialized data
    std::vector<std::uint8_t> compressed_data = HuffmanCoding::compress(serialized_data);
    if (compressed_data.empty() && !serialized_data.empty()) { // Compression failed
        ErrorHandler::logError("SnapshotManager: Huffman compression failed or returned empty for non-empty serialized data.");
        return false;
    }
    // If original data was empty, compressed_data will contain only original_size=0.

    // 3. Write compressed data to file
    std::ofstream outFile(actualFilePath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        ErrorHandler::logError("SnapshotManager: Failed to open file for saving: " + actualFilePath);
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());
    if (outFile.fail()) {
        ErrorHandler::logError("SnapshotManager: Failed to write data to file: " + actualFilePath);
        outFile.close();
        return false;
    }

    outFile.close();
    ErrorHandler::logError("SnapshotManager: State saved successfully to " + actualFilePath, false);
    return true;
}

/**
 * @brief Loads CellSpace state from a file.
 */
bool SnapshotManager::loadState(const std::string& filePath, CellSpace& cellSpace) {
    // 1. Read compressed data from file
    std::ifstream inFile(filePath, std::ios::binary | std::ios::ate); // ate to get size
    if (!inFile.is_open()) {
        ErrorHandler::logError("SnapshotManager: Failed to open file for loading: " + filePath);
        return false;
    }

    std::streamsize size = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    if (size == 0) {
        ErrorHandler::logError("SnapshotManager: Snapshot file is empty: " + filePath);
        // This could be a valid empty state if compress() handles it.
        // HuffmanCoding::compress for empty data returns just original_size=0.
        // HuffmanCoding::decompress should handle this.
    }

    std::vector<std::uint8_t> compressed_data(size);
    if (!inFile.read(reinterpret_cast<char*>(compressed_data.data()), size)) {
        ErrorHandler::logError("SnapshotManager: Failed to read data from file: " + filePath);
        inFile.close();
        return false;
    }
    inFile.close();

    // 2. Decompress the data
    std::vector<std::uint8_t> serialized_data = HuffmanCoding::decompress(compressed_data);
    if (serialized_data.empty() && !compressed_data.empty() &&
        !(compressed_data.size() == sizeof(uint64_t) && *reinterpret_cast<const uint64_t*>(compressed_data.data()) == 0) ) {
        // Decompression failed, and it wasn't just an empty stream representing empty original data
        ErrorHandler::logError("SnapshotManager: Huffman decompression failed or returned empty for non-empty compressed data from file: " + filePath);
        return false;
    }


    // 3. Deserialize data into CellSpace
    if (!deserializeCellSpace(serialized_data, cellSpace)) {
        ErrorHandler::logError("SnapshotManager: Failed to deserialize cell space data from file: " + filePath);
        return false; // Deserialization handles clearing cellSpace on error
    }

    ErrorHandler::logError("SnapshotManager: State loaded successfully from " + filePath, false);
    return true;
}
