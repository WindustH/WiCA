#include "cell_space.h"
#include <algorithm> // For std::min and std::max
#include <iostream>  // For std::cout debugging
#include <unordered_map> // Ensure it's included

/**
 * @brief Constructor for CellSpace.
 * @param defaultState The default state for cells in the grid.
 */
CellSpace::CellSpace(int defState) : defaultState_(defState), boundsInitialized_(false) {
    minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
    // std::cout << "[DEBUG] CellSpace Constructor: DefaultState=" << defaultState_ << std::endl;
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
    } else {
        if (coordinates.x < minGridBounds_.x) { minGridBounds_.x = coordinates.x; }
        if (coordinates.y < minGridBounds_.y) { minGridBounds_.y = coordinates.y; }
        if (coordinates.x > maxGridBounds_.x) { maxGridBounds_.x = coordinates.x; }
        if (coordinates.y > maxGridBounds_.y) { maxGridBounds_.y = coordinates.y; }
    }
}

/**
 * @brief Recalculates the entire bounding box based on current active cells.
 */
void CellSpace::recalculateBounds() {
    boundsInitialized_ = false;
    minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());

    if (activeCells_.empty()) {
        return;
    }

    for (const auto& pair : activeCells_) {
        updateBounds(pair.first);
    }
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
    auto it = activeCells_.find(coordinates);
    int currentState = (it != activeCells_.end()) ? it->second : defaultState_;

    if (state == defaultState_) {
        if (currentState != defaultState_) {
            activeCells_.erase(coordinates);
            if (activeCells_.empty()) {
                 boundsInitialized_ = false;
                 minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
                 maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
            } else if (boundsInitialized_ && (coordinates.x == minGridBounds_.x || coordinates.x == maxGridBounds_.x ||
                       coordinates.y == minGridBounds_.y || coordinates.y == maxGridBounds_.y)) {
                 recalculateBounds();
            }
        }
    } else {
        activeCells_[coordinates] = state;
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
 * @brief Applies a map of pending state changes to the grid.
 * @param cellsToUpdate An unordered_map where keys are Point coordinates
 * of cells to change, and values are their new integer states.
 */
void CellSpace::updateCells(const std::unordered_map<Point, int>& cellsToUpdate) {
    if (cellsToUpdate.empty()) return;

    bool boundaryPotentiallyAffected = false;

    for (const auto& cellUpdatePair : cellsToUpdate) {
        const Point& coords = cellUpdatePair.first;
        int newState = cellUpdatePair.second;

        auto it = activeCells_.find(coords);
        int currentState = (it != activeCells_.end()) ? it->second : defaultState_;

        if (currentState == newState) continue; // No change needed for this cell

        if (newState == defaultState_) { // Cell is becoming inactive
            if (it != activeCells_.end()) { // Was active
                activeCells_.erase(it);
                if (boundsInitialized_ &&
                    (coords.x == minGridBounds_.x || coords.x == maxGridBounds_.x ||
                     coords.y == minGridBounds_.y || coords.y == maxGridBounds_.y)) {
                    boundaryPotentiallyAffected = true;
                }
            }
        } else { // Cell is becoming active or changing its active state
            activeCells_[coords] = newState;
            updateBounds(coords); // This will also set boundsInitialized_ if it's the first active cell
            // If a cell is added/modified, it might expand bounds or be within current bounds.
            // If it expands, updateBounds handles it. If it's within, existing boundary cells are still valid unless removed.
            // We set boundaryPotentiallyAffected to true here as well, because if a new active cell
            // is placed *on* an existing boundary, and a *different* boundary cell was removed,
            // a simple check for removed boundary cells might not be enough.
            // A safer approach is to mark true if any change happens that might touch boundaries.
            if (boundsInitialized_ &&
                (coords.x <= minGridBounds_.x || coords.x >= maxGridBounds_.x ||
                 coords.y <= minGridBounds_.y || coords.y >= maxGridBounds_.y)) {
                 boundaryPotentiallyAffected = true; // If change is at or beyond current bounds
            }
        }
    }

    if (activeCells_.empty()) {
        boundsInitialized_ = false;
        minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
        maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
    } else if (boundaryPotentiallyAffected) {
        // If any operation (addition or removal) could have affected the boundary,
        // it's safest to recalculate. UpdateBounds only expands.
        recalculateBounds();
    }
}


const std::unordered_map<Point, int>& CellSpace::getActiveCells() const {
    return activeCells_;
}

void CellSpace::loadActiveCells(const std::unordered_map<Point, int>& cells, Point minB, Point maxB) {
    activeCells_ = cells;
    if (activeCells_.empty()) {
        boundsInitialized_ = false;
        minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
        maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
    } else {
        minGridBounds_ = minB;
        maxGridBounds_ = maxB;
        boundsInitialized_ = true;
        // For absolute certainty, especially if minB/maxB from file could be unreliable for the given cells:
        // recalculateBounds();
    }
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
}

int CellSpace::getDefaultState() const {
    return defaultState_;
}
