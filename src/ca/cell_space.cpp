#include "cell_space.h"
#include <algorithm> // For std::min and std::max
#include <iostream>  // For std::cout debugging
#include <unordered_map> // Ensure it's included
#include <set>
/**
 * @brief Constructor for CellSpace.
 * @param defaultState The default state for cells in the grid.
 */
CellSpace::CellSpace(int defState, std::vector<Point> neighborhood)
    : defaultState_(defState),
    boundsInitialized_(false),
    neighborhood_(neighborhood)
    {
    minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());

    std::set<Point> generalNeighborhoodSet;
    for (Point cell : neighborhood) {
        for (Point offset : neighborhood) {
            generalNeighborhoodSet.insert(cell + offset);
        }
    }
    reverseNeighborhood_.assign(generalNeighborhoodSet.begin(), generalNeighborhoodSet.end());
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

    if (nonDefaultCells_.empty()) {
        return;
    }

    for (const auto& pair : nonDefaultCells_) {
        updateBounds(pair.first);
    }
}


/**
 * @brief Gets the state of a cell.
 * @param coordinates The (x, y) coordinates.
 * @return The cell's state or defaultState_ if not active.
 */
int CellSpace::getCellState(Point coordinates) const {
    auto it = nonDefaultCells_.find(coordinates);
    if (it != nonDefaultCells_.end()) {
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
    auto it = nonDefaultCells_.find(coordinates);
    int currentState = (it != nonDefaultCells_.end()) ? it->second : defaultState_;
    if(state!=currentState){
        for(Point offset : reverseNeighborhood_){
            cellsToEvaluate_.insert(coordinates+offset);
        }
    }
    if (state == defaultState_) {
        if (currentState != defaultState_) {
            nonDefaultCells_.erase(coordinates);
            if (nonDefaultCells_.empty()) {
                 boundsInitialized_ = false;
                 minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
                 maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
            } else if (boundsInitialized_ && (coordinates.x == minGridBounds_.x || coordinates.x == maxGridBounds_.x ||
                       coordinates.y == minGridBounds_.y || coordinates.y == maxGridBounds_.y)) {
                // recalculateBounds();
            }
        }
    } else {
        nonDefaultCells_[coordinates] = state;
        updateBounds(coordinates);
    }
}

/**
 * @brief Collects states of neighboring cells.
 * @param centerCoordinates Coordinates of the central cell.
 * @param neighborhoodDefinition Relative offsets of neighbors.
 * @return Vector of neighbor states.
 */
std::vector<int> CellSpace::getNeighborStates(Point centerCoordinates) const {
    std::vector<int> neighborStates;
    neighborStates.reserve(neighborhood_.size());

    for (const Point& offset : neighborhood_) {
        neighborStates.push_back(getCellState(centerCoordinates + offset));
    }
    return neighborStates;
}

/**
 * @brief Applies a map of pending state changes to the grid.
 * @param cellsToUpdate An unordered_map where keys are Point coordinates
 * of cells to change, and values are their new integer states.
 * Before update, the cells to be evaluate will be cleared.
 */
void CellSpace::updateCells(const std::unordered_map<Point, int>& cellsToUpdate) {
    clearCellsToEvaluate();

    if (cellsToUpdate.empty()) return;
    bool boundaryPotentiallyAffected = false;

    for (const auto& cellUpdatePair : cellsToUpdate) {
        const Point& coords = cellUpdatePair.first;
        int newState = cellUpdatePair.second;

        setCellState(coords, newState);
    }

    if (nonDefaultCells_.empty()) {
        boundsInitialized_ = false;
        minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
        maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
    } else if (boundaryPotentiallyAffected) {
        // If any operation (addition or removal) could have affected the boundary,
        // it's safest to recalculate. UpdateBounds only expands.
        // recalculateBounds();
    }
}


const std::unordered_map<Point, int>& CellSpace::getNonDefaultCells() const {
    return nonDefaultCells_;
}

const std::unordered_set<Point>& CellSpace::getCellsToEvaluate() const {
    return cellsToEvaluate_;
}

void CellSpace::loadCells(const std::unordered_map<Point, int>& cells, Point minB, Point maxB) {
    nonDefaultCells_ = cells;
    if (nonDefaultCells_.empty()) {
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
    for(auto pair : nonDefaultCells_){
        for(Point offset : reverseNeighborhood_){
            cellsToEvaluate_.insert(offset+pair.first);
        }
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
    nonDefaultCells_.clear();
    cellsToEvaluate_.clear();
    boundsInitialized_ = false;
    minGridBounds_ = Point(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    maxGridBounds_ = Point(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
}

void CellSpace::clearCellsToEvaluate() {
    cellsToEvaluate_.clear();
}

int CellSpace::getDefaultState() const {
    return defaultState_;
}
