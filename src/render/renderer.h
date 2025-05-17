#ifndef RENDERER_H
#define RENDERER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_map> // For stateSdlColorMap_
#include <map>             // For batchedRects/Points in renderCells implementation
#include <unordered_set>   // For globallyLoggedMissingColors

#include "../core/rule.h"       // Required for Rule
#include "../ca/cell_space.h"   // Required for CellSpace
#include "viewport.h"           // Required for Viewport (includes Point struct)
#include "../utils/color.h"     // Required for Color

// Enum for grid display mode
enum class GridDisplayMode {
    AUTO,
    ON,
    OFF
};

// Helper struct to store pre-calculated render information for each cell rectangle
struct CellRenderInfo {
    SDL_Rect rect;
    SDL_Color color;
};

// Helper struct to store pre-calculated render information for each cell pixel
struct PixelRenderInfo {
    Point screenPos; // Point struct from viewport.h (likely {int x, int y})
    SDL_Color color;
};

// Custom comparator for SDL_Color to use it as a key in std::map
struct SdlColorCompare {
    bool operator()(const SDL_Color& a, const SDL_Color& b) const {
        if (a.r != b.r) return a.r < b.r;
        if (a.g != b.g) return a.g < b.g;
        if (a.b != b.b) return a.b < b.b;
        return a.a < b.a; // Compare alpha as well
    }
};

/**
 * @class Renderer
 * @brief Handles all SDL2 rendering for the cellular automaton grid and UI elements.
 */
class Renderer {
private:
    SDL_Renderer* sdlRenderer_;
    SDL_Window* sdlWindow_;
    std::unordered_map<int, SDL_Color> stateSdlColorMap_;

    TTF_Font* uiFont_;
    SDL_Color uiTextColor_;
    SDL_Color uiMsgColor_;
    SDL_Color uiBrushInfoColor_;
    SDL_Color uiBackgroundColor_;
    SDL_Color gridLineColor_;
    int gridLineWidth_;

    bool uiComponentsInitialized_;
    bool fontLoadedSuccessfully_;

    std::string currentFontName_;
    std::string currentFontPath_;
    int currentFontSize_;

    GridDisplayMode gridDisplayMode_;
    int gridHideThreshold_;

    // Private helper methods
    bool initializeTTF();
    void cleanupTTF();
    bool loadFontInternal(const std::string& fontIdentifier, int fontSize, bool isFullPath = false);
    bool loadDefaultFont(int fontSize);

    // Methods for rendering different parts
    void renderCells(const CellSpace& cellSpace, const Viewport& viewport);
    void renderGridLines(const Viewport& viewport);
    void renderMultiLineText(const std::string& text, int x, int y, SDL_Color color, int maxWidth, int& outHeight);

    // Static member to keep track of logged missing colors to avoid spamming logs
    static std::unordered_set<int> globallyLoggedMissingColors;

public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool initialize(SDL_Window* window, const Rule& config);
    void reinitializeColors(const Rule& newConfig);

    void renderGrid(const CellSpace& cellSpace, const Viewport& viewport);
    void renderUI(const std::string& commandText, bool showCommandInput,
                  const std::string& userMessage, const std::string& brushInfo,
                  const Viewport& viewport);

    void presentScreen();
    void cleanup();

    bool setFontSize(int newSize);
    bool setFontPath(const std::string& fontPath, int fontSize);
    int getCurrentFontSize() const { return currentFontSize_; }

    void setGridDisplayMode(GridDisplayMode mode);
    void setGridHideThreshold(int threshold);
    GridDisplayMode getGridDisplayMode() const { return gridDisplayMode_; }
    int getGridHideThreshold() const { return gridHideThreshold_; }

    void setGridLineWidth(int width);
    int getGridLineWidth() const { return gridLineWidth_; }

    void setGridLineColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255);
    SDL_Color getGridLineColor() const { return gridLineColor_; }

    bool isUiReady() const;
    static SDL_Color convertToSdlColor(const Color& color);
};

#endif // RENDERER_H
