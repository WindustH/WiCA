#include "cell_space.h"
#include <algorithm> // For std::min and std::max
#include <iostream>  // For std::cout debugging

/**
 * @brief Constructor for CellSpace.
 * @param defaultState The default state for cells in the grid.
 */
CellSpace::CellSpace(int defState) : defaultState_(defState), boundsInitialized_(false) {
    minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
    std::cout << "[DEBUG] CellSpace Constructor: DefaultState=" << defaultState_ << std::endl;
}

/**
 * @brief Updates the minimum and maximum grid bounds.
 * @param coordinates The coordinates of the cell that might expand the bounds.
 */
void CellSpace::updateBounds(Point coordinates) {
    if (!boundsInitialized_) {
        minGridBounds_ = coordinates;
        maxGridBounds_ = coordinates;
        boundsInitialized_ = true;
        // std::cout << "[DEBUG] CellSpace::updateBounds - Initialized to (" << coordinates.x << "," << coordinates.y << ")" << std::endl;
    } else {
        bool changed = false;
        if (coordinates.x < minGridBounds_.x) { minGridBounds_.x = coordinates.x; changed = true; }
        if (coordinates.y < minGridBounds_.y) { minGridBounds_.y = coordinates.y; changed = true; }
        if (coordinates.x > maxGridBounds_.x) { maxGridBounds_.x = coordinates.x; changed = true; }
        if (coordinates.y > maxGridBounds_.y) { maxGridBounds_.y = coordinates.y; changed = true; }
        // if (changed) {
        //     std::cout << "[DEBUG] CellSpace::updateBounds - Updated: Min(" << minGridBounds_.x << "," << minGridBounds_.y
        //               << ") Max(" << maxGridBounds_.x << "," << maxGridBounds_.y << ")" << std::endl;
        // }
    }
}

/**
 * @brief Recalculates the entire bounding box based on current active cells.
 */
void CellSpace::recalculateBounds() {
    // std::cout << "[DEBUG] CellSpace::recalculateBounds called. Active cells: " << activeCells_.size() << std::endl;
    boundsInitialized_ = false; // Reset before recalculating
    minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());

    if (activeCells_.empty()) {
        // std::cout << "[DEBUG] CellSpace::recalculateBounds - No active cells, bounds remain uninitialized." << std::endl;
        return;
    }

    for (const auto& pair : activeCells_) {
        updateBounds(pair.first); // updateBounds will set boundsInitialized_ to true on the first cell
    }
    // std::cout << "[DEBUG] CellSpace::recalculateBounds - Finished. Min(" << minGridBounds_.x << "," << minGridBounds_.y
    //           << ") Max(" << maxGridBounds_.x << "," << maxGridBounds_.y << ") Initialized: " << boundsInitialized_ << std::endl;
}


/**
 * @brief Gets the state of a cell.
 * @param coordinates The (x, y) coordinates.
 * @return The cell's state or defaultState_ if not active.
 */
int CellSpace::getCellState(Point coordinates) const {
    auto it = activeCells_.find(coordinates);
    if (it != activeCells_.end()) {
        return it->second;
    }
    return defaultState_;
}

/**
 * @brief Sets the state of a cell.
 * @param coordinates The (x, y) coordinates.
 * @param state The new state.
 */
void CellSpace::setCellState(Point coordinates, int state) {
    // std::cout << "[DEBUG] CellSpace::setCellState: Coords(" << coordinates.x << "," << coordinates.y << "), NewState=" << state << ", DefaultState=" << defaultState_ << std::endl;
    int currentState = getCellState(coordinates);

    if (state == defaultState_) {
        if (currentState != defaultState_) { // Only erase if it was active
            activeCells_.erase(coordinates);
            // std::cout << "  [CellSpace] Cell (" << coordinates.x << "," << coordinates.y << ") set to default. Removed. Active cells: " << activeCells_.size() << std::endl;
            if (activeCells_.empty()) {
                 boundsInitialized_ = false;
                 minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
                 maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
            } else if (coordinates.x == minGridBounds_.x || coordinates.x == maxGridBounds_.x ||
                       coordinates.y == minGridBounds_.y || coordinates.y == maxGridBounds_.y) {
                 recalculateBounds();
            }
        }
    } else { // Setting to a non-default (active) state
        activeCells_[coordinates] = state;
        // std::cout << "  [CellSpace] Cell (" << coordinates.x << "," << coordinates.y << ") set to " << state << ". Added/Updated. Active cells: " << activeCells_.size() << std::endl;
        updateBounds(coordinates);
    }
}

/**
 * @brief Collects states of neighboring cells.
 * @param centerCoordinates Coordinates of the central cell.
 * @param neighborhoodDefinition Relative offsets of neighbors.
 * @return Vector of neighbor states.
 */
std::vector<int> CellSpace::getNeighborStates(Point centerCoordinates, const std::vector<Point>& neighborhoodDefinition) const {
    std::vector<int> neighborStates;
    neighborStates.reserve(neighborhoodDefinition.size());

    for (const Point& offset : neighborhoodDefinition) {
        neighborStates.push_back(getCellState(centerCoordinates + offset));
    }
    return neighborStates;
}

/**
 * @brief Applies pending state changes.
 * @param cellsToUpdate Vector of (Point, newState) pairs.
 */
void CellSpace::updateCells(const std::vector<std::pair<Point, int>>& cellsToUpdate) {
    if (cellsToUpdate.empty()) return;
    // std::cout << "[DEBUG] CellSpace::updateCells - Updating " << cellsToUpdate.size() << " cells." << std::endl;

    bool removedCellMightAffectBounds = false;
    bool addedCell = false;

    for (const auto& cellUpdate : cellsToUpdate) {
        const Point& coords = cellUpdate.first;
        int newState = cellUpdate.second;
        int currentState = getCellState(coords);

        if (currentState == newState) continue;

        if (newState == defaultState_) {
            if (activeCells_.erase(coords)) { // erase returns number of elements removed (0 or 1 for map)
                // std::cout << "  [CellSpace Update] Cell (" << coords.x << "," << coords.y << ") set to default. Removed." << std::endl;
                if (coords.x == minGridBounds_.x || coords.x == maxGridBounds_.x ||
                    coords.y == minGridBounds_.y || coords.y == maxGridBounds_.y) {
                    removedCellMightAffectBounds = true;
                }
            }
        } else {
            activeCells_[coords] = newState;
            // std::cout << "  [CellSpace Update] Cell (" << coords.x << "," << coords.y << ") set to " << newState << ". Added/Updated." << std::endl;
            updateBounds(coords);
            addedCell = true; // An active cell was added/changed, bounds might have expanded.
        }
    }

    if (activeCells_.empty()) {
        boundsInitialized_ = false;
        minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
        maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
        // std::cout << "[DEBUG] CellSpace::updateCells - All cells cleared, bounds reset." << std::endl;
    } else if (removedCellMightAffectBounds && !addedCell) {
        // If only removals happened and one of them was a boundary cell, recalculate.
        // If additions also happened, updateBounds would have handled expansions.
        // This logic is to catch shrinking bounds correctly.
        recalculateBounds();
    }
    // std::cout << "[DEBUG] CellSpace::updateCells - Finished. Active cells: " << activeCells_.size() << ". Bounds Initialized: " << boundsInitialized_ << std::endl;
}


const std::map<Point, int>& CellSpace::getActiveCells() const {
    return activeCells_;
}

void CellSpace::loadActiveCells(const std::map<Point, int>& cells, Point minB, Point maxB) {
    activeCells_ = cells;
    std::cout << "[DEBUG] CellSpace::loadActiveCells - Loaded " << activeCells_.size() << " cells." << std::endl;
    if (activeCells_.empty()) {
        boundsInitialized_ = false;
        minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
        maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
    } else {
        // Trust the bounds from the snapshot if they are provided and seem valid.
        // For robustness, one might validate or always recalculate.
        // For now, assume minB and maxB from snapshot are correct if cells is not empty.
        minGridBounds_ = minB;
        maxGridBounds_ = maxB;
        boundsInitialized_ = true;
        // Alternatively, to be absolutely sure:
        // recalculateBounds();
    }
     std::cout << "  [CellSpace Load] Min(" << minGridBounds_.x << "," << minGridBounds_.y
              << ") Max(" << maxGridBounds_.x << "," << maxGridBounds_.y << ") Initialized: " << boundsInitialized_ << std::endl;
}

Point CellSpace::getMinBounds() const {
    if (!boundsInitialized_) {
        return Point(0,0);
    }
    return minGridBounds_;
}

Point CellSpace::getMaxBounds() const {
     if (!boundsInitialized_) {
        return Point(0,0);
    }
    return maxGridBounds_;
}

bool CellSpace::areBoundsInitialized() const {
    return boundsInitialized_;
}

void CellSpace::clear() {
    activeCells_.clear();
    boundsInitialized_ = false;
    minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
    std::cout << "[DEBUG] CellSpace::clear - All cells cleared, bounds reset." << std::endl;
}

int CellSpace::getDefaultState() const {
    return defaultState_;
}
