#include "renderer.h"
#include "../utils/logger.h" // New logger
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <unordered_map> // Ensure this is included, though renderer.h should bring it in.

const std::string ASSETS_FONT_PATH = "assets/fonts/";

Renderer::Renderer()
    : sdlRenderer_(nullptr),
      sdlWindow_(nullptr),
      uiFont_(nullptr),
      uiTextColor_({255, 255, 255, 255}),
      uiMsgColor_({255, 255, 0, 255}),
      uiBrushInfoColor_({200, 200, 255, 255}),
      uiBackgroundColor_({50, 50, 50, 200}),
      gridLineColor_({80, 80, 80, 255}),
      gridLineWidth_(1),
      uiComponentsInitialized_(false),
      fontLoadedSuccessfully_(false),
      currentFontName_(""),
      currentFontPath_(""),
      currentFontSize_(16),
      gridDisplayMode_(GridDisplayMode::AUTO),
      gridHideThreshold_(10)
      {
        // std::cout << "[DEBUG] Renderer Constructor called." << std::endl;
}

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::initializeTTF() {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (logger) logger->info("Start to initialize TTF.");
    if (uiComponentsInitialized_) return true;

    if (TTF_Init() == -1) {
        if (logger) logger->error("TTF initialization failed.");
        uiComponentsInitialized_ = false;
        return false;
    }
    // std::cout << "[DEBUG] TTF_Init successful." << std::endl;
    uiComponentsInitialized_ = true;

    if (logger) logger->info("TTF initialized.");
    return true;
}

void Renderer::cleanupTTF() {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (logger) logger->info("Start to clean up TTF.");
    if (uiComponentsInitialized_) {
        TTF_Quit();
        uiComponentsInitialized_ = false;
    }
    if (logger) logger->info("TTF cleaned up.");
}

bool Renderer::loadFontInternal(const std::string& fontIdentifier, int fontSize, bool isFullPath) {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (logger) logger->info("Start to load font.");
    if (!uiComponentsInitialized_) {
        if (logger) logger->error("TTF system not initialized. Cannot load font.");
        return false;
    }

    if (uiFont_) {
        TTF_CloseFont(uiFont_);
        uiFont_ = nullptr;
        fontLoadedSuccessfully_ = false;
    }

    std::string pathToTry;
    if (isFullPath) {
        pathToTry = fontIdentifier;
        // std::cout << "[DEBUG] Renderer::loadFontInternal - Attempting to load font from full path: " << pathToTry << " with size " << fontSize << std::endl;
    } else {
        pathToTry = ASSETS_FONT_PATH + fontIdentifier;
        // std::cout << "[DEBUG] Renderer::loadFontInternal - Attempting to load font from assets: " << pathToTry << " with size " << fontSize << std::endl;
    }

    uiFont_ = TTF_OpenFont(pathToTry.c_str(), fontSize);
    if (!uiFont_) {
        if (!isFullPath) {
            //  std::cout << "[DEBUG] Renderer::loadFontInternal - Asset font failed. Trying as system font: " << fontIdentifier << std::endl;
            uiFont_ = TTF_OpenFont(fontIdentifier.c_str(), fontSize);
            if (!uiFont_) {
                if (logger) logger->error("Failed for '" + pathToTry + "' (and as system font '" + fontIdentifier + "'). SDL_ttf Error: " + std::string(TTF_GetError()));
                fontLoadedSuccessfully_ = false;
                currentFontName_ = "";
                currentFontPath_ = "";
                return false;
            } else {
                //  std::cout << "[DEBUG] Successfully loaded font from system path: " << fontIdentifier << " size " << fontSize << std::endl;
                 currentFontPath_ = fontIdentifier; // Store the identifier used (system font name)
                 currentFontName_ = fontIdentifier; // Can be the same for system fonts
            }
        } else {
            if (logger) logger->error("Failed for full path '" + pathToTry + "'. SDL_ttf Error: " + std::string(TTF_GetError()));
            fontLoadedSuccessfully_ = false;
            currentFontName_ = "";
            currentFontPath_ = "";
            return false;
        }
    } else {
        // std::cout << "[DEBUG] Successfully loaded font: " << pathToTry << " size " << fontSize << std::endl;
        currentFontPath_ = pathToTry; // Store the full path that succeeded
        if (isFullPath || pathToTry.find(ASSETS_FONT_PATH) == 0) { // If it was a full path or an asset path
            std::filesystem::path fsPath(pathToTry);
            currentFontName_ = fsPath.filename().string();
        } else { // Should not happen if logic above is correct, but as fallback
            currentFontName_ = fontIdentifier;
        }
    }

    currentFontSize_ = fontSize;
    fontLoadedSuccessfully_ = true;

    if (logger) logger->info("Font loaded.");
    return true;
}

bool Renderer::loadDefaultFont(int fontSize) {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (logger) logger->info("Start to load default font.");
    const char* defaultFontNames[] = {
        "default.ttf", // Example: "arial.ttf" or a known font in assets/fonts/
        nullptr
    };

    for (int i = 0; defaultFontNames[i] != nullptr; ++i) {
        if (loadFontInternal(defaultFontNames[i], fontSize, false)) {
            return true;
        }
    }

    // std::cout << "[DEBUG] Renderer::loadDefaultFont - All local asset fonts failed. Trying system fonts." << std::endl;
    const char* systemFontFallbacks[] = { "monospace", "Consolas", "Courier New", "sans-serif", "Arial", nullptr }; // Added Arial
    for (int i = 0; systemFontFallbacks[i] != nullptr; ++i) {
        // For system fonts, the 'fontIdentifier' is the name itself, and 'isFullPath' is effectively false (or rather, it's not a relative asset path)
        // The internal logic of loadFontInternal will try it as a direct name if asset path fails.
        if (loadFontInternal(systemFontFallbacks[i], fontSize, false)) { // Treat as identifier, not full path
            if (logger) logger->warn("Loaded system fallback font '" + std::string(systemFontFallbacks[i]) + "' as default.");
            return true;
        }
    }

    if (logger) logger->error("Failed to load ANY default font (local or system). UI text will not be available.");
    return false;
}

void Renderer::reinitializeColors(const Rule& newConfig) {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    stateSdlColorMap_.clear();
    if (!newConfig.isLoaded()) {
        if (logger) logger->error("Provided newConfig is not loaded. Using fallback colors.");
        stateSdlColorMap_[0] = convertToSdlColor(Color(255, 255, 255, 255)); // White for state 0
        stateSdlColorMap_[1] = convertToSdlColor(Color(0, 0, 0, 255));     // Black for state 1
    } else {
        const auto& appColorMap = newConfig.getStateColorMap(); // This is
        for (const auto& pair : appColorMap) {
            stateSdlColorMap_[pair.first] = convertToSdlColor(pair.second);
        }
        // Ensure default state color is present
        int configDefaultState = newConfig.getDefaultState();
        if (stateSdlColorMap_.find(configDefaultState) == stateSdlColorMap_.end()){
            Color c = newConfig.getColorForState(configDefaultState); // getColorForState should provide a color
            stateSdlColorMap_[configDefaultState] = convertToSdlColor(c);
        } else {
        }
    }
}


bool Renderer::initialize(SDL_Window* window, const Rule& config) {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (logger) logger->info("Start to initialize renderer.");
    if (!window) {
        if (logger) logger->error("Provided window is null.");
        return false;
    }
    sdlWindow_ = window;

    sdlRenderer_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer_) {
        if (logger) logger->error("SDL_CreateRenderer (ACCELERATED) failed. Trying SOFTWARE fallback.");
        sdlRenderer_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
        if (!sdlRenderer_) {
            if (logger) logger->error("SDL_CreateRenderer (SOFTWARE) also failed.");
            return false;
        }
    }
    SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);

    if (!initializeTTF()) {
        if (logger) logger->error("Failed to initialize TTF system. UI text might not be available.");
        // Continue without font if TTF fails, but log it.
    } else {
        if (!loadDefaultFont(currentFontSize_)) { // currentFontSize_ is 16 by default
             if (logger) logger->error("Failed to load any default font during initialization.");
        }
    }
    reinitializeColors(config);

    if (logger) logger->info("Renderer initialized successfully.");
    return true;
}

bool Renderer::setFontSize(int newSize) {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (newSize <= 0) {
        if (logger) logger->error("Invalid font size: " + std::to_string(newSize));
        return false;
    }
    if (currentFontPath_.empty() && currentFontName_.empty()) {
        if (logger) logger->error("No font currently loaded (path and name are empty). Cannot change size. Trying to load default font with new size.");
        return loadDefaultFont(newSize); // Attempt to load a default with the new size
    }

    bool pathIsLikelyAbsoluteOrAsset = (!currentFontPath_.empty() &&
                                       (currentFontPath_.find('/') != std::string::npos ||
                                        currentFontPath_.find('\\') != std::string::npos ||
                                        currentFontPath_.rfind(ASSETS_FONT_PATH, 0) == 0) );

    std::string identifierToLoad = pathIsLikelyAbsoluteOrAsset ? currentFontPath_ : currentFontName_;
    bool useFullPathFlag = pathIsLikelyAbsoluteOrAsset; // If currentFontPath_ was used and it's absolute/asset

    if (identifierToLoad.empty()) { // Fallback if currentFontPath_ was empty but currentFontName_ was not
        identifierToLoad = currentFontName_;
        useFullPathFlag = false; // Treat currentFontName_ as an identifier, not a full path initially
    }


    if (loadFontInternal(identifierToLoad, newSize, useFullPathFlag)) {
        return true;
    } else {
        // If loading with currentFontPath_ (as full path) failed, and currentFontName_ is different and might be a system font
        if (useFullPathFlag && !currentFontName_.empty() && currentFontName_ != identifierToLoad) {
            if (loadFontInternal(currentFontName_, newSize, false)) { // Treat currentFontName_ as an identifier
                return true;
            }
        }
    }
    if (logger) logger->error("Failed to reload font '" + identifierToLoad + "' (and potentially '" + currentFontName_ + "') at new size " + std::to_string(newSize));
    return false;
}

bool Renderer::setFontPath(const std::string& fontPath, int fontSize) {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (fontPath.empty()) {
        if (logger) logger->error("Font path is empty.");
        return false;
    }
    if (fontSize <= 0) {
        if (logger) logger->error("Invalid font size: " + std::to_string(fontSize));
        return false;
    }
    if (loadFontInternal(fontPath, fontSize, true)) { // Try as full path first
        return true;
    }
    if (loadFontInternal(fontPath, fontSize, false)) {
        return true;
    }

    if (logger) logger->error("Failed to load font from path/name: " + fontPath);
    return false;
}

void Renderer::setGridDisplayMode(GridDisplayMode mode) {
    gridDisplayMode_ = mode;
    std::string modeStr = "AUTO";
    if (mode == GridDisplayMode::ON) modeStr = "ON";
    else if (mode == GridDisplayMode::OFF) modeStr = "OFF";
}

void Renderer::setGridHideThreshold(int threshold) {
    if (threshold < 0) threshold = 0;
    gridHideThreshold_ = threshold;
}

void Renderer::setGridLineWidth(int width) {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (width < 1) {
        gridLineWidth_ = 1;
        if (logger) logger->error("Invalid width " + std::to_string(width) + ". Setting to 1px.");
    } else {
        gridLineWidth_ = width;
    }
}

void Renderer::setGridLineColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    gridLineColor_ = {r, g, b, a};
}


void Renderer::renderGrid(const CellSpace& cellSpace, const Viewport& viewport) {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (!sdlRenderer_) return;

    SDL_Color backgroundColor = {220, 220, 220, 255}; // Default background
    int defaultStateVal = cellSpace.getDefaultState();
    auto it_default_color = stateSdlColorMap_.find(defaultStateVal);
    if (it_default_color != stateSdlColorMap_.end()) {
        backgroundColor = it_default_color->second;
    } else {
        // Fallback if default state's color isn't in map for some reason
        if (logger) logger->warn("Default state " + std::to_string(defaultStateVal) + " color not found in map. Using fallback background.");
    }

    SDL_SetRenderDrawColor(sdlRenderer_, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    SDL_RenderClear(sdlRenderer_);

    float currentCellPixelSize = viewport.getCurrentCellSize();
    int screenW = viewport.getScreenWidth();
    int screenH = viewport.getScreenHeight();

    const auto& activeCellsMap = cellSpace.getNonDefaultCells(); // This is now std::unordered_map

    bool drawGridLines = false;
    switch (gridDisplayMode_) {
        case GridDisplayMode::ON:
            drawGridLines = true;
            break;
        case GridDisplayMode::OFF:
            drawGridLines = false;
            break;
        case GridDisplayMode::AUTO:
            if (currentCellPixelSize >= static_cast<float>(gridHideThreshold_)) {
                drawGridLines = true;
            }
            break;
    }

    for (const auto& pair : activeCellsMap) {
        Point worldPos = pair.first;
        int state = pair.second;

        SDL_Rect cellRect;
        Point screenPosStart = viewport.worldToScreen(worldPos);

        // Calculate cell width and height based on the next cell's screen position
        // This ensures cells are contiguous when no grid lines are drawn.
        Point screenPosEndOfCellX = viewport.worldToScreen({worldPos.x + 1, worldPos.y});
        Point screenPosEndOfCellY = viewport.worldToScreen({worldPos.x, worldPos.y + 1});

        cellRect.x = screenPosStart.x;
        cellRect.y = screenPosStart.y;
        cellRect.w = screenPosEndOfCellX.x - screenPosStart.x;
        cellRect.h = screenPosEndOfCellY.y - screenPosStart.y;

        // Ensure minimum 1x1 pixel size if calculated dimensions are smaller (e.g. for very zoomed out views)
        // This is particularly important when currentCellPixelSize is < 1
        if (cellRect.w < 1 && currentCellPixelSize > 0) cellRect.w = 1;
        if (cellRect.h < 1 && currentCellPixelSize > 0) cellRect.h = 1;
        if (currentCellPixelSize <= 0) { // If cells are infinitely small or negative, don't draw
            cellRect.w = 0;
            cellRect.h = 0;
        }


        // Culling: only draw if the cell is at least partially visible
        if (cellRect.x < screenW && cellRect.y < screenH &&
            cellRect.x + cellRect.w > 0 && cellRect.y + cellRect.h > 0) {

            auto it_color = stateSdlColorMap_.find(state);
            SDL_Color drawColor;
            if (it_color != stateSdlColorMap_.end()) {
                drawColor = it_color->second;
            } else {
                // Fallback for states not in color map (e.g. magenta)
                drawColor = {255, 0, 255, 255};
                // Log this missing state color once to avoid spamming logs
                static std::unordered_map<int, bool> loggedMissingColors;
                if (loggedMissingColors.find(state) == loggedMissingColors.end()) {
                    if (logger) logger->warn("Color for state " + std::to_string(state) + " not found. Using fallback magenta.");
                    loggedMissingColors[state] = true;
                }
            }

            SDL_SetRenderDrawColor(sdlRenderer_, drawColor.r, drawColor.g, drawColor.b, drawColor.a);
            SDL_RenderFillRect(sdlRenderer_, &cellRect);
        }
    }

    if (drawGridLines && currentCellPixelSize > 0) { // Also check currentCellPixelSize to avoid issues if it's zero/negative
        SDL_SetRenderDrawColor(sdlRenderer_, gridLineColor_.r, gridLineColor_.g, gridLineColor_.b, gridLineColor_.a);

        Viewport::PointF worldTopLeftF = viewport.screenToWorldF({0,0});
        Viewport::PointF worldBottomRightF = viewport.screenToWorldF({screenW, screenH});

        int lineOffset = (gridLineWidth_ - 1) / 2;

        // Vertical lines
        for (float wx = std::floor(worldTopLeftF.x); wx <= std::ceil(worldBottomRightF.x +1); ++wx) { // +1 to ensure last line is drawn
            Point screenPos = viewport.worldToScreen({static_cast<int>(std::round(wx)), 0}); // Use worldToScreen for consistency
             if (screenPos.x + lineOffset >= -gridLineWidth_ && screenPos.x - lineOffset < screenW + gridLineWidth_) { // Generous culling
                if (gridLineWidth_ == 1) {
                    SDL_RenderDrawLine(sdlRenderer_, screenPos.x, 0, screenPos.x, screenH);
                } else {
                    SDL_Rect lineRect = {screenPos.x - lineOffset, 0, gridLineWidth_, screenH};
                    SDL_RenderFillRect(sdlRenderer_, &lineRect);
                }
            }
        }
        // Horizontal lines
        for (float wy = std::floor(worldTopLeftF.y); wy <= std::ceil(worldBottomRightF.y +1); ++wy) { // +1 to ensure last line is drawn
            Point screenPos = viewport.worldToScreen({0, static_cast<int>(std::round(wy))}); // Use worldToScreen
            if (screenPos.y + lineOffset >= -gridLineWidth_ && screenPos.y - lineOffset < screenH + gridLineWidth_) { // Generous culling
                 if (gridLineWidth_ == 1) {
                    SDL_RenderDrawLine(sdlRenderer_, 0, screenPos.y, screenW, screenPos.y);
                } else {
                    SDL_Rect lineRect = {0, screenPos.y - lineOffset, screenW, gridLineWidth_};
                    SDL_RenderFillRect(sdlRenderer_, &lineRect);
                }
            }
        }
    }
}

void Renderer::renderMultiLineText(const std::string& text, int x, int y, SDL_Color color, int maxWidth, int& outHeight) {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    outHeight = 0;
    if (!isUiReady() || text.empty()) {
        return;
    }

    std::stringstream ss(text);
    std::string line;
    int currentY = y;
    int fontLineSkip = TTF_FontLineSkip(uiFont_);
    if (fontLineSkip <= 0) fontLineSkip = (uiFont_ && TTF_FontHeight(uiFont_) > 0) ? TTF_FontHeight(uiFont_) + 2 : currentFontSize_ + 2;


    while (std::getline(ss, line, '\n')) {
        // TTF_RenderText_Blended_Wrapped expects a non-empty string. If line is empty, render a space to maintain line height.
        SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(uiFont_, line.empty() ? " " : line.c_str(), color, static_cast<Uint32>(maxWidth));
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(sdlRenderer_, surface);
            if (texture) {
                SDL_Rect dstRect = {x, currentY, surface->w, surface->h};
                SDL_RenderCopy(sdlRenderer_, texture, nullptr, &dstRect);
                SDL_DestroyTexture(texture);
                currentY += surface->h;
                outHeight += surface->h;
            } else {
                 if (logger) logger->error("SDL_CreateTextureFromSurface failed for multi-line: " + std::string(SDL_GetError()));
                 currentY += fontLineSkip; // Fallback line skip
                 outHeight += fontLineSkip;
            }
            SDL_FreeSurface(surface);
        } else {
            if (logger) logger->error("TTF_RenderText_Blended_Wrapped failed for multi-line: '" + line + "' Error: " + TTF_GetError());
            currentY += fontLineSkip; // Fallback line skip
            outHeight += fontLineSkip;
        }
        // Add a small gap between lines if the rendered surface height is less than TTF_FontLineSkip,
        // but only if there are more lines to render.
        if (ss.peek() != EOF && surface && surface->h < fontLineSkip ) {
             int gap = fontLineSkip - surface->h;
             if (gap < 0) gap = 0; // Should not happen if surface->h < fontLineSkip
             if (gap > fontLineSkip /2) gap = fontLineSkip /2; // Cap the gap
             currentY += gap;
             outHeight += gap;
        }
    }
}

void Renderer::renderUI(const std::string& commandText, bool showCommandInput,
                        const std::string& userMessage, const std::string& brushInfo,
                        const Viewport& viewport) {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (!sdlRenderer_) return;

    if (!isUiReady()) {
        static bool uiErrorLogged = false; // Log only once per session run
        if(!uiErrorLogged) {
           if(!fontLoadedSuccessfully_) {
               if (logger) logger->error("UI Font not available (fontLoadedSuccessfully_ is false), cannot render text elements.");
           } else if (!uiFont_) { // uiFont_ is nullptr despite fontLoadedSuccessfully_ being true
               if (logger) logger->error("UI Font pointer is null despite successful load flag. Cannot render text.");
           }
           uiErrorLogged = true;
        }
        return;
    }

    int screenW = viewport.getScreenWidth();
    int screenH = viewport.getScreenHeight();
    int textPadding = 5;
    int UIMargin = 10;
    int currentY = UIMargin; // Start Y position for UI elements from top
    int fontLineSkip = TTF_FontLineSkip(uiFont_);
    if (fontLineSkip <= 0) fontLineSkip = (uiFont_ && TTF_FontHeight(uiFont_) > 0) ? TTF_FontHeight(uiFont_) + 2 : currentFontSize_ + 2;


    // Render Brush Info (Top-left)
    if (!brushInfo.empty()) {
        int brushInfoRenderedHeight = 0;
        int brushInfoMaxWidth = screenW / 2 - UIMargin; // Example max width
        renderMultiLineText(brushInfo, UIMargin + textPadding, currentY + textPadding, uiBrushInfoColor_, brushInfoMaxWidth, brushInfoRenderedHeight);

        if (brushInfoRenderedHeight > 0) {
            SDL_Rect bgRect = { UIMargin, currentY,
                                std::min(brushInfoMaxWidth, screenW - 2*UIMargin) + 2 * textPadding, // Calculate width based on rendered text or max width
                                brushInfoRenderedHeight + 2 * textPadding};
            // To get actual width of rendered text for bgRect, TTF_SizeText could be used before renderMultiLineText
            // For simplicity, using a pre-calculated max width or a fixed width.
            // A more accurate width would require TTF_SizeUTF8 for each line in renderMultiLineText and taking the max.
            // For now, let's make the background a bit wider if text is short.
            int tempW = 0; TTF_SizeUTF8(uiFont_, brushInfo.substr(0, brushInfo.find('\n')).c_str(), &tempW, nullptr); // Approx width of first line
            bgRect.w = std::max(tempW + 2 * textPadding, bgRect.w);
            bgRect.w = std::min(bgRect.w, screenW - 2*UIMargin);


            SDL_SetRenderDrawColor(sdlRenderer_, uiBackgroundColor_.r, uiBackgroundColor_.g, uiBackgroundColor_.b, uiBackgroundColor_.a);
            SDL_RenderFillRect(sdlRenderer_, &bgRect);
            // Re-render text on top of background
            renderMultiLineText(brushInfo, UIMargin + textPadding, currentY + textPadding, uiBrushInfoColor_, brushInfoMaxWidth, brushInfoRenderedHeight);
            currentY += bgRect.h + UIMargin / 2;
        }
    }

    // Render User Message (Below Brush Info or Top-left)
    int messageRenderedHeight = 0;
    if (!userMessage.empty()) {
        int messageMaxWidth = screenW - (2 * UIMargin) - (2 * textPadding);
        if (messageMaxWidth < 50) messageMaxWidth = 50; // Minimum width

        // Estimate background height before rendering text for it
        // This is tricky for wrapped text. A simpler approach is to render text, get height, then render bg and text again.
        // Or, use a generous fixed height or a calculation based on string length / average chars per line.
        // For now, render text to get height, then draw BG, then re-render text.
        renderMultiLineText(userMessage, UIMargin + textPadding, currentY + textPadding, uiMsgColor_, messageMaxWidth, messageRenderedHeight);

        if (messageRenderedHeight > 0) {
            SDL_Rect msgBgRect = {UIMargin, currentY, messageMaxWidth + (2*textPadding), messageRenderedHeight + (2*textPadding)};
            SDL_SetRenderDrawColor(sdlRenderer_, uiBackgroundColor_.r, uiBackgroundColor_.g, uiBackgroundColor_.b, uiBackgroundColor_.a);
            SDL_RenderFillRect(sdlRenderer_, &msgBgRect);
            // Re-render text on top of background
            renderMultiLineText(userMessage, UIMargin + textPadding, currentY + textPadding, uiMsgColor_, messageMaxWidth, messageRenderedHeight);
            // currentY += msgBgRect.h + UIMargin / 2; // This would move command input down
        }
    }


    // Render Command Input (Bottom of the screen)
    if (showCommandInput) {
        std::string fullCommandText = "/" + commandText + "_"; // Add leading slash for display and cursor

        int cmdTextWidth = 0;
        int cmdTextHeight = 0;
        // Use TTF_SizeUTF8 to get dimensions for wrapped text if it were to wrap.
        // However, command input is usually single line that might exceed width.
        // For simplicity, assume it won't wrap or will be truncated by the background rect.
        // TTF_RenderText_Blended_Wrapped will handle wrapping if text is too long.

        SDL_Surface* textSurface = TTF_RenderText_Blended_Wrapped(uiFont_, fullCommandText.c_str(), uiTextColor_, static_cast<Uint32>(screenW - (2*UIMargin) - (2*textPadding)));
        if (!textSurface) {
            if (logger) logger->error("TTF_RenderText_Blended_Wrapped failed for command input: " + std::string(TTF_GetError()));
        } else {
            cmdTextWidth = textSurface->w;
            cmdTextHeight = textSurface->h;

            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(sdlRenderer_, textSurface);
            if (!textTexture) {
                if (logger) logger->error("SDL_CreateTextureFromSurface failed for command input: " + std::string(SDL_GetError()));
            } else {
                int cmdBoxContentHeight = cmdTextHeight;
                int cmdBoxHeight = cmdBoxContentHeight + (2 * textPadding);
                // Ensure minimum height based on font line skip
                if (cmdBoxHeight < fontLineSkip + (2*textPadding)) {
                    cmdBoxHeight = fontLineSkip + (2*textPadding);
                }

                int cmd_y_pos = screenH - cmdBoxHeight - UIMargin;

                SDL_Rect uiBackgroundRect = { UIMargin, cmd_y_pos, screenW - (2*UIMargin), cmdBoxHeight };
                SDL_SetRenderDrawColor(sdlRenderer_, uiBackgroundColor_.r, uiBackgroundColor_.g, uiBackgroundColor_.b, uiBackgroundColor_.a);
                SDL_RenderFillRect(sdlRenderer_, &uiBackgroundRect);

                // Center text vertically if cmdBoxHeight is larger than textSurface->h due to fontLineSkip
                int text_y_offset = (cmdBoxHeight - cmdTextHeight - 2*textPadding)/2;
                if (text_y_offset < 0) text_y_offset = 0;

                SDL_Rect uiTextRect = { UIMargin + textPadding, cmd_y_pos + textPadding + text_y_offset, cmdTextWidth, cmdTextHeight };
                SDL_RenderCopy(sdlRenderer_, textTexture, nullptr, &uiTextRect);
                SDL_DestroyTexture(textTexture);
            }
            SDL_FreeSurface(textSurface);
        }
    }
}

void Renderer::presentScreen() {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (sdlRenderer_) {
        SDL_RenderPresent(sdlRenderer_);
    } else {
        if (logger) logger->error("sdlRenderer_ is null, cannot present.");
    }
}

void Renderer::cleanup() {
    auto logger = Logging::GetLogger(Logging::Module::Renderer);
    if (logger) logger->info("Renderer clean-up called.");
    if (uiFont_) {
        TTF_CloseFont(uiFont_);
        uiFont_ = nullptr;
        fontLoadedSuccessfully_ = false;
    }
    cleanupTTF();
    if (sdlRenderer_) {
        SDL_DestroyRenderer(sdlRenderer_);
        sdlRenderer_ = nullptr;
    }
}

bool Renderer::isUiReady() const {
    return uiComponentsInitialized_ && fontLoadedSuccessfully_ && uiFont_ != nullptr;
}

SDL_Color Renderer::convertToSdlColor(const Color& color) {
    return {color.r, color.g, color.b, color.a};
}
