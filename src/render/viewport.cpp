#include "viewport.h"
#include <algorithm> // For std::min, std::max
#include <cmath>     // For std::floor, std::ceil, std::round, std::abs
#include "../utils/logger.h" // New logger

/**
 * @brief Constructor for Viewport.
 * @param screenWidth Window width in pixels.
 * @param screenHeight Window height in pixels.
 * @param defaultCellSize The size of a cell at zoom 1.0.
 */
Viewport::Viewport(int sw, int sh, float dcs)
    : zoomLevel_(1.0f),
      viewOffset_({0.0f, 0.0f}), // Initialize PointF members
      screenWidth_(sw),
      screenHeight_(sh),
      autoFitEnabled_(false),
      defaultCellSize_(dcs > 0 ? dcs : 10.0f) { // Ensure defaultCellSize is positive
}

/**
 * @brief Zooms the view.
 * @param factor Zoom multiplication factor.
 * @param mouseScreenPos Screen coordinates to zoom around. If (-1,-1), use screen center.
 */
void Viewport::zoom(float factor, Point mouseScreenPos) {
    if (factor <= 0.0f) return; // Invalid zoom factor

    // If auto-fit is on, zooming manually should disable it.
    if (autoFitEnabled_) {
        autoFitEnabled_ = false;
    }

    Point zoomCenterScreen = mouseScreenPos;
    if (mouseScreenPos.x == -1 && mouseScreenPos.y == -1) { // Default to screen center
        zoomCenterScreen = Point(screenWidth_ / 2, screenHeight_ / 2);
    }

    // Convert zoom center from screen to world coordinates before zoom
    PointF worldP_before = screenToWorldF(zoomCenterScreen); // Use float version for precision

    zoomLevel_ *= factor;


    float currentCellPixelSize = getCurrentCellSize(); // Store to avoid re-calculation
    if (std::abs(currentCellPixelSize) < 1e-6f) { // Avoid division by zero
        // Handle degenerate case, perhaps by resetting view or logging error
        return;
    }

    viewOffset_.x = worldP_before.x - static_cast<float>(zoomCenterScreen.x) / currentCellPixelSize;
    viewOffset_.y = worldP_before.y - static_cast<float>(zoomCenterScreen.y) / currentCellPixelSize;
}

/**
 * @brief Zooms the view to achieve a target cell size on screen.
 * @param targetCellSize The desired size of a cell in pixels.
 * @param zoomCenterScreen The screen coordinates around which to zoom.
 */
void Viewport::zoomToCellSize(float targetCellSize, Point zoomCenterScreen) {
    if (targetCellSize <= 0.0f || defaultCellSize_ <= 0.0f) {
        return; // Cannot zoom to non-positive or if default is non-positive
    }
    float currentActualCellSize = getCurrentCellSize();
    if (std::abs(currentActualCellSize) < 1e-6f && std::abs(targetCellSize) < 1e-6f) {
        return; // Already at or near zero, avoid division by zero if target is also zero
    }
    if (std::abs(currentActualCellSize) < 1e-6f && targetCellSize > 0) { // If current is zero, but target is not
        // This case implies zoomLevel_ is effectively zero.
        // We need to set zoomLevel_ directly to achieve targetCellSize.
        zoomLevel_ = targetCellSize / defaultCellSize_;
        // After setting zoomLevel_, we still need to adjust viewOffset_ correctly.
        // The logic in zoom() handles this adjustment based on a factor.
        // Here, we can't easily use a "factor" from zero.
        // Instead, let's set the zoomLevel and then re-center.
        PointF worldP_at_center = screenToWorldF(zoomCenterScreen);
        viewOffset_.x = worldP_at_center.x - static_cast<float>(zoomCenterScreen.x) / (defaultCellSize_ * zoomLevel_);
        viewOffset_.y = worldP_at_center.y - static_cast<float>(zoomCenterScreen.y) / (defaultCellSize_ * zoomLevel_);
        if (autoFitEnabled_) autoFitEnabled_ = false;
        return;
    }


    float zoomFactor = targetCellSize / currentActualCellSize;
    zoom(zoomFactor, zoomCenterScreen);
}


/**
 * @brief Pans the view.
 * @param screenDelta Amount to move in screen pixels.
 */
void Viewport::pan(Point screenDelta) {
    // If auto-fit is on, panning manually should disable it.
    if (autoFitEnabled_) {
        autoFitEnabled_ = false;
    }
    float currentCellPxSize = getCurrentCellSize();
    if (std::abs(currentCellPxSize) < 1e-6f) return; // Avoid division by zero if cell size is effectively zero

    viewOffset_.x -= static_cast<float>(screenDelta.x) / currentCellPxSize;
    viewOffset_.y -= static_cast<float>(screenDelta.y) / currentCellPxSize;
}

/**
 * @brief Centers the view on a world coordinate.
 * @param worldCenter World coordinate (can be float for precision) to center on.
 */
void Viewport::setCenter(PointF worldCenter) {
     if (autoFitEnabled_) { // Disabling autofit if manually centering
        autoFitEnabled_ = false;
    }
    float currentCellPxSize = getCurrentCellSize();
    if (std::abs(currentCellPxSize) < 1e-6f) { // Avoid division by zero
        viewOffset_.x = worldCenter.x; // Or some other default behavior
        viewOffset_.y = worldCenter.y;
        return;
    }
    viewOffset_.x = worldCenter.x - (static_cast<float>(screenWidth_) / 2.0f) / currentCellPxSize;
    viewOffset_.y = worldCenter.y - (static_cast<float>(screenHeight_) / 2.0f) / currentCellPxSize;
}

/**
 * @brief Toggles or sets auto-fit mode.
 * @param enabled True to enable, false to disable.
 * @param cellSpace Reference to CellSpace for bounds.
 */
void Viewport::setAutoFit(bool enabled, const CellSpace& cellSpace) {
    autoFitEnabled_ = enabled;
    if (autoFitEnabled_) {
        updateAutoFit(cellSpace);
    }
}

/**
 * @brief Updates zoom/pan if auto-fit is enabled.
 * @param cellSpace Reference to CellSpace.
 */
void Viewport::updateAutoFit(const CellSpace& cellSpace) {
    if (!autoFitEnabled_) {
        return;
    }
    if (!cellSpace.areBoundsInitialized() || cellSpace.getNonDefaultCells().empty()) {
        // If no cells or bounds not init, reset to a default view (e.g., zoom 1.0, centered at origin)
        zoomLevel_ = 1.0f / (defaultCellSize_ / this->getDefaultCellSize()); // Normalize zoom to show defaultCellSize_ as actual default
        zoomLevel_ = 1.0f; // More simply, set zoom to 1.0
        setCenter({0.0f, 0.0f});
        // Ensure autoFitEnabled remains true if it was intentionally set for an empty grid
        // autoFitEnabled_ = true; // This line might be redundant if called from setAutoFit
        return;
    }

    Point minB = cellSpace.getMinBounds();
    Point maxB = cellSpace.getMaxBounds();

    float worldWidth = static_cast<float>(maxB.x - minB.x + 1);
    float worldHeight = static_cast<float>(maxB.y - minB.y + 1);

    if (worldWidth <= 0.0f) worldWidth = 1.0f; // Avoid division by zero or negative values
    if (worldHeight <= 0.0f) worldHeight = 1.0f;

    const float paddingFactor = 0.1f; // 10% padding on each side
    float targetScreenWidth = static_cast<float>(screenWidth_) * (1.0f - 2.0f * paddingFactor);
    float targetScreenHeight = static_cast<float>(screenHeight_) * (1.0f - 2.0f * paddingFactor);

    if (targetScreenWidth <= 0.0f) targetScreenWidth = 1.0f; // Ensure positive dimensions
    if (targetScreenHeight <= 0.0f) targetScreenHeight = 1.0f;

    // Calculate zoom level needed to fit width and height
    float zoomX = targetScreenWidth / (worldWidth * defaultCellSize_);
    float zoomY = targetScreenHeight / (worldHeight * defaultCellSize_);

    zoomLevel_ = std::min(zoomX, zoomY);
    if (zoomLevel_ < 1e-3f) zoomLevel_ = 1e-3f; // Prevent zoom from becoming too small or zero

    // Calculate the center of the bounded area
    PointF worldCenter(
        static_cast<float>(minB.x) + worldWidth / 2.0f,
        static_cast<float>(minB.y) + worldHeight / 2.0f
    );

    // Recalculate viewOffset using the new zoomLevel and center
    float currentCellPxSize = getCurrentCellSize(); // This is defaultCellSize_ * zoomLevel_
     if (std::abs(currentCellPxSize) < 1e-6f) { // Avoid division by zero
        // This case should ideally not be reached if zoomLevel_ has a minimum.
        viewOffset_.x = worldCenter.x;
        viewOffset_.y = worldCenter.y;
        return;
    }
    viewOffset_.x = worldCenter.x - (static_cast<float>(screenWidth_) / 2.0f) / currentCellPxSize;
    viewOffset_.y = worldCenter.y - (static_cast<float>(screenHeight_) / 2.0f) / currentCellPxSize;
}


/**
 * @brief Converts screen coordinates to world coordinates (integer grid cells).
 * @param screenPos Screen coordinates.
 * @return World coordinates (Point).
 */
Point Viewport::screenToWorld(Point screenPos) const {
    PointF worldF = screenToWorldF(screenPos);
    // Flooring gives the world grid cell coordinate
    return Point(static_cast<int>(std::floor(worldF.x)), static_cast<int>(std::floor(worldF.y)));
}

/**
 * @brief Converts screen coordinates to precise world coordinates (float).
 * @param screenPos Screen coordinates.
 * @return Precise world coordinates (PointF).
 */
Viewport::PointF Viewport::screenToWorldF(Point screenPos) const {
    float currentCellPxSize = getCurrentCellSize();
    if (std::abs(currentCellPxSize) < 1e-6f) return {0.0f, 0.0f}; // Avoid division by zero

    float worldX = viewOffset_.x + static_cast<float>(screenPos.x) / currentCellPxSize;
    float worldY = viewOffset_.y + static_cast<float>(screenPos.y) / currentCellPxSize;
    return {worldX, worldY};
}


/**
 * @brief Converts world coordinates (integer grid cells) to screen coordinates.
 * @param worldPos World coordinates.
 * @return Screen coordinates (top-left of the cell).
 */
Point Viewport::worldToScreen(Point worldPos) const {
    float currentCellPxSize = getCurrentCellSize();
    // Calculate screen position by finding the difference from the viewOffset and scaling by cell size
    int screenX = static_cast<int>(std::floor((static_cast<float>(worldPos.x) - viewOffset_.x) * currentCellPxSize));
    int screenY = static_cast<int>(std::floor((static_cast<float>(worldPos.y) - viewOffset_.y) * currentCellPxSize));
    return Point(screenX, screenY);
}

float Viewport::getZoomLevel() const {
    return zoomLevel_;
}

Viewport::PointF Viewport::getViewOffsetF() const {
    return viewOffset_;
}

Point Viewport::getViewOffset() const {
    // Round to nearest integer for a representative integer offset if needed
    return Point(static_cast<int>(std::round(viewOffset_.x)), static_cast<int>(std::round(viewOffset_.y)));
}


/**
 * @brief Calculates the visible world rectangle.
 * @return SDL_Rect representing visible world area (in grid coordinates).
 */
SDL_Rect Viewport::getVisibleWorldRect() const {
    // Use precise float conversion first
    PointF topLeftWorldF = screenToWorldF(Point(0, 0));
    PointF bottomRightWorldF = screenToWorldF(Point(screenWidth_ -1 , screenHeight_ -1)); // Use screenWidth-1, screenHeight-1 for bottom-right pixel

    SDL_Rect visibleRect;
    // Floor for top-left, Ceil for bottom-right to ensure coverage
    visibleRect.x = static_cast<int>(std::floor(topLeftWorldF.x));
    visibleRect.y = static_cast<int>(std::floor(topLeftWorldF.y));
    // Width and height are number of cells. +1 because if x1=0, x2=0, width is 1 cell.
    visibleRect.w = static_cast<int>(std::ceil(bottomRightWorldF.x)) - visibleRect.x; // No +1 if ceil already covers it
    visibleRect.h = static_cast<int>(std::ceil(bottomRightWorldF.y)) - visibleRect.y; // No +1

    // Alternative for w/h to ensure at least 1 cell if visible
    if (visibleRect.w <= 0) visibleRect.w = 1;
    if (visibleRect.h <= 0) visibleRect.h = 1;


    return visibleRect;
}

float Viewport::getCurrentCellSize() const {
    return defaultCellSize_ * zoomLevel_;
}

float Viewport::getDefaultCellSize() const { // Added this method
    return defaultCellSize_;
}

void Viewport::setScreenDimensions(int width, int height, const CellSpace& cellSpace) {
    screenWidth_ = width;
    screenHeight_ = height;
    if (autoFitEnabled_) {
        updateAutoFit(cellSpace);
    }
}

int Viewport::getScreenWidth() const {
    return screenWidth_;
}

int Viewport::getScreenHeight() const {
    return screenHeight_;
}

bool Viewport::isAutoFitEnabled() const {
    return autoFitEnabled_;
}
