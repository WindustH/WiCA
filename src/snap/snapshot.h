#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <string>
#include <vector>
#include <cstdint> // For uint types

// Forward declarations
class CellSpace; // Manages the grid data to be saved/loaded
namespace HuffmanCoding { } // Namespace for compression utilities

/**
 * @class SnapshotManager
 * @brief Handles saving and loading of the cell space state to/from custom binary snapshot files.
 *
 * This class serializes the active cells and their states, along with grid boundary
 * information, into a byte stream. This stream is then compressed using Huffman coding
 * before being written to a file. The reverse process (decompression and deserialization)
 * is handled when loading a snapshot.
 */
class SnapshotManager {
public:
    /**
     * @brief Default constructor.
     */
    SnapshotManager();

    /**
     * @brief Saves the current state of the given CellSpace to a specified file.
     * The data is serialized, compressed using Huffman coding, and then written.
     * @param filePath The path to the file where the snapshot will be saved.
     * The extension ".snapshot" will be appended if not present.
     * @param cellSpace A constant reference to the CellSpace whose state is to be saved.
     * @return True if saving was successful, false otherwise. Errors are logged.
     */
    bool saveState(const std::string& filePath, const CellSpace& cellSpace);

    /**
     * @brief Loads a cell space state from a specified snapshot file into the given CellSpace.
     * The file data is read, decompressed using Huffman coding, and then deserialized
     * to reconstruct the cell states and grid boundaries.
     * @param filePath The path to the snapshot file to load.
     * @param cellSpace A reference to the CellSpace object that will be populated with the loaded state.
     * Any existing state in cellSpace will be cleared.
     * @return True if loading was successful, false otherwise. Errors are logged.
     */
    bool loadState(const std::string& filePath, CellSpace& cellSpace);

private:
    // Helper methods for serialization and deserialization

    /**
     * @brief Serializes CellSpace data into a byte vector.
     * Format:
     * - MinBounds.x (int32_t)
     * - MinBounds.y (int32_t)
     * - MaxBounds.x (int32_t)
     * - MaxBounds.y (int32_t)
     * - Number of active cells (uint32_t)
     * - For each active cell:
     * - Point.x (int32_t)
     * - Point.y (int32_t)
     * - State (int32_t, though states are often smaller, using int32 for simplicity)
     * @param cellSpace The CellSpace to serialize.
     * @return A vector of bytes representing the serialized data.
     */
    std::vector<std::uint8_t> serializeCellSpace(const CellSpace& cellSpace) const;

    /**
     * @brief Deserializes data from a byte vector into a CellSpace.
     * @param data The byte vector containing serialized CellSpace data.
     * @param cellSpace The CellSpace object to populate.
     * @return True if deserialization was successful, false otherwise (e.g., data corruption).
     */
    bool deserializeCellSpace(const std::vector<std::uint8_t>& data, CellSpace& cellSpace) const;

    // Helper to write a 32-bit integer to a byte vector (little-endian)
    void writeInt32(std::vector<std::uint8_t>& buffer, std::int32_t value) const;
    // Helper to read a 32-bit integer from a byte vector (little-endian)
    std::int32_t readInt32(const std::vector<std::uint8_t>& buffer, size_t& offset) const;
};

#endif // SNAPSHOT_MANAGER_H
