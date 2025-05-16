#include "application.h"
#include "../utils/error_handler.h"
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <iostream>
#include <algorithm>
#include <limits>
#include <filesystem> // For checking file existence

// Constructor
Application::Application()
    : isRunning_(false),
      window_(nullptr),
      glContext_(nullptr),
      currentConfigPath_(""), // Initialize currentConfigPath_
      inputHandler_(*this),
      commandParser_(*this),
      cellSpace_(0),
      ruleEngine_(),
      renderer_(),
      viewport_(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, DEFAULT_CELL_PIXEL_SIZE),
      snapshotManager_(),
      simulationPaused_(true),
      lastUpdateTime_(0),
      simulationSpeed_(10.0f),
      timePerUpdate_(100),
      simulationLag_(0),
      currentBrushState_(1),
      currentBrushSize_(1),
      commandInputActive_(false),
      commandInputBuffer_(""),
      userMessage_(""),
      userMessageDisplayTime_(0),
      userMessageIsMultiLine_(false),
      showBrushInfo_(true)
       {
        std::cout << "[DEBUG] Application Constructor: Initial cellSpace defaultState = 0, currentBrushState = 1, simulationLag_ = 0" << std::endl;
}

// Destructor
Application::~Application() {
    cleanupSubsystems();
    cleanupSDL();
}

// --- Initialization and Cleanup ---

bool Application::initializeSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        ErrorHandler::sdlError("Application: SDL_Init failed", true);
        return false;
    }

    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        ErrorHandler::logError("Application: IMG_Init failed to initialize all requested image formats. SDL_image Error: " + std::string(IMG_GetError()), false);
    } else {
        ErrorHandler::logError("Application: SDL_image initialized successfully.", false);
    }

    window_ = SDL_CreateWindow(
        "SDL2 Cellular Automaton Simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        DEFAULT_SCREEN_WIDTH,
        DEFAULT_SCREEN_HEIGHT,
        SDL_WINDOW_RESIZABLE
    );
    if (!window_) {
        ErrorHandler::sdlError("Application: SDL_CreateWindow failed", true);
        return false;
    }
    ErrorHandler::logError("Application: SDL initialized (Video & Timer).", false);
    return true;
}

bool Application::initializeSubsystems(const std::string& configPath) {
    currentConfigPath_ = configPath; // Store the initial config path
    std::cout << "[DEBUG] Application::initializeSubsystems loading config: " << currentConfigPath_ << std::endl;

    if (!config_.loadFromFile(currentConfigPath_)) {
        ErrorHandler::logError("Application: CRITICAL - Failed to load configuration file: " + currentConfigPath_ + ". Using fallbacks.", true);
        // Attempt to initialize with some defaults if config load fails
        config_ = Rule(); // Create a default/empty config
    } else {
        postMessageToUser("Config loaded: " + currentConfigPath_.substr(currentConfigPath_.find_last_of("/\\") + 1));
    }

    int configDefaultState = config_.getDefaultState(); // Will use internal default if not loaded
    cellSpace_ = CellSpace(configDefaultState);
    std::cout << "[DEBUG] CellSpace initialized with default state: " << configDefaultState << std::endl;

    const auto& availableStates = config_.getStates();
    std::cout << "[DEBUG] Config available states: ";
    for(int s : availableStates) { std::cout << s << " "; }
    std::cout << "(Default is: " << configDefaultState << ")" << std::endl;

    if (!availableStates.empty()) {
        currentBrushState_ = availableStates[0];
        if (currentBrushState_ == configDefaultState) {
            bool foundNonDefault = false;
            for (size_t i = 0; i < availableStates.size(); ++i) {
                if (availableStates[i] != configDefaultState) {
                    currentBrushState_ = availableStates[i];
                    foundNonDefault = true;
                    break;
                }
            }
            if (!foundNonDefault) { // All available states are same as default, or only one state
                 currentBrushState_ = availableStates[0]; // Stick to the first one
            }
        }
    } else {
        currentBrushState_ = (configDefaultState == 1) ? 0 : 1;
        ErrorHandler::logError("Application: Config 'states' array is empty. Setting brush state to " + std::to_string(currentBrushState_) + " as a fallback.", false);
    }
    std::cout << "[DEBUG] Final initial brush state: " << currentBrushState_ << std::endl;

    if (!ruleEngine_.initialize(config_)) {
        ErrorHandler::logError("Application: Failed to initialize RuleEngine.", true);
        return false;
    }

    // Renderer initialization depends on a valid window and config for colors
    if (!renderer_.initialize(window_, config_)) { // Pass the loaded or default config
        ErrorHandler::logError("Application: Failed to initialize Renderer.", true);
        return false;
    }

    setSimulationSpeed(simulationSpeed_);

    setAutoFitView(true);
    if (cellSpace_.areBoundsInitialized() && !cellSpace_.getActiveCells().empty()) {
        centerViewOnGrid();
        viewport_.updateAutoFit(cellSpace_);
    } else {
        viewport_.setCenter({0.0f, 0.0f});
        float targetCellSize = viewport_.getDefaultCellSize();
        float currentActualCellSize = viewport_.getCurrentCellSize();

        if (std::abs(currentActualCellSize - targetCellSize) > 1e-5f) {
             viewport_.zoomToCellSize(targetCellSize, Point(viewport_.getScreenWidth()/2, viewport_.getScreenHeight()/2));
        }
         std::cout << "[DEBUG] No initial cells, centering view on origin with default zoom." << std::endl;
    }

    ErrorHandler::logError("Application: All subsystems initialized successfully.", false);
    return true;
}

void Application::cleanupSDL() {
    if (glContext_) {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    IMG_Quit();
    SDL_Quit();
    ErrorHandler::logError("Application: SDL cleaned up.", false);
}

void Application::cleanupSubsystems() {
    renderer_.cleanup();
    ErrorHandler::logError("Application: Subsystems cleaned up.", false);
}


bool Application::initialize(const std::string& configPath) {
    if (!initializeSDL()) {
        return false;
    }
    if (!initializeSubsystems(configPath)) { // Pass initial config path
        cleanupSDL();
        return false;
    }
    isRunning_ = true;
    lastUpdateTime_ = SDL_GetTicks();
    simulationLag_ = 0;
    postMessageToUser("Welcome! Type 'help' or press 'H' for commands.", 5000);
    return true;
}

void Application::loadConfiguration(const std::string& configPath) {
    bool wasPaused = simulationPaused_;
    if (!wasPaused) pauseSimulation();

    postMessageToUser("Loading new configuration: " + configPath + "...", 0, false);

    Rule newConfig;
    if (!newConfig.loadFromFile(configPath)) {
        ErrorHandler::logError("Application::loadConfiguration - CRITICAL - Failed to load new configuration file: " + configPath, true);
        postMessageToUser("Error: Failed to load config: " + configPath.substr(configPath.find_last_of("/\\") + 1), 5000);
        if(!wasPaused && isRunning_) resumeSimulation();
        return;
    }

    config_ = newConfig; // Assign new config
    currentConfigPath_ = configPath; // Update current config path

    // Re-initialize CellSpace
    int newDefaultState = config_.getDefaultState();
    cellSpace_ = CellSpace(newDefaultState); // Creates a new CellSpace, clearing old one
    std::cout << "[DEBUG] CellSpace re-initialized with new default state: " << newDefaultState << std::endl;

    // Re-initialize RuleEngine
    if (!ruleEngine_.initialize(config_)) {
        ErrorHandler::logError("Application::loadConfiguration - Failed to re-initialize RuleEngine with new config.", true);
        postMessageToUser("Error: Failed to apply new rules from config.", 5000);
        // Potentially revert to old config or stop simulation
    } else {
        std::cout << "[DEBUG] RuleEngine re-initialized with new config." << std::endl;
    }

    // Re-initialize Renderer colors
    renderer_.reinitializeColors(config_);
    std::cout << "[DEBUG] Renderer colors re-initialized." << std::endl;

    // Update brush state based on new config
    const auto& availableStates = config_.getStates();
    if (!availableStates.empty()) {
        currentBrushState_ = availableStates[0];
        if (currentBrushState_ == newDefaultState) {
             bool foundNonDefault = false;
            for (size_t i = 0; i < availableStates.size(); ++i) {
                if (availableStates[i] != newDefaultState) {
                    currentBrushState_ = availableStates[i];
                    foundNonDefault = true;
                    break;
                }
            }
            if (!foundNonDefault) currentBrushState_ = availableStates[0];
        }
    } else {
        currentBrushState_ = (newDefaultState == 1) ? 0 : 1;
    }
    std::cout << "[DEBUG] Brush state updated for new config: " << currentBrushState_ << std::endl;

    // Reset view
    if (viewport_.isAutoFitEnabled()) {
        viewport_.updateAutoFit(cellSpace_);
    } else {
        centerViewOnGrid(); // Center on (potentially empty) grid
    }

    postMessageToUser("Configuration loaded: " + configPath.substr(configPath.find_last_of("/\\") + 1), 3000);
    simulationLag_ = 0; // Reset simulation lag

    if (!wasPaused && isRunning_) resumeSimulation();
}


void Application::run() {
    Uint32 previousTime = SDL_GetTicks();

    while (isRunning_) {
        Uint32 currentTime = SDL_GetTicks();
        Uint32 elapsedTime = currentTime - previousTime;
        previousTime = currentTime;
        simulationLag_ += elapsedTime;

        processInput();

        if (!simulationPaused_ && timePerUpdate_ > 0) {
            while (simulationLag_ >= timePerUpdate_) {
                updateSimulation();
                simulationLag_ -= timePerUpdate_;
            }
        } else if (!simulationPaused_ && timePerUpdate_ == 0 && simulationSpeed_ > 0) {
            updateSimulation();
            simulationLag_ = 0;
        }
        renderScene();
    }
}

void Application::processInput() {
    inputHandler_.processEvents(cellSpace_, viewport_);
}

void Application::updateSimulation() {
    if (simulationPaused_ || !ruleEngine_.isInitialized()) {
        return;
    }
    std::unordered_map<Point, int> changes = ruleEngine_.calculateNextGeneration(cellSpace_);
    if (!changes.empty()) {
        cellSpace_.updateCells(changes);
        if (viewport_.isAutoFitEnabled()) {
            viewport_.updateAutoFit(cellSpace_);
        }
    }
}

void Application::renderScene() {
    std::string currentMessageToDisplay;
    if (userMessageIsMultiLine_ || (userMessageDisplayTime_ > 0 && SDL_GetTicks() < userMessageDisplayTime_)) {
        currentMessageToDisplay = userMessage_;
    } else if (!userMessage_.empty() && userMessageDisplayTime_ > 0 && SDL_GetTicks() >= userMessageDisplayTime_) {
        userMessage_.clear();
        userMessageDisplayTime_ = 0;
        userMessageIsMultiLine_ = false;
    }

    std::string brushInfoString;
    if (showBrushInfo_) {
        brushInfoString = "Brush: S" + std::to_string(currentBrushState_) +
                          " (Size: " + std::to_string(currentBrushSize_) + ")";
    }

    renderer_.renderGrid(cellSpace_, viewport_);
    // The commandInputBuffer_ is passed directly; Renderer adds the '/' for display
    renderer_.renderUI(commandInputBuffer_, commandInputActive_, currentMessageToDisplay, brushInfoString, viewport_);
    renderer_.presentScreen();
}


void Application::quit() {
    isRunning_ = false;
    ErrorHandler::logError("Application: Quit signal received.", false);
}

void Application::togglePause() {
    simulationPaused_ = !simulationPaused_;
    if (simulationPaused_) {
        postMessageToUser("Simulation Paused.");
        simulationLag_ = 0;
        ErrorHandler::logError("Application: Simulation paused.", false);
    } else {
        lastUpdateTime_ = SDL_GetTicks();
        simulationLag_ = 0;
        postMessageToUser("Simulation Resumed.");
        ErrorHandler::logError("Application: Simulation resumed.", false);
    }
}
void Application::pauseSimulation() {
    if (!simulationPaused_) {
        simulationPaused_ = true;
        simulationLag_ = 0;
        postMessageToUser("Simulation Paused.");
        ErrorHandler::logError("Application: Simulation paused by command.", false);
    }
}
void Application::resumeSimulation() {
    if (simulationPaused_) {
        simulationPaused_ = false;
        lastUpdateTime_ = SDL_GetTicks();
        simulationLag_ = 0;
        postMessageToUser("Simulation Resumed.");
        ErrorHandler::logError("Application: Simulation resumed by command.", false);
    }
}


void Application::setBrushState(int state) {
    bool isValidState = false;
    const auto& availableStates = config_.getStates();

    if (config_.isLoaded() && !availableStates.empty()) {
        for (int validState : availableStates) {
            if (state == validState) {
                isValidState = true;
                break;
            }
        }
    } else if (!config_.isLoaded()) {
        if (state == 0 || state == 1) isValidState = true;
    }


    if (isValidState) {
        currentBrushState_ = state;
        postMessageToUser("Brush state: " + std::to_string(state));
    } else {
        ErrorHandler::logError("Application: Attempted to set invalid brush state " + std::to_string(state), false);
        std::string validStatesStr = "Valid states: ";
        if (availableStates.empty()) validStatesStr += "(none defined in config or config load failed)";
        for(size_t i=0; i<availableStates.size(); ++i) {validStatesStr += std::to_string(availableStates[i]) + (i < availableStates.size()-1 ? ", " : "");}
        postMessageToUser("Error: Invalid state " + std::to_string(state) + " for brush. " + validStatesStr);
    }
}

void Application::setBrushSize(int size) {
    if (size >= 1 && size <= 50) {
        currentBrushSize_ = size;
        ErrorHandler::logError("Application: Brush size set to " + std::to_string(size), false);
        postMessageToUser("Brush size: " + std::to_string(size));
    } else {
        ErrorHandler::logError("Application: Invalid brush size " + std::to_string(size) + ". Must be >= 1 and <=50.", false);
        postMessageToUser("Error: Brush size must be 1-50.");
    }
}

void Application::applyBrush(Point worldPos, CellSpace& cs) {
    int halfSize = (currentBrushSize_ -1) / 2;
    for (int dy = -halfSize; dy <= halfSize; ++dy) {
        for (int dx = -halfSize; dx <= halfSize; ++dx) {
            Point cellToChange(worldPos.x + dx, worldPos.y + dy);
            cellSpace_.setCellState(cellToChange, currentBrushState_);
        }
    }
    if (viewport_.isAutoFitEnabled()) {
        viewport_.updateAutoFit(cellSpace_);
    }
}


void Application::toggleCommandInput() {
    commandInputActive_ = !commandInputActive_;
    if (commandInputActive_) {
        commandInputBuffer_.clear();
        SDL_StartTextInput();
        ErrorHandler::logError("Application: Command input activated.", false);
        postMessageToUser("Command input ON. Press '/' or Esc to close.", 2000);
    } else {
        SDL_StopTextInput();
        ErrorHandler::logError("Application: Command input deactivated.", false);
    }
}

bool Application::isCommandInputActive() const {
    return commandInputActive_;
}

void Application::appendCommandText(const std::string& text, bool isBackspace) {
    if (!commandInputActive_) return;

    if (isBackspace) {
        if (!commandInputBuffer_.empty()) {
            commandInputBuffer_.pop_back();
        }
    } else {
        commandInputBuffer_ += text;
    }
}

void Application::executeCommand() {
    if (!commandInputActive_ || commandInputBuffer_.empty()) {
        if (commandInputActive_) toggleCommandInput();
        return;
    }

    std::string commandToExecute = commandInputBuffer_;
    // Strip all leading '/' characters
    while (!commandToExecute.empty() && commandToExecute.front() == '/') {
        commandToExecute = commandToExecute.substr(1);
    }

    if (commandToExecute.empty()) { // If command was only slashes
        if (commandInputActive_) toggleCommandInput();
        return;
    }

    ErrorHandler::logError("Application: Executing command: " + commandToExecute, false);

    bool commandRecognized = commandParser_.parseAndExecute(commandToExecute, cellSpace_, viewport_, snapshotManager_);

    if (commandInputActive_) {
        toggleCommandInput();
    }
    // CommandParser posts "Unknown command..." if not recognized
}

void Application::setAppFontSize(int size) {
    if (size > 0 && size < 100) {
        if (renderer_.setFontSize(size)) {
            postMessageToUser("Font size set to " + std::to_string(size));
            ErrorHandler::logError("Application: Font size set to " + std::to_string(size), false);
        } else {
            postMessageToUser("Error: Could not apply font size " + std::to_string(size) + ". Check logs.");
        }
    } else {
        postMessageToUser("Error: Invalid font size " + std::to_string(size) + ". Must be >0 and <100.", 3000);
        ErrorHandler::logError("Application: Invalid font size requested: " + std::to_string(size), false);
    }
}

void Application::setAppFontPath(const std::string& path) {
    if (path.empty()) {
        postMessageToUser("Error: Font path cannot be empty.");
        return;
    }
    int fontSizeToUse = renderer_.isUiReady() ? renderer_.getCurrentFontSize() : DEFAULT_FONT_SIZE;

    if (renderer_.setFontPath(path, fontSizeToUse)) {
        postMessageToUser("Font set to: " + path.substr(path.find_last_of("/\\") + 1));
        ErrorHandler::logError("Application: Font set to path: " + path, false);
    } else {
        postMessageToUser("Error: Could not load font from path: " + path);
    }
}

void Application::setGridDisplayMode(const std::string& modeStr) {
    std::string modeLower = modeStr;
    std::transform(modeLower.begin(), modeLower.end(), modeLower.begin(), ::tolower);

    GridDisplayMode newMode;
    if (modeLower == "on") {
        newMode = GridDisplayMode::ON;
    } else if (modeLower == "off") {
        newMode = GridDisplayMode::OFF;
    } else if (modeLower == "auto") {
        newMode = GridDisplayMode::AUTO;
    } else {
        postMessageToUser("Error: Invalid grid display mode '" + modeStr + "'. Use auto, on, or off.");
        return;
    }
    renderer_.setGridDisplayMode(newMode);
    postMessageToUser("Grid display: " + modeLower);
}

void Application::setGridHideThreshold(int threshold) {
    if (threshold < 0) {
        postMessageToUser("Error: Grid hide threshold must be non-negative.");
        return;
    }
    renderer_.setGridHideThreshold(threshold);
    postMessageToUser("Grid hide threshold: " + std::to_string(threshold) + "px");
}

void Application::setGridLineWidth(int width) {
    if (width < 1) {
        postMessageToUser("Error: Grid line width must be 1 or greater.");
        return;
    }
    renderer_.setGridLineWidth(width);
    postMessageToUser("Grid line width set to " + std::to_string(width) + "px.");
}

void Application::setGridLineColor(int r, int g, int b, int a) {
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) {
        postMessageToUser("Error: Color components must be between 0 and 255.");
        return;
    }
    renderer_.setGridLineColor(static_cast<Uint8>(r), static_cast<Uint8>(g), static_cast<Uint8>(b), static_cast<Uint8>(a));
    postMessageToUser("Grid line color set.");
}


bool Application::isViewportAutoFitEnabled() const {
    return viewport_.isAutoFitEnabled();
}


void Application::setAutoFitView(bool enabled) {
    viewport_.setAutoFit(enabled, cellSpace_);
    if (enabled) {
        ErrorHandler::logError("Application: Autofit enabled.", false);
        postMessageToUser("Autofit ON.");
    } else {
        ErrorHandler::logError("Application: Autofit disabled.", false);
        postMessageToUser("Autofit OFF.");
    }
}

void Application::centerViewOnGrid() {
    if (cellSpace_.areBoundsInitialized() && !cellSpace_.getActiveCells().empty()) {
        Point minB = cellSpace_.getMinBounds();
        Point maxB = cellSpace_.getMaxBounds();
        Viewport::PointF center(
            static_cast<float>(minB.x) + static_cast<float>(maxB.x - minB.x +1) / 2.0f,
            static_cast<float>(minB.y) + static_cast<float>(maxB.y - minB.y +1) / 2.0f
        );
        viewport_.setCenter(center);
        ErrorHandler::logError("Application: View centered on grid.", false);
        postMessageToUser("View centered.");
    } else {
        viewport_.setCenter({0.0f, 0.0f});
        ErrorHandler::logError("Application: Grid empty or no bounds, view centered on origin.", false);
        postMessageToUser("Grid is empty. Centered on origin (0,0).");
    }
}

void Application::saveSnapshot(const std::string& filename) {
    if (snapshotManager_.saveState(filename, cellSpace_)) {
        ErrorHandler::logError("Application: Snapshot saved to " + filename, false);
        postMessageToUser("Snapshot saved: " + filename);
    } else {
        ErrorHandler::logError("Application: Failed to save snapshot to " + filename, true);
        postMessageToUser("Error: Failed to save snapshot " + filename);
    }
}

void Application::loadSnapshot(const std::string& filename) {
    bool wasPaused = simulationPaused_;
    if(!wasPaused) pauseSimulation();

    if (snapshotManager_.loadState(filename, cellSpace_)) {
        ErrorHandler::logError("Application: Snapshot loaded from " + filename, false);
        postMessageToUser("Snapshot loaded: " + filename);
        if (viewport_.isAutoFitEnabled()) {
            viewport_.updateAutoFit(cellSpace_);
        } else {
            centerViewOnGrid();
        }
    } else {
        ErrorHandler::logError("Application: Failed to load snapshot from " + filename, true);
        postMessageToUser("Error: Failed to load snapshot " + filename);
    }

    if (!wasPaused && isRunning_) resumeSimulation();
}

void Application::clearSimulation() {
    bool wasPaused = simulationPaused_;
    if(!wasPaused) pauseSimulation();

    cellSpace_.clear();

    if (viewport_.isAutoFitEnabled()) {
        viewport_.updateAutoFit(cellSpace_);
    } else {
         viewport_.setCenter({0.0f, 0.0f});
         float defaultCellSize = viewport_.getDefaultCellSize();
         viewport_.zoomToCellSize(defaultCellSize, Point(viewport_.getScreenWidth()/2, viewport_.getScreenHeight()/2));
    }
    postMessageToUser("Grid cleared.");
    ErrorHandler::logError("Application: Simulation grid cleared.", false);

    if (!wasPaused && isRunning_) resumeSimulation();
}

void Application::setSimulationSpeed(float updatesPerSecond) {
    if (updatesPerSecond <= 0.0f) {
        simulationSpeed_ = 0.1f;
        ErrorHandler::logError("Application: Invalid speed. Setting to minimum " + std::to_string(simulationSpeed_) + " UPS.", false);
    } else if (updatesPerSecond > 200.0f) {
        simulationSpeed_ = 200.0f;
         ErrorHandler::logError("Application: Speed too high. Setting to maximum " + std::to_string(simulationSpeed_) + " UPS.", false);
    }
    else {
        simulationSpeed_ = updatesPerSecond;
    }

    if (simulationSpeed_ > 0.0f) {
        timePerUpdate_ = static_cast<Uint32>(1000.0f / simulationSpeed_);
        if (timePerUpdate_ == 0) timePerUpdate_ = 1;
    } else {
        timePerUpdate_ = std::numeric_limits<Uint32>::max();
    }


    ErrorHandler::logError("Application: Simulation speed set to " + std::to_string(simulationSpeed_) +
                           " UPS (Time per update: " + std::to_string(timePerUpdate_) + "ms).", false);
    postMessageToUser("Speed: " + std::to_string(simulationSpeed_) + " UPS");
}

void Application::onWindowResized(int newWidth, int newHeight) {
    if (newWidth > 0 && newHeight > 0) {
        viewport_.setScreenDimensions(newWidth, newHeight, cellSpace_);
        ErrorHandler::logError("Application: Window resized to " + std::to_string(newWidth) + "x" + std::to_string(newHeight), false);
    }
}

void Application::postMessageToUser(const std::string& message, Uint32 durationMs, bool isMultiLine) {
    userMessage_ = message;
    userMessageIsMultiLine_ = isMultiLine;
    if (isMultiLine || durationMs == 0) {
        userMessageDisplayTime_ = std::numeric_limits<Uint32>::max() -1;
    } else {
        userMessageDisplayTime_ = SDL_GetTicks() + durationMs;
    }

    std::cout << "USER_MSG: " << message << (isMultiLine ? " (multi-line)" : "") << (durationMs > 0 && durationMs != (std::numeric_limits<Uint32>::max()-1) ? " for " + std::to_string(durationMs) + "ms" : " (persistent)") << std::endl;
}

int Application::getCurrentBrushState() const {
    return currentBrushState_;
}

int Application::getCurrentBrushSize() const {
    return currentBrushSize_;
}

void Application::toggleBrushInfoDisplay() {
    showBrushInfo_ = !showBrushInfo_;
    postMessageToUser(showBrushInfo_ ? "Brush info: ON" : "Brush info: OFF", 1500);
}

bool Application::shouldShowBrushInfo() const {
    return showBrushInfo_;
}

std::string Application::getHelpString() const {
    return "Available Commands (use '-' for spaces in command names):\n"
           "  save <file>              Saves current state\n"
           "  load <file>              Loads state from file\n"
           "  load-config <file>       Loads new JSON rules & colors\n"
           "  brush-state <val>        Sets brush state (integer)\n"
           "  brush-size <val>         Sets brush size (e.g. 1, 3)\n"
           "  font-size <points>       Sets UI font size (e.g. 16)\n"
           "  set-font <path>          Sets UI font from file path\n"
           "  set-grid-display <mode>  Grid: auto, on, or off\n"
           "  set-grid-threshold <px>  Grid hide threshold for auto mode\n"
           "  set-grid-width <px>      Sets grid line thickness\n"
           "  set-grid-color <r g b [a]> Sets grid line color (0-255)\n"
           "  pause / resume           Toggles simulation pause\n"
           "  autofit <on|off>         Toggles viewport autofit (or toggle)\n"
           "  center                   Centers view on active cells\n"
           "  speed <ups>              Sets simulation speed (updates/sec)\n"
           "  clear-grid / clear       Clears all active cells\n"
           "  toggle-brush-info        Shows/hides brush state on screen\n"
           "  help / h / ?             Shows this help message\n"
           "  quit / exit              Exits the application\n"
           "Shortcuts:\n"
           "  Space: Toggle Pause | / : Command Mode | H : This Help\n"
           "  Esc: Quit Program / Close Command Mode\n"
           "  Mouse Wheel: Zoom | Middle Mouse Drag: Pan | Left Mouse: Paint";
}

void Application::displayHelp() {
    postMessageToUser(getHelpString(), 0, true);
}
