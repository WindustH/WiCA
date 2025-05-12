#ifndef CELL_SPACE_H
#define CELL_SPACE_H

#include <map>
#include <vector>
#include <unordered_map> // For potentially using unordered_map with custom Point hash
#include "../utils/point.h"
#include <limits> // Required for std::numeric_limits

/**
 * @class CellSpace
 * @brief Manages the 2D grid of cells for the cellular automaton.
 *
 * This class is responsible for storing the states of cells, supporting a
 * dynamically expanding grid, and providing methods to access and modify
 * cell states. It efficiently stores only non-default state cells.
 */
class CellSpace {
private:
    // Using std::map for ordered iteration if needed, and because Point has operator<.
    // std::unordered_map<Point, int> could be used with the custom hash for Point for potentially faster lookups.
    std::map<Point, int> activeCells_; // Stores only cells not in the default state.
    int defaultState_;                 // The state of cells not present in activeCells_.

    // Bounding box of active cells. Initialized to be invalid.
    Point minGridBounds_;
    Point maxGridBounds_;
    bool boundsInitialized_;           // Flag to track if bounds have been set at least once.

    /**
     * @brief Updates the minimum and maximum grid bounds based on a new or modified cell's coordinates.
     * @param coordinates The coordinates of the cell that might expand the bounds.
     */
    void updateBounds(Point coordinates);

    /**
     * @brief Recalculates the entire bounding box based on current active cells.
     * Used after loading or clearing cells, or if a cell is set back to default.
     */
    void recalculateBounds();

public:
    /**
     * @brief Constructor for CellSpace.
     * @param defaultState The default state for cells in the grid.
     */
    CellSpace(int defaultState);

    /**
     * @brief Gets the state of a cell at the given coordinates.
     * @param coordinates The (x, y) coordinates of the cell.
     * @return The state of the cell. Returns defaultState_ if the cell is not explicitly stored.
     */
    int getCellState(Point coordinates) const;

    /**
     * @brief Sets the state of a cell at the given coordinates.
     * If the new state is the defaultState_, the cell might be removed from active storage.
     * This method also handles dynamic grid expansion by updating the bounds.
     * @param coordinates The (x, y) coordinates of the cell.
     * @param state The new state for the cell.
     */
    void setCellState(Point coordinates, int state);

    /**
     * @brief Collects the states of neighboring cells according to a given neighborhood definition.
     * @param centerCoordinates The coordinates of the cell whose neighbors are being queried.
     * @param neighborhoodDefinition A vector of Point offsets defining the relative positions of neighbors.
     * @return A vector of integers representing the states of the neighboring cells, in the order
     * defined by neighborhoodDefinition.
     */
    std::vector<int> getNeighborStates(Point centerCoordinates, const std::vector<Point>& neighborhoodDefinition) const;

    /**
     * @brief Applies a list of pending state changes to the grid.
     * This is typically called after the RuleEngine calculates the next generation.
     * @param cellsToUpdate A vector of pairs, where each pair contains the Point coordinates
     * of a cell and its new integer state.
     */
    void updateCells(const std::vector<std::pair<Point, int>>& cellsToUpdate);

    /**
     * @brief Returns a constant reference to the map of active (non-default) cells.
     * Useful for rendering, saving, or iterating through only the active cells.
     * @return const std::map<Point, int>& A reference to the active cells map.
     */
    const std::map<Point, int>& getActiveCells() const;

    /**
     * @brief Loads a set of active cells into the cell space.
     * This typically happens when loading a state from a snapshot. The existing
     * active cells are cleared before loading the new set.
     * @param cells A map of Point coordinates to integer states for the cells to load.
     * @param minBounds The minimum bounds of the loaded cell configuration.
     * @param maxBounds The maximum bounds of the loaded cell configuration.
     */
    void loadActiveCells(const std::map<Point, int>& cells, Point minBounds, Point maxBounds);

    /**
     * @brief Gets the minimum (bottom-left) coordinates of the bounding box of active cells.
     * If no cells are active, the returned Point might represent an invalid or default state.
     * Check boundsInitialized_ or if minGridBounds_ == maxGridBounds_ in a specific way.
     * @return Point The minimum x, y coordinates.
     */
    Point getMinBounds() const;

    /**
     * @brief Gets the maximum (top-right) coordinates of the bounding box of active cells.
     * If no cells are active, the returned Point might represent an invalid or default state.
     * @return Point The maximum x, y coordinates.
     */
    Point getMaxBounds() const;

    /**
     * @brief Checks if the grid bounds have been initialized (i.e., if there's at least one active cell or bounds were set).
     * @return True if bounds are initialized, false otherwise.
     */
    bool areBoundsInitialized() const;


    /**
     * @brief Clears all active cells from the space, resetting it to an empty state
     * where all cells are effectively defaultState_. Resets bounds.
     */
    void clear();

    /**
     * @brief Gets the default state of the grid.
     * @return The default cell state.
     */
    int getDefaultState() const;
};

#endif // CELL_SPACE_H
