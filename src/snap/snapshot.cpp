#include "snapshot.h"
#include "../ca/cell_space.h"
#include "huffman_coding.h"
#include "../utils/logger.h" // New logger
#include "../utils/point.h" // For Point struct and std::hash<Point>

#include <fstream>   // For file I/O (std::ofstream, std::ifstream)
#include <iterator>  // For std::istreambuf_iterator
#include <unordered_map> // Required for std::unordered_map

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
    auto logger = Logger::getLogger(Logger::Module::Snapshot);
    if (offset + sizeof(std::int32_t) > buffer.size()) {
        if (logger) logger->error("ReadInt32 - Read out of bounds.");
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

    writeInt32(serializedData, boundsValid ? minBounds.x : 0);
    writeInt32(serializedData, boundsValid ? minBounds.y : 0);
    writeInt32(serializedData, boundsValid ? maxBounds.x : 0);
    writeInt32(serializedData, boundsValid ? maxBounds.y : 0);


    const auto& activeCells = cellSpace.getNonDefaultCells(); // This is std::unordered_map<Point, int>
    std::uint32_t numActiveCells = static_cast<std::uint32_t>(activeCells.size());
    writeInt32(serializedData, static_cast<std::int32_t>(numActiveCells));

    for (const auto& pair : activeCells) {
        const Point& p = pair.first;
        int state = pair.second;
        writeInt32(serializedData, p.x);
        writeInt32(serializedData, p.y);
        writeInt32(serializedData, state);
    }
    return serializedData;
}

/**
 * @brief Deserializes data into CellSpace.
 */
bool SnapshotManager::deserializeCellSpace(const std::vector<std::uint8_t>& data, CellSpace& cellSpace) const {
    auto logger = Logger::getLogger(Logger::Module::Snapshot);
    cellSpace.clear();

    size_t offset = 0;
    try {
        Point minBounds, maxBounds;
        minBounds.x = readInt32(data, offset);
        minBounds.y = readInt32(data, offset);
        maxBounds.x = readInt32(data, offset);
        maxBounds.y = readInt32(data, offset);

        std::uint32_t numActiveCells = static_cast<std::uint32_t>(readInt32(data, offset));

        std::unordered_map<Point, int> loadedCells; // No explicit PointHash needed
        loadedCells.reserve(numActiveCells);

        for (std::uint32_t i = 0; i < numActiveCells; ++i) {
            Point p;
            p.x = readInt32(data, offset);
            p.y = readInt32(data, offset);
            int state = readInt32(data, offset);
            loadedCells[p] = state;
        }

        if (offset != data.size()) {
            if (logger) logger->error("Deserialization - Trailing data or incomplete read. Offset: " +
                                   std::to_string(offset) + ", Data size: " + std::to_string(data.size()));
        }

        cellSpace.loadCells(loadedCells, minBounds, maxBounds);

    } catch (const std::out_of_range& e) {
        if (logger) logger->error("Deserialization error - " + std::string(e.what()));
        cellSpace.clear();
        return false;
    } catch (...) {
        if (logger) logger->error("Unknown deserialization error.");
        cellSpace.clear();
        return false;
    }

    return true;
}


/**
 * @brief Saves the CellSpace state to a file.
 */
bool SnapshotManager::saveState(const std::string& filePath, const CellSpace& cellSpace) {
    auto logger = Logger::getLogger(Logger::Module::Snapshot);
    std::string actualFilePath = filePath;
    if (actualFilePath.length() < 10 || actualFilePath.substr(actualFilePath.length() - 9) != ".snapshot") {
        actualFilePath += ".snapshot";
    }

    std::vector<std::uint8_t> serialized_data = serializeCellSpace(cellSpace);
    if (serialized_data.empty() && !cellSpace.getNonDefaultCells().empty()) {
        if (logger) logger->error("Serialization produced empty data for non-empty cell space during save.");
    }

    std::vector<std::uint8_t> compressed_data = HuffmanCoding::compress(serialized_data);
    if (compressed_data.empty() && !serialized_data.empty()) {
        if (logger) logger->error("Huffman compression failed or returned empty for non-empty serialized data.");
        return false;
    }

    std::ofstream outFile(actualFilePath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        if (logger) logger->error("Failed to open file for saving: " + actualFilePath);
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());
    if (outFile.fail()) {
        if (logger) logger->error("Failed to write data to file: " + actualFilePath);
        outFile.close();
        return false;
    }

    outFile.close();
    if (logger) logger->error("State saved successfully to " + actualFilePath);
    return true;
}

/**
 * @brief Loads CellSpace state from a file.
 */
bool SnapshotManager::loadState(const std::string& filePath, CellSpace& cellSpace) {
    auto logger = Logger::getLogger(Logger::Module::Snapshot);
    std::ifstream inFile(filePath, std::ios::binary | std::ios::ate);
    if (!inFile.is_open()) {
        if (logger) logger->error("Failed to open file for loading: " + filePath);
        return false;
    }

    std::streamsize size = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    if (size == 0) {
        if (logger) logger->error("Snapshot file is empty: " + filePath);
    }

    std::vector<std::uint8_t> compressed_data(size);
    if (!inFile.read(reinterpret_cast<char*>(compressed_data.data()), size)) {
        if (logger) logger->error("Failed to read data from file: " + filePath);
        inFile.close();
        return false;
    }
    inFile.close();

    std::vector<std::uint8_t> serialized_data = HuffmanCoding::decompress(compressed_data);
    if (serialized_data.empty() && !compressed_data.empty() &&
        !(compressed_data.size() == sizeof(uint64_t) && *reinterpret_cast<const uint64_t*>(compressed_data.data()) == 0) ) {
        if (logger) logger->error("Huffman decompression failed or returned empty for non-empty compressed data from file: " + filePath);
        return false;
    }

    if (!deserializeCellSpace(serialized_data, cellSpace)) {
        if (logger) logger->error("Failed to deserialize cell space data from file: " + filePath);
        return false;
    }

    if (logger) logger->error("State loaded successfully from " + filePath);
    return true;
}
