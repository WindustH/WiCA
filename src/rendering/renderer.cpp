#include "renderer.h"
#include "../utils/error_handler.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <filesystem>

const std::string ASSETS_FONT_PATH = "assets/fonts/";

Renderer::Renderer()
    : sdlRenderer_(nullptr),
      sdlWindow_(nullptr),
      uiFont_(nullptr),
      uiTextColor_({255, 255, 255, 255}),
      uiMsgColor_({255, 255, 0, 255}),
      uiBrushInfoColor_({200, 200, 255, 255}),
      uiBackgroundColor_({50, 50, 50, 200}),
      gridLineColor_({80, 80, 80, 255}),     // Default grid line color
      gridLineWidth_(1),                       // Default grid line width
      uiComponentsInitialized_(false),
      fontLoadedSuccessfully_(false),
      currentFontName_(""),
      currentFontPath_(""),
      currentFontSize_(16),
      gridDisplayMode_(GridDisplayMode::AUTO),
      gridHideThreshold_(10)
      {
        std::cout << "[DEBUG] Renderer Constructor called." << std::endl;
}

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::initializeTTF() {
    if (uiComponentsInitialized_) return true;

    if (TTF_Init() == -1) {
        ErrorHandler::sdlError("Renderer: TTF_Init failed");
        uiComponentsInitialized_ = false;
        return false;
    }
    std::cout << "[DEBUG] TTF_Init successful." << std::endl;
    uiComponentsInitialized_ = true;
    return true;
}

void Renderer::cleanupTTF() {
    if (uiComponentsInitialized_) {
        TTF_Quit();
        uiComponentsInitialized_ = false;
        std::cout << "[DEBUG] Renderer: TTF_Quit called." << std::endl;
    }
}

bool Renderer::loadFontInternal(const std::string& fontIdentifier, int fontSize, bool isFullPath) {
    if (!uiComponentsInitialized_) {
        ErrorHandler::logError("Renderer::loadFontInternal - TTF system not initialized. Cannot load font.", true);
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
        std::cout << "[DEBUG] Renderer::loadFontInternal - Attempting to load font from full path: " << pathToTry << " with size " << fontSize << std::endl;
    } else {
        pathToTry = ASSETS_FONT_PATH + fontIdentifier;
        std::cout << "[DEBUG] Renderer::loadFontInternal - Attempting to load font from assets: " << pathToTry << " with size " << fontSize << std::endl;
    }

    uiFont_ = TTF_OpenFont(pathToTry.c_str(), fontSize);
    if (!uiFont_) {
        if (!isFullPath) {
             std::cout << "[DEBUG] Renderer::loadFontInternal - Asset font failed. Trying as system font: " << fontIdentifier << std::endl;
            uiFont_ = TTF_OpenFont(fontIdentifier.c_str(), fontSize);
            if (!uiFont_) {
                ErrorHandler::logError("Renderer: TTF_OpenFont failed for '" + pathToTry + "' (and as system font '" + fontIdentifier + "'). SDL_ttf Error: " + std::string(TTF_GetError()));
                fontLoadedSuccessfully_ = false;
                currentFontName_ = "";
                currentFontPath_ = "";
                return false;
            } else {
                 std::cout << "[DEBUG] Successfully loaded font from system path: " << fontIdentifier << " size " << fontSize << std::endl;
                 currentFontPath_ = fontIdentifier;
                 currentFontName_ = fontIdentifier;
            }
        } else {
            ErrorHandler::logError("Renderer: TTF_OpenFont failed for full path '" + pathToTry + "'. SDL_ttf Error: " + std::string(TTF_GetError()));
            fontLoadedSuccessfully_ = false;
            currentFontName_ = "";
            currentFontPath_ = "";
            return false;
        }
    } else {
        std::cout << "[DEBUG] Successfully loaded font: " << pathToTry << " size " << fontSize << std::endl;
        currentFontPath_ = pathToTry;
        if (isFullPath) {
            std::filesystem::path fsPath(pathToTry);
            currentFontName_ = fsPath.filename().string();
        } else {
            currentFontName_ = fontIdentifier;
        }
    }

    currentFontSize_ = fontSize;
    fontLoadedSuccessfully_ = true;
    return true;
}

bool Renderer::loadDefaultFont(int fontSize) {
    std::cout << "[DEBUG] Renderer::loadDefaultFont with size " << fontSize << std::endl;
    const char* defaultFontNames[] = {
        "default.ttf",
        nullptr
    };

    for (int i = 0; defaultFontNames[i] != nullptr; ++i) {
        if (loadFontInternal(defaultFontNames[i], fontSize, false)) {
            return true;
        }
    }

    std::cout << "[DEBUG] Renderer::loadDefaultFont - All local asset fonts failed. Trying system fonts." << std::endl;
    const char* systemFontFallbacks[] = { "monospace", "Consolas", "Courier New", "sans-serif", "arial", nullptr };
    for (int i = 0; systemFontFallbacks[i] != nullptr; ++i) {
        if (loadFontInternal(systemFontFallbacks[i], fontSize, false)) {
            std::cout << "[INFO] Renderer: Loaded system fallback font '" << systemFontFallbacks[i] << "' as default." << std::endl;
            return true;
        }
    }

    ErrorHandler::logError("Renderer: CRITICAL - Failed to load ANY default font (local or system). UI text will not be available.");
    return false;
}

void Renderer::reinitializeColors(const Config& newConfig) {
    stateSdlColorMap_.clear();
    std::cout << "[DEBUG] Renderer::reinitializeColors - Color map cleared." << std::endl;
    if (!newConfig.isLoaded()) {
        ErrorHandler::logError("Renderer::reinitializeColors - Provided newConfig is not loaded. Using fallback colors.");
        stateSdlColorMap_[0] = convertToSdlColor(Color(255, 255, 255, 255));
        stateSdlColorMap_[1] = convertToSdlColor(Color(0, 0, 0, 255));
    } else {
        const auto& appColorMap = newConfig.getStateColorMap();
        std::cout << "[DEBUG] Renderer::reinitializeColors - Loading colors from new config:" << std::endl;
        for (const auto& pair : appColorMap) {
            stateSdlColorMap_[pair.first] = convertToSdlColor(pair.second);
            std::cout << "  State " << pair.first << ": R" << (int)pair.second.r << " G" << (int)pair.second.g << " B" << (int)pair.second.b << " A" << (int)pair.second.a << std::endl;
        }
        int configDefaultState = newConfig.getDefaultState();
        if (stateSdlColorMap_.find(configDefaultState) == stateSdlColorMap_.end()){
            Color c = newConfig.getColorForState(configDefaultState);
            stateSdlColorMap_[configDefaultState] = convertToSdlColor(c);
             std::cout << "  Default State " << configDefaultState << " (from new config, added): R" << (int)c.r << " G" << (int)c.g << " B" << (int)c.b << " A" << (int)c.a << std::endl;
        } else {
             std::cout << "  Default State " << configDefaultState << " (from new config) already in map." << std::endl;
        }
    }
    std::cout << "[DEBUG] Renderer stateSdlColorMap_ repopulated. Size: " << stateSdlColorMap_.size() << std::endl;
}


bool Renderer::initialize(SDL_Window* window, const Config& config) {
    std::cout << "[DEBUG] Renderer::initialize()" << std::endl;
    if (!window) {
        ErrorHandler::logError("Renderer: Provided window is null.");
        return false;
    }
    sdlWindow_ = window;

    sdlRenderer_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer_) {
        ErrorHandler::sdlError("Renderer: SDL_CreateRenderer (ACCELERATED) failed. Trying SOFTWARE fallback.");
        sdlRenderer_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
        if (!sdlRenderer_) {
            ErrorHandler::sdlError("Renderer: SDL_CreateRenderer (SOFTWARE) also failed.");
            return false;
        }
    }
    std::cout << "[DEBUG] Renderer: SDL_CreateRenderer successful." << std::endl;
    SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);

    if (!initializeTTF()) {
        ErrorHandler::logError("Renderer: Failed to initialize TTF system. UI text might not be available.");
    } else {
        if (!loadDefaultFont(currentFontSize_)) {
             ErrorHandler::logError("Renderer: Failed to load any default font during initialization.");
        }
    }
    reinitializeColors(config); // Use the new method to load colors

    ErrorHandler::logError("Renderer: Initialized successfully.", false);
    return true;
}

bool Renderer::setFontSize(int newSize) {
    if (newSize <= 0) {
        ErrorHandler::logError("Renderer::setFontSize - Invalid font size: " + std::to_string(newSize));
        return false;
    }
    if (currentFontPath_.empty()) {
        ErrorHandler::logError("Renderer::setFontSize - No font currently loaded (path is empty). Cannot change size. Trying to load default font with new size.");
        if (loadDefaultFont(newSize)) {
            return true;
        }
        return false;
    }

    std::cout << "[DEBUG] Renderer::setFontSize - Setting font size to " << newSize << " for font path: " << currentFontPath_ << std::endl;

    bool pathIsLikelyAbsoluteOrSystem = (currentFontPath_.find('/') != std::string::npos ||
                                         currentFontPath_.find('\\') != std::string::npos ||
                                         ASSETS_FONT_PATH.rfind(currentFontPath_, 0) != 0); // if not starting with asset path prefix

    if (loadFontInternal(currentFontPath_, newSize, pathIsLikelyAbsoluteOrSystem)) {
        return true;
    } else {
         std::cout << "[DEBUG] Renderer::setFontSize - Reloading with currentFontPath_ failed. Trying with currentFontName_." << std::endl;
        if (!currentFontName_.empty() && loadFontInternal(currentFontName_, newSize, false)) {
            return true;
        }
    }
    ErrorHandler::logError("Renderer::setFontSize - Failed to reload font '" + currentFontPath_ + "' or '" + currentFontName_ + "' at new size " + std::to_string(newSize));
    return false;
}

bool Renderer::setFontPath(const std::string& fontPath, int fontSize) {
    if (fontPath.empty()) {
        ErrorHandler::logError("Renderer::setFontPath - Font path is empty.");
        return false;
    }
    if (fontSize <= 0) {
        ErrorHandler::logError("Renderer::setFontPath - Invalid font size: " + std::to_string(fontSize));
        return false;
    }
    std::cout << "[DEBUG] Renderer::setFontPath - Setting font to path: " << fontPath << " with size " << fontSize << std::endl;
    if (loadFontInternal(fontPath, fontSize, true)) {
        return true;
    }
    ErrorHandler::logError("Renderer::setFontPath - Failed to load font from path: " + fontPath);
    return false;
}

void Renderer::setGridDisplayMode(GridDisplayMode mode) {
    gridDisplayMode_ = mode;
    std::string modeStr = "AUTO";
    if (mode == GridDisplayMode::ON) modeStr = "ON";
    else if (mode == GridDisplayMode::OFF) modeStr = "OFF";
    std::cout << "[DEBUG] Renderer: Grid display mode set to " << modeStr << std::endl;
}

void Renderer::setGridHideThreshold(int threshold) {
    if (threshold < 0) threshold = 0;
    gridHideThreshold_ = threshold;
    std::cout << "[DEBUG] Renderer: Grid hide threshold set to " << threshold << std::endl;
}

void Renderer::setGridLineWidth(int width) {
    if (width < 1) {
        gridLineWidth_ = 1;
        ErrorHandler::logError("Renderer::setGridLineWidth - Invalid width " + std::to_string(width) + ". Setting to 1px.", false);
    } else {
        gridLineWidth_ = width;
    }
    std::cout << "[DEBUG] Renderer: Grid line width set to " << gridLineWidth_ << "px." << std::endl;
}

void Renderer::setGridLineColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    gridLineColor_ = {r, g, b, a};
    std::cout << "[DEBUG] Renderer: Grid line color set to R" << (int)r << " G" << (int)g << " B" << (int)b << " A" << (int)a << std::endl;
}


void Renderer::renderGrid(const CellSpace& cellSpace, const Viewport& viewport) {
    if (!sdlRenderer_) return;

    SDL_Color backgroundColor = {220, 220, 220, 255};
    int defaultStateVal = cellSpace.getDefaultState();
    auto it_default_color = stateSdlColorMap_.find(defaultStateVal);
    if (it_default_color != stateSdlColorMap_.end()) {
        backgroundColor = it_default_color->second;
    }

    SDL_SetRenderDrawColor(sdlRenderer_, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    SDL_RenderClear(sdlRenderer_);

    float currentCellPixelSize = viewport.getCurrentCellSize();
    int screenW = viewport.getScreenWidth();
    int screenH = viewport.getScreenHeight();

    const auto& activeCellsMap = cellSpace.getActiveCells();

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

        if (!drawGridLines) { // Optimized cell drawing for no gaps when grid is off
            Point screenPosEndOfCellX = viewport.worldToScreen({worldPos.x + 1, worldPos.y});
            Point screenPosEndOfCellY = viewport.worldToScreen({worldPos.x, worldPos.y + 1});

            cellRect.x = screenPosStart.x;
            cellRect.y = screenPosStart.y;
            cellRect.w = screenPosEndOfCellX.x - screenPosStart.x;
            cellRect.h = screenPosEndOfCellY.y - screenPosStart.y;

            // Ensure minimum 1x1 pixel size if calculated dimensions are smaller (e.g. for very zoomed out views)
            if (cellRect.w < 1) cellRect.w = 1;
            if (cellRect.h < 1) cellRect.h = 1;

        } else { // Original logic when grid lines might be drawn (or if above proves problematic)
            int cellPixelDrawSize = static_cast<int>(std::max(1.0f, currentCellPixelSize));
            cellRect = { screenPosStart.x, screenPosStart.y, cellPixelDrawSize, cellPixelDrawSize };
        }


        if (cellRect.x < screenW && cellRect.y < screenH &&
            cellRect.x + cellRect.w > 0 && cellRect.y + cellRect.h > 0) {

            auto it_color = stateSdlColorMap_.find(state);
            SDL_Color drawColor;
            if (it_color != stateSdlColorMap_.end()) {
                drawColor = it_color->second;
            } else {
                drawColor = {255, 0, 255, 255};
            }

            SDL_SetRenderDrawColor(sdlRenderer_, drawColor.r, drawColor.g, drawColor.b, drawColor.a);
            SDL_RenderFillRect(sdlRenderer_, &cellRect);
        }
    }

    if (drawGridLines) {
        SDL_SetRenderDrawColor(sdlRenderer_, gridLineColor_.r, gridLineColor_.g, gridLineColor_.b, gridLineColor_.a);

        Viewport::PointF worldTopLeftF = viewport.screenToWorldF({0,0});
        Viewport::PointF worldBottomRightF = viewport.screenToWorldF({screenW, screenH});

        // Calculate offset for thicker lines
        int lineOffset = (gridLineWidth_ - 1) / 2;

        for (float wx = std::floor(worldTopLeftF.x); wx <= std::ceil(worldBottomRightF.x); ++wx) {
            Point screenPos = viewport.worldToScreen({static_cast<int>(std::round(wx)), 0});
            if (screenPos.x + lineOffset >= 0 && screenPos.x - lineOffset < screenW) {
                if (gridLineWidth_ == 1) {
                    SDL_RenderDrawLine(sdlRenderer_, screenPos.x, 0, screenPos.x, screenH);
                } else {
                    SDL_Rect lineRect = {screenPos.x - lineOffset, 0, gridLineWidth_, screenH};
                    SDL_RenderFillRect(sdlRenderer_, &lineRect);
                }
            }
        }
        for (float wy = std::floor(worldTopLeftF.y); wy <= std::ceil(worldBottomRightF.y); ++wy) {
            Point screenPos = viewport.worldToScreen({0, static_cast<int>(std::round(wy))});
            if (screenPos.y + lineOffset >= 0 && screenPos.y - lineOffset < screenH) {
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
    outHeight = 0;
    if (!isUiReady() || text.empty()) {
        return;
    }

    std::stringstream ss(text);
    std::string line;
    int currentY = y;
    int fontLineSkip = TTF_FontLineSkip(uiFont_);
    if (fontLineSkip <= 0) fontLineSkip = (uiFont_ && TTF_FontHeight(uiFont_) > 0) ? TTF_FontHeight(uiFont_) + 2 : 18;


    while (std::getline(ss, line, '\n')) {
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
                 ErrorHandler::logError("Renderer: SDL_CreateTextureFromSurface failed for multi-line: " + std::string(SDL_GetError()));
                 currentY += fontLineSkip;
                 outHeight += fontLineSkip;
            }
            SDL_FreeSurface(surface);
        } else {
            ErrorHandler::logError("Renderer: TTF_RenderText_Blended_Wrapped failed for multi-line: '" + line + "' Error: " + TTF_GetError());
            currentY += fontLineSkip;
            outHeight += fontLineSkip;
        }
        if (ss.peek() != EOF && surface && surface->h < fontLineSkip * 0.85 ) {
             int gap = static_cast<int>(fontLineSkip * 0.15);
             currentY += gap;
             outHeight += gap;
        }
    }
}

void Renderer::renderUI(const std::string& commandText, bool showCommandInput,
                        const std::string& userMessage, const std::string& brushInfo,
                        const Viewport& viewport) {
    if (!sdlRenderer_) return;

    if (!isUiReady()) {
        static bool fontErrorLogged = false;
        if(!fontErrorLogged && !fontLoadedSuccessfully_) {
           ErrorHandler::logError("Renderer::renderUI - UI Font not available (fontLoadedSuccessfully_ is false), cannot render text elements.");
           fontErrorLogged = true;
        } else if (fontLoadedSuccessfully_) {
            ErrorHandler::logError("Renderer::renderUI - UI Font pointer is null despite successful load flag. Cannot render text.");
            fontErrorLogged = true;
        }
        return;
    }

    int screenW = viewport.getScreenWidth();
    int screenH = viewport.getScreenHeight();
    int textPadding = 5;
    int UIMargin = 10;
    int currentY = UIMargin;
    int fontLineSkip = TTF_FontLineSkip(uiFont_);
    if (fontLineSkip <= 0) fontLineSkip = (uiFont_ && TTF_FontHeight(uiFont_) > 0) ? TTF_FontHeight(uiFont_) + 2 : 18;


    if (!brushInfo.empty()) {
        SDL_Surface* surface = TTF_RenderText_Blended(uiFont_, brushInfo.c_str(), uiBrushInfoColor_);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(sdlRenderer_, surface);
            if (texture) {
                SDL_Rect bgRect = { UIMargin - textPadding, currentY - textPadding, surface->w + 2 * textPadding, surface->h + 2 * textPadding};
                SDL_SetRenderDrawColor(sdlRenderer_, uiBackgroundColor_.r, uiBackgroundColor_.g, uiBackgroundColor_.b, uiBackgroundColor_.a);
                SDL_RenderFillRect(sdlRenderer_, &bgRect);

                SDL_Rect textRect = {UIMargin, currentY, surface->w, surface->h};
                SDL_RenderCopy(sdlRenderer_, texture, nullptr, &textRect);
                SDL_DestroyTexture(texture);
                currentY += bgRect.h + UIMargin / 2;
            } else { ErrorHandler::logError("Renderer: SDL_CreateTextureFromSurface for brush_info failed: " + std::string(SDL_GetError())); }
            SDL_FreeSurface(surface);
        } else { ErrorHandler::logError("Renderer: TTF_RenderText_Blended for brush_info failed: " + std::string(TTF_GetError())); }
    }

    int messageRenderedHeight = 0;
    if (!userMessage.empty()) {
        int messageMaxWidth = screenW - (2 * UIMargin) - (2 * textPadding);
        if (messageMaxWidth < 50) messageMaxWidth = 50;

        int estimatedLines = 0;
        std::string temp = userMessage; size_t pos = 0;
        while((pos = temp.find('\n', pos)) != std::string::npos) { estimatedLines++; pos++;}
        estimatedLines++;

        int estimatedHeight = (estimatedLines * fontLineSkip);
        if (userMessage.length() > (size_t)messageMaxWidth / 7 && estimatedLines == 1) estimatedLines = 2;
        estimatedHeight = (estimatedLines * fontLineSkip) + ((estimatedLines >1 ? estimatedLines-1 : 0) * fontLineSkip/4) ;


        SDL_Rect msgBgRect = {UIMargin - textPadding, currentY - textPadding, messageMaxWidth + (2*textPadding), std::max(fontLineSkip + 2*textPadding, estimatedHeight + 2*textPadding) };
        SDL_SetRenderDrawColor(sdlRenderer_, uiBackgroundColor_.r, uiBackgroundColor_.g, uiBackgroundColor_.b, uiBackgroundColor_.a);
        SDL_RenderFillRect(sdlRenderer_, &msgBgRect);

        renderMultiLineText(userMessage, UIMargin, currentY, uiMsgColor_, messageMaxWidth, messageRenderedHeight);
    }


    if (showCommandInput) {
        std::string fullCommandText = "/" + commandText + "_"; // Add leading slash for display and cursor

        SDL_Surface* textSurface = TTF_RenderText_Blended_Wrapped(uiFont_, fullCommandText.c_str(), uiTextColor_, static_cast<Uint32>(screenW - (2*UIMargin) - (2*textPadding)));
        if (!textSurface) {
            ErrorHandler::logError("Renderer: TTF_RenderText_Blended_Wrapped failed for command input: " + std::string(TTF_GetError()));
        } else {
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(sdlRenderer_, textSurface);
            if (!textTexture) {
                ErrorHandler::logError("Renderer: SDL_CreateTextureFromSurface failed for command input: " + std::string(SDL_GetError()));
            } else {
                int cmdBoxContentHeight = textSurface->h;
                int cmdBoxHeight = cmdBoxContentHeight + (2 * textPadding);
                if (cmdBoxHeight < fontLineSkip + (2*textPadding)) {
                    cmdBoxHeight = fontLineSkip + (2*textPadding);
                }

                int cmd_y_pos = screenH - cmdBoxHeight - UIMargin;

                SDL_Rect uiBackgroundRect = { UIMargin, cmd_y_pos, screenW - (2*UIMargin), cmdBoxHeight };
                SDL_SetRenderDrawColor(sdlRenderer_, uiBackgroundColor_.r, uiBackgroundColor_.g, uiBackgroundColor_.b, uiBackgroundColor_.a);
                SDL_RenderFillRect(sdlRenderer_, &uiBackgroundRect);

                SDL_Rect uiTextRect = { UIMargin + textPadding, cmd_y_pos + textPadding, textSurface->w, textSurface->h };
                SDL_RenderCopy(sdlRenderer_, textTexture, nullptr, &uiTextRect);
                SDL_DestroyTexture(textTexture);
            }
            SDL_FreeSurface(textSurface);
        }
    }
}

void Renderer::presentScreen() {
    if (sdlRenderer_) {
        SDL_RenderPresent(sdlRenderer_);
    } else {
        ErrorHandler::logError("Renderer::presentScreen - sdlRenderer_ is null, cannot present.", true);
    }
}

void Renderer::cleanup() {
    std::cout << "[DEBUG] Renderer::cleanup() called." << std::endl;
    if (uiFont_) {
        TTF_CloseFont(uiFont_);
        uiFont_ = nullptr;
        fontLoadedSuccessfully_ = false;
        std::cout << "[DEBUG] Renderer: UI Font closed." << std::endl;
    }
    cleanupTTF();
    if (sdlRenderer_) {
        SDL_DestroyRenderer(sdlRenderer_);
        sdlRenderer_ = nullptr;
        std::cout << "[DEBUG] Renderer: SDL_DestroyRenderer called." << std::endl;
    }
}

bool Renderer::isUiReady() const {
    return uiComponentsInitialized_ && fontLoadedSuccessfully_ && uiFont_ != nullptr;
}

SDL_Color Renderer::convertToSdlColor(const Color& color) {
    return {color.r, color.g, color.b, color.a};
}
