#ifndef CELL_SPACE_H
#define CELL_SPACE_H

#include <vector>
#include <unordered_map>
#include "../utils/point.h"
#include <limits>

/**
 * @class CellSpace
 * @brief Manages the 2D grid of cells for the cellular automaton.
 */
class CellSpace {
private:
    std::unordered_map<Point, int> activeCells_;
    int defaultState_;

    Point minGridBounds_;
    Point maxGridBounds_;
    bool boundsInitialized_;

    void updateBounds(Point coordinates);
    void recalculateBounds();

public:
    CellSpace(int defaultState);

    int getCellState(Point coordinates) const;
    void setCellState(Point coordinates, int state);
    std::vector<int> getNeighborStates(Point centerCoordinates, const std::vector<Point>& neighborhoodDefinition) const;

    /**
     * @brief Applies a map of pending state changes to the grid.
     * This is typically called after the RuleEngine calculates the next generation.
     * @param cellsToUpdate An unordered_map where keys are Point coordinates
     * of cells to change, and values are their new integer states.
     */
    void updateCells(const std::unordered_map<Point, int>& cellsToUpdate);

    const std::unordered_map<Point, int>& getActiveCells() const;
    void loadActiveCells(const std::unordered_map<Point, int>& cells, Point minBounds, Point maxBounds);

    Point getMinBounds() const;
    Point getMaxBounds() const;
    bool areBoundsInitialized() const;

    void clear();
    int getDefaultState() const;
};

#endif // CELL_SPACE_H
