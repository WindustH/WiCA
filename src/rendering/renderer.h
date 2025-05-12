#ifndef RENDERER_H
#define RENDERER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <map>
#include "../core/config.h" // Required for Config
#include "../ca/cell_space.h"
#include "viewport.h"
#include "../utils/color.h"

// Enum for grid display mode
enum class GridDisplayMode {
    AUTO,
    ON,
    OFF
};

/**
 * @class Renderer
 * @brief Handles all SDL2 rendering for the cellular automaton grid and UI elements.
 */
class Renderer {
private:
    SDL_Renderer* sdlRenderer_;
    SDL_Window* sdlWindow_;
    std::map<int, SDL_Color> stateSdlColorMap_;

    TTF_Font* uiFont_;
    SDL_Color uiTextColor_;
    SDL_Color uiMsgColor_;
    SDL_Color uiBrushInfoColor_;
    SDL_Color uiBackgroundColor_;
    SDL_Color gridLineColor_;
    int gridLineWidth_; // New: Width of the grid lines in pixels

    bool uiComponentsInitialized_;
    bool fontLoadedSuccessfully_;

    std::string currentFontName_;
    std::string currentFontPath_;
    int currentFontSize_;

    GridDisplayMode gridDisplayMode_;
    int gridHideThreshold_;

    bool initializeTTF();
    void cleanupTTF();
    bool loadFontInternal(const std::string& fontIdentifier, int fontSize, bool isFullPath = false);
    bool loadDefaultFont(int fontSize);
    void renderMultiLineText(const std::string& text, int x, int y, SDL_Color color, int maxWidth, int& outHeight);

public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool initialize(SDL_Window* window, const Config& config);
    /**
     * @brief Reinitializes the color map from a new configuration.
     * @param newConfig The new configuration object.
     */
    void reinitializeColors(const Config& newConfig);

    void renderGrid(const CellSpace& cellSpace, const Viewport& viewport);
    void renderUI(const std::string& commandText, bool showCommandInput,
                  const std::string& userMessage, const std::string& brushInfo,
                  const Viewport& viewport);

    void presentScreen();
    void cleanup();

    bool setFontSize(int newSize);
    bool setFontPath(const std::string& fontPath, int fontSize);

    void setGridDisplayMode(GridDisplayMode mode);
    void setGridHideThreshold(int threshold);
    GridDisplayMode getGridDisplayMode() const { return gridDisplayMode_; }
    int getGridHideThreshold() const { return gridHideThreshold_; }

    /**
     * @brief Sets the grid line width.
     * @param width The new width in pixels (must be >= 1).
     */
    void setGridLineWidth(int width);
    int getGridLineWidth() const { return gridLineWidth_; }

    /**
     * @brief Sets the grid line color.
     * @param r Red component (0-255).
     * @param g Green component (0-255).
     * @param b Blue component (0-255).
     * @param a Alpha component (0-255, default 255).
     */
    void setGridLineColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255);
    SDL_Color getGridLineColor() const { return gridLineColor_; }


    int getCurrentFontSize() const { return currentFontSize_; }
    bool isUiReady() const;
    static SDL_Color convertToSdlColor(const Color& color);
};

#endif // RENDERER_H
