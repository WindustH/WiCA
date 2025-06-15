#ifndef VIEWPORT_H
#define VIEWPORT_H

#include "../utils/point.h"
#include "../ca/cell_space.h" // Required for autoFit functionality
#include <SDL.h>              // For SDL_Rect

/**
 * @class Viewport
 * @brief Manages the camera's view (zoom, pan) over the cell grid.
 *
 * This class handles coordinate transformations between screen space (pixels)
 * and world space (grid coordinates). It supports zooming in/out and panning
 * the view, as well as an auto-fit feature to display all active cells.
 */
class Viewport {
public: // Made PointF public for use in Application or other classes if needed
    /**
     * @struct PointF
     * @brief Defines a 2D point/vector structure using floating-point coordinates.
     * Used for precise world coordinates before conversion to integer grid cells.
     */
    struct PointF {
        float x, y;
        PointF(float x_val = 0.0f, float y_val = 0.0f) : x(x_val), y(y_val) {}
    };

private:
    float zoomLevel_;       // Current zoom factor. 1.0 is default. >1 is zoomed in, <1 is zoomed out.
    PointF viewOffset_;     // Precise World coordinate (float) that corresponds to the top-left of the screen. (Pan)
    int screenWidth_;       // Width of the rendering window in pixels.
    int screenHeight_;      // Height of the rendering window in pixels.
    bool autoFitEnabled_;   // Flag indicating if auto-fit mode is active.
    float defaultCellSize_; // The size of a cell in pixels at zoomLevel_ = 1.0.

public:
    /**
     * @brief Constructor for Viewport.
     * @param screenWidth The width of the application window in pixels.
     * @param screenHeight The height of the application window in pixels.
     * @param defaultCellSize The desired size of a single cell in pixels when zoom is 1.0.
     */
    Viewport(int screenWidth, int screenHeight, float defaultCellSize = 10.0f);

    /**
     * @brief Zooms the view by a given factor, optionally centered around a mouse position.
     * @param factor The multiplication factor for zoom (e.g., 1.1 for 10% zoom in, 0.9 for 10% zoom out).
     * @param mouseScreenPos The position of the mouse on the screen, used as the zoom center.
     * If not provided or if Point(-1,-1), zooms around screen center.
     */
    void zoom(float factor, Point mouseScreenPos = Point(-1, -1));

    /**
     * @brief Zooms the view to achieve a target cell size on screen.
     * @param targetCellSize The desired size of a cell in pixels.
     * @param zoomCenterScreen The screen coordinates around which to zoom.
     */
    void zoomToCellSize(float targetCellSize, Point zoomCenterScreen);


    /**
     * @brief Pans the view by a given delta in screen pixels.
     * @param screenDelta The amount to move the view, in screen pixels (dx, dy).
     */
    void pan(Point screenDelta);

    /**
     * @brief Centers the view on a specific world coordinate.
     * @param worldCenter The world (grid) coordinate (can be float for precision) to center the view on.
     */
    void setCenter(PointF worldCenter);

    /**
     * @brief Toggles or sets the auto-fit mode and applies it if enabled.
     * When enabled, the viewport automatically adjusts zoom and pan
     * to fit all active cells (defined by cellSpace's bounds) within the screen.
     * @param enabled True to enable auto-fit, false to disable.
     * @param cellSpace A reference to the CellSpace, used to get the bounds of active cells.
     */
    void setAutoFit(bool enabled, const CellSpace& cellSpace);


    /**
     * @brief If auto-fit is enabled, this method recalculates and applies the zoom/pan
     * to fit the current state of cellSpace.
     * @param cellSpace A reference to the CellSpace.
     */
    void updateAutoFit(const CellSpace& cellSpace);


    /**
     * @brief Converts screen coordinates (pixels) to world coordinates (integer grid cells).
     * @param screenPos The Point representing screen coordinates.
     * @return Point The corresponding world coordinates, floored.
     */
    Point screenToWorld(Point screenPos) const;

    /**
     * @brief Converts screen coordinates (pixels) to precise world coordinates (float).
     * @param screenPos The Point representing screen coordinates.
     * @return PointF The corresponding precise world coordinates.
     */
    PointF screenToWorldF(Point screenPos) const;


    /**
     * @brief Converts world coordinates (grid cells) to screen coordinates (pixels).
     * @param worldPos The Point representing world coordinates.
     * @return Point The corresponding screen coordinates (top-left of the cell).
     */
    Point worldToScreen(Point worldPos) const;

    /**
     * @brief Gets the current zoom level.
     * @return float The zoom level.
     */
    float getZoomLevel() const;

    /**
     * @brief Gets the current precise view offset (world coordinate at screen top-left as float).
     * @return PointF The view offset.
     */
    PointF getViewOffsetF() const;

    /**
     * @brief Gets the current view offset, rounded to integer (world coordinate at screen top-left).
     * @return Point The view offset.
     */
    Point getViewOffset() const;


    /**
     * @brief Calculates the rectangle of the world (in grid coordinates) currently visible on screen.
     * @return SDL_Rect An SDL_Rect where x,y are the top-left world coordinates and w,h are the
     * width and height in world units (number of cells).
     */
    SDL_Rect getVisibleWorldRect() const;


    /**
     * @brief Gets the current size of a cell in screen pixels.
     * @return float The size of one cell in pixels based on current zoom.
     */
    float getCurrentCellSize() const;

    /**
     * @brief Gets the default size of a cell in screen pixels (at zoom 1.0).
     * @return float The default size of one cell in pixels.
     */
    float getDefaultCellSize() const; // Added this method

    /**
     * @brief Updates the screen dimensions, e.g., if the window is resized.
     * This also triggers an auto-fit update if auto-fit is enabled.
     * @param width The new screen width in pixels.
     * @param height The new screen height in pixels.
     * @param cellSpace A reference to the CellSpace, needed if auto-fit is active.
     */
    void setScreenDimensions(int width, int height, const CellSpace& cellSpace);

    /**
     * @brief Gets the screen width.
     * @return int Screen width in pixels.
     */
    int getScreenWidth() const;

    /**
     * @brief Gets the screen height.
     * @return int Screen height in pixels.
     */
    int getScreenHeight() const;

    /**
     * @brief Checks if autofit is currently enabled.
     * @return True if autofit is enabled, false otherwise.
     */
    bool isAutoFitEnabled() const;
};

#endif // VIEWPORT_H
