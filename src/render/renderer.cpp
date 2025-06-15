#include "renderer.h"
#include "../utils/logger.h"
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <vector>
#include <map>
#include <unordered_set> // For globallyLoggedMissingColors
// #include <random> // No longer needed for sampling
// #include <thread> // No longer needed for sampling

#include "../utils/timer.h"

// TBB Includes
#include <tbb/parallel_for_each.h>
#include <tbb/concurrent_vector.h>

const std::string ASSETS_FONT_PATH = "assets/fonts/";

// Definition for the static member
std::unordered_set<int> Renderer::globallyLoggedMissingColors;

// Renderer Constructor and other methods (initialize, font loading, UI, etc.) remain the same as your previous version.
// For brevity, I will only show the modified renderCells and dependent parts if any.
// Make sure to integrate this into your full renderer.cpp file.

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
}

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::initializeTTF() {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (logger) logger->info("Start to initialize TTF.");
    if (uiComponentsInitialized_) return true;

    if (TTF_Init() == -1) {
        if (logger) logger->error("TTF initialization failed.");
        uiComponentsInitialized_ = false;
        return false;
    }
    uiComponentsInitialized_ = true;
    if (logger) logger->info("TTF initialized.");
    return true;
}

void Renderer::cleanupTTF() {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (logger) logger->info("Start to clean up TTF.");
    if (uiComponentsInitialized_) {
        TTF_Quit();
        uiComponentsInitialized_ = false;
    }
    if (logger) logger->info("TTF cleaned up.");
}

bool Renderer::loadFontInternal(const std::string& fontIdentifier, int fontSize, bool isFullPath) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
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
    } else {
        pathToTry = ASSETS_FONT_PATH + fontIdentifier;
    }

    uiFont_ = TTF_OpenFont(pathToTry.c_str(), fontSize);
    if (!uiFont_) {
        if (!isFullPath) {
            uiFont_ = TTF_OpenFont(fontIdentifier.c_str(), fontSize);
            if (!uiFont_) {
                if (logger) logger->error("Failed for '" + pathToTry + "' (and as system font '" + fontIdentifier + "'). SDL_ttf Error: " + std::string(TTF_GetError()));
                fontLoadedSuccessfully_ = false;
                currentFontName_ = "";
                currentFontPath_ = "";
                return false;
            } else {
                 currentFontPath_ = fontIdentifier;
                 currentFontName_ = fontIdentifier;
            }
        } else {
            if (logger) logger->error("Failed for full path '" + pathToTry + "'. SDL_ttf Error: " + std::string(TTF_GetError()));
            fontLoadedSuccessfully_ = false;
            currentFontName_ = "";
            currentFontPath_ = "";
            return false;
        }
    } else {
        currentFontPath_ = pathToTry;
        if (isFullPath || pathToTry.rfind(ASSETS_FONT_PATH, 0) == 0) {
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
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (logger) logger->info("Start to load default font with size: " + std::to_string(fontSize));
    const char* defaultFontNames[] = { "default.ttf", nullptr };

    for (int i = 0; defaultFontNames[i] != nullptr; ++i) {
        if (loadFontInternal(defaultFontNames[i], fontSize, false)) return true;
    }

    if (logger) logger->debug("Local asset default fonts failed. Trying system fallbacks.");
    const char* systemFontFallbacks[] = { "monospace", "Consolas", "Courier New", "sans-serif", "Arial", nullptr };
    for (int i = 0; systemFontFallbacks[i] != nullptr; ++i) {
        if (loadFontInternal(systemFontFallbacks[i], fontSize, false)) {
            if (logger) logger->warn("Loaded system fallback font '" + std::string(systemFontFallbacks[i]) + "' as default.");
            return true;
        }
    }

    if (logger) logger->error("Failed to load ANY default font (local or system). UI text will not be available.");
    return false;
}

void Renderer::reinitializeColors(const Rule& newConfig) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    stateSdlColorMap_.clear();
    if (!newConfig.isLoaded()) {
        if (logger) logger->error("Provided newConfig is not loaded. Using fallback colors.");
        stateSdlColorMap_[0] = convertToSdlColor(Color(255, 255, 255, 255));
        stateSdlColorMap_[1] = convertToSdlColor(Color(0, 0, 0, 255));
    } else {
        const auto& appColorMap = newConfig.getStateColorMap();
        for (const auto& pair : appColorMap) {
            stateSdlColorMap_[pair.first] = convertToSdlColor(pair.second);
        }
        int configDefaultState = newConfig.getDefaultState();
        if (stateSdlColorMap_.find(configDefaultState) == stateSdlColorMap_.end()){
            if (logger) logger->debug("Default state " + std::to_string(configDefaultState) + " color not in map, adding it.");
            Color c = newConfig.getColorForState(configDefaultState);
            stateSdlColorMap_[configDefaultState] = convertToSdlColor(c);
        }
    }
}


bool Renderer::initialize(SDL_Window* window, const Rule& config) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (logger) logger->info("Start to initialize renderer.");
    if (!window) {
        if (logger) logger->error("Provided window is null.");
        return false;
    }
    sdlWindow_ = window;

    sdlRenderer_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer_) {
        if (logger) logger->warn("SDL_CreateRenderer (ACCELERATED) failed. Trying SOFTWARE fallback. Error: " + std::string(SDL_GetError()));
        sdlRenderer_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
        if (!sdlRenderer_) {
            if (logger) logger->error("SDL_CreateRenderer (SOFTWARE) also failed. Error: " + std::string(SDL_GetError()));
            return false;
        }
    }
    SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);

    if (!initializeTTF()) {
        if (logger) logger->warn("Failed to initialize TTF system. UI text might not be available.");
    } else {
        if (!loadDefaultFont(currentFontSize_)) {
             if (logger) logger->warn("Failed to load any default font during initialization.");
        }
    }
    reinitializeColors(config);

    if (logger) logger->info("Renderer initialized successfully.");
    return true;
}

bool Renderer::setFontSize(int newSize) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (newSize <= 0) {
        if (logger) logger->error("Invalid font size: " + std::to_string(newSize));
        return false;
    }
    if (currentFontPath_.empty() && currentFontName_.empty()) {
        if (logger) logger->warn("No font currently loaded (path and name are empty). Cannot change size. Trying to load default font with new size.");
        return loadDefaultFont(newSize);
    }

    bool pathIsLikelyAbsoluteOrAsset = (!currentFontPath_.empty() &&
                                       (currentFontPath_.find('/') != std::string::npos ||
                                        currentFontPath_.find('\\') != std::string::npos ||
                                        currentFontPath_.rfind(ASSETS_FONT_PATH, 0) == 0) );

    std::string identifierToLoad = pathIsLikelyAbsoluteOrAsset ? currentFontPath_ : currentFontName_;
    bool useFullPathFlag = pathIsLikelyAbsoluteOrAsset;

    if (identifierToLoad.empty()) {
        identifierToLoad = currentFontName_;
        useFullPathFlag = false;
    }

    if (loadFontInternal(identifierToLoad, newSize, useFullPathFlag)) {
        return true;
    } else {
        if (useFullPathFlag && !currentFontName_.empty() && currentFontName_ != identifierToLoad) {
            if (loadFontInternal(currentFontName_, newSize, false)) {
                return true;
            }
        }
    }
    if (logger) logger->error("Failed to reload font '" + identifierToLoad + "' (and potentially '" + currentFontName_ + "') at new size " + std::to_string(newSize));
    return false;
}

bool Renderer::setFontPath(const std::string& fontPath, int fontSize) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (fontPath.empty()) {
        if (logger) logger->error("Font path is empty.");
        return false;
    }
    if (fontSize <= 0) {
        if (logger) logger->error("Invalid font size: " + std::to_string(fontSize));
        return false;
    }
    if (loadFontInternal(fontPath, fontSize, true)) {
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
}

void Renderer::setGridHideThreshold(int threshold) {
    if (threshold < 0) threshold = 0;
    gridHideThreshold_ = threshold;
}

void Renderer::setGridLineWidth(int width) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (width < 1) {
        gridLineWidth_ = 1;
        if (logger) logger->warn("Invalid grid line width " + std::to_string(width) + ". Setting to 1px.");
    } else {
        gridLineWidth_ = width;
    }
}

void Renderer::setGridLineColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    gridLineColor_ = {r, g, b, a};
}

// --- renderCells with Deterministic Sampling for Sub-pixel Cells ---
void Renderer::renderCells(const CellSpace& cellSpace, const Viewport& viewport) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (!sdlRenderer_) return;

    const auto& activeCellsMap = cellSpace.getNonDefaultCells();
    if (activeCellsMap.empty()) {
        return;
    }

    // Pre-calculate cell screen dimensions and rendering mode
    Point screenOrigin = viewport.worldToScreen({0, 0});
    Point screenOneX = viewport.worldToScreen({1, 0});
    Point screenOneY = viewport.worldToScreen({0, 1});

    const float actual_cell_w_float = static_cast<float>(screenOneX.x - screenOrigin.x);
    const float actual_cell_h_float = static_cast<float>(screenOneY.y - screenOrigin.y);

    bool renderAsPixels = false;
    int sample_step_x = 1; // Default step if not rendering as pixels or if cell size >= 1px
    int sample_step_y = 1;
    int cell_render_w = 0;
    int cell_render_h = 0;

    if (actual_cell_w_float <= 0.0f || actual_cell_h_float <= 0.0f) {
        return; // Cells have no positive area on screen
    }

    if (actual_cell_w_float < 1.0f && actual_cell_h_float < 1.0f) {
        renderAsPixels = true;
        // Calculate sampling step based on how many world cells fit into one pixel approximately
        sample_step_x = static_cast<int>(std::max(1.0f, 1.0f / actual_cell_w_float));
        sample_step_y = static_cast<int>(std::max(1.0f, 1.0f / actual_cell_h_float));
    } else {
        renderAsPixels = false;
        cell_render_w = static_cast<int>(actual_cell_w_float);
        cell_render_h = static_cast<int>(actual_cell_h_float);
        if (cell_render_w == 0 && actual_cell_w_float > 0.0f) cell_render_w = 1;
        if (cell_render_h == 0 && actual_cell_h_float > 0.0f) cell_render_h = 1;

        if (cell_render_w <= 0 || cell_render_h <= 0) return; // Still no renderable area
    }

    tbb::concurrent_vector<CellRenderInfo> cellInfos;
    tbb::concurrent_vector<PixelRenderInfo> pixelInfos;

    // Estimate size for pixelInfos/cellInfos. This is a rough estimate.
    if (renderAsPixels) {
        pixelInfos.reserve(activeCellsMap.size() / (static_cast<size_t>(sample_step_x * sample_step_y)) + 10);
    } else {
        cellInfos.reserve(activeCellsMap.size());
    }

    tbb::concurrent_vector<int> statesMissingColorLog;

    int sW = viewport.getScreenWidth();
            int sH = viewport.getScreenHeight();
    // Parallel calculation
    tbb::parallel_for_each(activeCellsMap.begin(), activeCellsMap.end(),
        [&, this, renderAsPixels, sample_step_x, sample_step_y, cell_render_w, cell_render_h](const auto& pair) {
            Point worldPos = pair.first;
            int state = pair.second;
            Point screenPosStart = viewport.worldToScreen(worldPos);

            if (renderAsPixels) {
                // Deterministic sampling based on world coordinates and step
                // Handles negative coordinates correctly for modulo to ensure consistent grid
                bool x_match = ((worldPos.x % sample_step_x + sample_step_x) % sample_step_x) == 0;
                bool y_match = ((worldPos.y % sample_step_y + sample_step_y) % sample_step_y) == 0;

                if (x_match && y_match) {
                    if (screenPosStart.x >= 0 && screenPosStart.x < sW &&
                        screenPosStart.y >= 0 && screenPosStart.y < sH) {
                        SDL_Color drawColor;
                        auto it_color = this->stateSdlColorMap_.find(state);
                        if (it_color != this->stateSdlColorMap_.end()) {
                            drawColor = it_color->second;
                        } else {
                            drawColor = {255, 0, 255, 255};
                            statesMissingColorLog.push_back(state);
                        }
                        pixelInfos.emplace_back(PixelRenderInfo{screenPosStart, drawColor});
                    }
                }
            } else { // Render as rectangles
                SDL_Rect cellRect;
                cellRect.x = screenPosStart.x;
                cellRect.y = screenPosStart.y;
                cellRect.w = cell_render_w;
                cellRect.h = cell_render_h;

                if (cellRect.x < sW && cellRect.y < sH &&
                    cellRect.x + cellRect.w > 0 && cellRect.y + cellRect.h > 0) {
                    SDL_Color drawColor;
                    auto it_color = this->stateSdlColorMap_.find(state);
                    if (it_color != this->stateSdlColorMap_.end()) {
                        drawColor = it_color->second;
                    } else {
                        drawColor = {255, 0, 255, 255};
                        statesMissingColorLog.push_back(state);
                    }
                    cellInfos.emplace_back(CellRenderInfo{cellRect, drawColor});
                }
            }
        }
    );

    // Log missing colors
    if (logger) {
        for (int state_val : statesMissingColorLog) {
            if (Renderer::globallyLoggedMissingColors.find(state_val) == Renderer::globallyLoggedMissingColors.end()) {
                logger->warn("Color for state " + std::to_string(state_val) + " not found. Using fallback magenta.");
                Renderer::globallyLoggedMissingColors.insert(state_val);
            }
        }
    }

    // Batch rendering rectangles
    if (!renderAsPixels && !cellInfos.empty()) {
        std::map<SDL_Color, std::vector<SDL_Rect>, SdlColorCompare> batchedRects;
        for (const auto& info : cellInfos) {
            batchedRects[info.color].push_back(info.rect);
        }
        for (const auto& batch : batchedRects) {
            const SDL_Color& color = batch.first;
            const std::vector<SDL_Rect>& rects = batch.second;
            if (!rects.empty()) {
                SDL_SetRenderDrawColor(sdlRenderer_, color.r, color.g, color.b, color.a);
                SDL_RenderFillRects(sdlRenderer_, rects.data(), static_cast<int>(rects.size()));
            }
        }
    }

    // Batch rendering pixels
    if (renderAsPixels && !pixelInfos.empty()) {
        std::map<SDL_Color, std::vector<SDL_Point>, SdlColorCompare> batchedPoints;
        for (const auto& pInfo : pixelInfos) {
            batchedPoints[pInfo.color].push_back({pInfo.screenPos.x, pInfo.screenPos.y});
        }
        for (const auto& batch : batchedPoints) {
            const SDL_Color& color = batch.first;
            const std::vector<SDL_Point>& points = batch.second;
            if (!points.empty()) {
                SDL_SetRenderDrawColor(sdlRenderer_, color.r, color.g, color.b, color.a);
                SDL_RenderDrawPoints(sdlRenderer_, points.data(), static_cast<int>(points.size()));
            }
        }
    }
}

// renderGridLines remains the same
void Renderer::renderGridLines(const Viewport& viewport) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (!sdlRenderer_) return;

    float currentCellPixelSize = viewport.getCurrentCellSize();
    int screenW = viewport.getScreenWidth();
    int screenH = viewport.getScreenHeight();

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

    if (drawGridLines && currentCellPixelSize > 0) {
        SDL_SetRenderDrawColor(sdlRenderer_, gridLineColor_.r, gridLineColor_.g, gridLineColor_.b, gridLineColor_.a);

        Viewport::PointF worldTopLeftF = viewport.screenToWorldF({0,0});
        Viewport::PointF worldBottomRightF = viewport.screenToWorldF({screenW, screenH});

        int lineOffset = (gridLineWidth_ - 1) / 2;

        for (float wx = std::floor(worldTopLeftF.x); wx <= std::ceil(worldBottomRightF.x +1.0f); ++wx) {
            Point screenPos = viewport.worldToScreen({static_cast<int>(std::round(wx)), 0});
             if (screenPos.x + lineOffset >= -gridLineWidth_ && screenPos.x - lineOffset < screenW + gridLineWidth_) {
                if (gridLineWidth_ == 1) {
                    SDL_RenderDrawLine(sdlRenderer_, screenPos.x, 0, screenPos.x, screenH);
                } else {
                    SDL_Rect lineRect = {screenPos.x - lineOffset, 0, gridLineWidth_, screenH};
                    SDL_RenderFillRect(sdlRenderer_, &lineRect);
                }
            }
        }
        for (float wy = std::floor(worldTopLeftF.y); wy <= std::ceil(worldBottomRightF.y +1.0f); ++wy) {
            Point screenPos = viewport.worldToScreen({0, static_cast<int>(std::round(wy))});
            if (screenPos.y + lineOffset >= -gridLineWidth_ && screenPos.y - lineOffset < screenH + gridLineWidth_) {
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

// renderGrid calls the modified renderCells and renderGridLines
void Renderer::renderGrid(const CellSpace& cellSpace, const Viewport& viewport) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    auto timer = Timer::getTimer(Timer::Module::renderGrid);
    if (!sdlRenderer_) return;

    timer.start();

    SDL_Color backgroundColor = {220, 220, 220, 255};
    int defaultStateVal = cellSpace.getDefaultState();
    auto it_default_color = stateSdlColorMap_.find(defaultStateVal);
    if (it_default_color != stateSdlColorMap_.end()) {
        backgroundColor = it_default_color->second;
    } else {
        if (logger) logger->warn("Default state " + std::to_string(defaultStateVal) + " color not found in map. Using fallback background.");
    }

    SDL_SetRenderDrawColor(sdlRenderer_, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    SDL_RenderClear(sdlRenderer_);

    renderCells(cellSpace, viewport);
    renderGridLines(viewport);

    timer.stop();
}

void Renderer::renderMultiLineText(const std::string& text, int x, int y, SDL_Color color, int maxWidth, int& outHeight) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
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
                 currentY += fontLineSkip;
                 outHeight += fontLineSkip;
            }
            SDL_FreeSurface(surface);
        } else {
            currentY += fontLineSkip;
            outHeight += fontLineSkip;
        }
        if (ss.peek() != EOF && surface && surface->h < fontLineSkip ) {
             int gap = fontLineSkip - surface->h;
             currentY += std::max(0, gap);
             outHeight += std::max(0, gap);
        }
    }
}

void Renderer::renderUI(const std::string& commandText, bool showCommandInput,
                        const std::string& userMessage, const std::string& brushInfo,
                        const Viewport& viewport) {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (!sdlRenderer_) return;

    if (!isUiReady()) {
        static bool uiErrorLoggedOnce = false;
        if(!uiErrorLoggedOnce) {
           if(!fontLoadedSuccessfully_) {
               if (logger) logger->error("UI Font not available (fontLoadedSuccessfully_ is false), cannot render text elements.");
           } else if (!uiFont_) {
               if (logger) logger->error("UI Font pointer is null despite successful load flag. Cannot render text.");
           }
           uiErrorLoggedOnce = true;
        }
        return;
    }

    int screenW = viewport.getScreenWidth();
    int screenH = viewport.getScreenHeight();
    int textPadding = 5;
    int UIMargin = 10;
    int currentY = UIMargin;
    int fontLineSkip = TTF_FontLineSkip(uiFont_);
    if (fontLineSkip <= 0) fontLineSkip = (uiFont_ && TTF_FontHeight(uiFont_) > 0) ? TTF_FontHeight(uiFont_) + 2 : currentFontSize_ + 2;

    if (!brushInfo.empty()) {
        int brushInfoRenderedHeight = 0;
        int brushInfoMaxWidth = screenW / 3 - UIMargin;
        if (brushInfoMaxWidth < 100) brushInfoMaxWidth = 100;

        int actualBrushInfoTextWidth = 0;
        std::string firstLineBrush = brushInfo.substr(0, brushInfo.find('\n'));
        if(!firstLineBrush.empty()){
            TTF_SizeUTF8(uiFont_, firstLineBrush.c_str(), &actualBrushInfoTextWidth, nullptr);
        } else if (brushInfo.length() > 0) {
             TTF_SizeUTF8(uiFont_, brushInfo.c_str(), &actualBrushInfoTextWidth, nullptr);
        }

        renderMultiLineText(brushInfo, UIMargin + textPadding, currentY + textPadding, uiBrushInfoColor_, brushInfoMaxWidth, brushInfoRenderedHeight);

        if (brushInfoRenderedHeight > 0) {
            SDL_Rect bgRect = { UIMargin, currentY,
                                std::min(actualBrushInfoTextWidth + 2 * textPadding, brushInfoMaxWidth + 2 * textPadding),
                                brushInfoRenderedHeight + 2 * textPadding};
             bgRect.w = std::min(bgRect.w, screenW - 2 * UIMargin);

            SDL_SetRenderDrawColor(sdlRenderer_, uiBackgroundColor_.r, uiBackgroundColor_.g, uiBackgroundColor_.b, uiBackgroundColor_.a);
            SDL_RenderFillRect(sdlRenderer_, &bgRect);
            renderMultiLineText(brushInfo, UIMargin + textPadding, currentY + textPadding, uiBrushInfoColor_, brushInfoMaxWidth, brushInfoRenderedHeight);
            currentY += bgRect.h + UIMargin / 2;
        }
    }

    int messageRenderedHeight = 0;
    if (!userMessage.empty()) {
        int messageMaxWidth = screenW - (2 * UIMargin);

        renderMultiLineText(userMessage, UIMargin + textPadding, currentY + textPadding, uiMsgColor_, messageMaxWidth, messageRenderedHeight);

        if (messageRenderedHeight > 0) {
            int actualMsgTextWidth = 0;
            std::string firstLineMsg = userMessage.substr(0, userMessage.find('\n'));
             if(!firstLineMsg.empty()){
                TTF_SizeUTF8(uiFont_, firstLineMsg.c_str(), &actualMsgTextWidth, nullptr);
            } else if(userMessage.length() > 0){
                 TTF_SizeUTF8(uiFont_, userMessage.c_str(), &actualMsgTextWidth, nullptr);
            }

            SDL_Rect msgBgRect = {UIMargin, currentY,
                                  std::min(actualMsgTextWidth + 2 * textPadding, messageMaxWidth + 2 * textPadding),
                                  messageRenderedHeight + (2*textPadding)};
            msgBgRect.w = std::min(msgBgRect.w, screenW - 2*UIMargin);

            SDL_SetRenderDrawColor(sdlRenderer_, uiBackgroundColor_.r, uiBackgroundColor_.g, uiBackgroundColor_.b, uiBackgroundColor_.a);
            SDL_RenderFillRect(sdlRenderer_, &msgBgRect);
            renderMultiLineText(userMessage, UIMargin + textPadding, currentY + textPadding, uiMsgColor_, messageMaxWidth, messageRenderedHeight);
        }
    }

    if (showCommandInput) {
        std::string fullCommandText = "/" + commandText + "_";
        int cmdTextWidth = 0;
        int cmdTextHeight = 0;
        int cmdMaxWidth = screenW - (2*UIMargin) - (2*textPadding);

        SDL_Surface* textSurface = TTF_RenderText_Blended_Wrapped(uiFont_, fullCommandText.c_str(), uiTextColor_, static_cast<Uint32>(cmdMaxWidth));
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
                if (cmdBoxHeight < fontLineSkip + (2*textPadding)) {
                    cmdBoxHeight = fontLineSkip + (2*textPadding);
                }

                int cmd_y_pos = screenH - cmdBoxHeight - UIMargin;

                SDL_Rect uiBackgroundRect = { UIMargin, cmd_y_pos, screenW - (2*UIMargin), cmdBoxHeight };
                SDL_SetRenderDrawColor(sdlRenderer_, uiBackgroundColor_.r, uiBackgroundColor_.g, uiBackgroundColor_.b, uiBackgroundColor_.a);
                SDL_RenderFillRect(sdlRenderer_, &uiBackgroundRect);

                int text_y_offset = (cmdBoxHeight - cmdTextHeight) / 2;

                SDL_Rect uiTextRect = { UIMargin + textPadding, cmd_y_pos + textPadding + text_y_offset , cmdTextWidth, cmdTextHeight };
                uiTextRect.w = std::min(cmdTextWidth, cmdMaxWidth);

                SDL_RenderCopy(sdlRenderer_, textTexture, nullptr, &uiTextRect);
                SDL_DestroyTexture(textTexture);
            }
            SDL_FreeSurface(textSurface);
        }
    }
}

void Renderer::presentScreen() {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
    if (sdlRenderer_) {
        SDL_RenderPresent(sdlRenderer_);
    } else {
        if (logger) logger->error("sdlRenderer_ is null, cannot present.");
    }
}

void Renderer::cleanup() {
    auto logger = Logger::getLogger(Logger::Module::Renderer);
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
