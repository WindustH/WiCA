#ifndef CELL_SPACE_H
#define CELL_SPACE_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "../utils/point.h"
#include <limits>
/**
 * @class CellSpace
 * @brief Manages the 2D grid of cells for the cellular automaton.
 */
class CellSpace {
private:
    std::unordered_map<Point, int> nonDefaultCells_;
    std::unordered_set<Point> cellsToEvaluate_;

    int defaultState_;

    bool boundsInitialized_;

    std::vector<Point> neighborhood_;
    std::vector<Point> reverseNeighborhood_;

    Point minGridBounds_;
    Point maxGridBounds_;

    void updateBounds(Point coordinates);
    void recalculateBounds();

public:
    CellSpace(int defaultState, std::vector<Point> neighborhood);

    int getCellState(Point coordinates) const;
    void setCellState(Point coordinates, int state);
    std::vector<int> getNeighborStates(Point centerCoordinates) const;

    /**
     * @brief Applies a map of pending state changes to the grid.
     * This is typically called after the RuleEngine calculates the next generation.
     * @param cellsToUpdate An unordered_map where keys are Point coordinates
     * of cells to change, and values are their new integer states.
     */
    void updateCells(const std::unordered_map<Point, int>& cellsToUpdate);

    const std::unordered_map<Point, int>& getNonDefaultCells() const;
    const std::unordered_set<Point>& getCellsToEvaluate() const;
    void loadCells(const std::unordered_map<Point, int>& cells, Point minBounds, Point maxBounds);

    Point getMinBounds() const;
    Point getMaxBounds() const;
    bool areBoundsInitialized() const;

    void clear();
    void clearCellsToEvaluate();
    int getDefaultState() const;
};

#endif // CELL_SPACE_H
