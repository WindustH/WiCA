#include "application.h"
#include <SDL_image.h>
#include <SDL_ttf.h>
#include "../utils/logger.h" // New logger
#include "../utils/error_handler.h"
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
      cellSpace_(0,{}),
      ruleEngine_(),
      renderer_(),
      viewport_(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, DEFAULT_CELL_PIXEL_SIZE),
      snapshotManager_(),
      simulationPaused_(true),
      lastUpdateTime_(0),
      simulationSpeed_(10.0f),
      timePerUpdate_(100),
      simulationLag_(0),
      timePerFrame_(10),
      refreshLag_(0),
      currentBrushState_(1),
      currentBrushSize_(1),
      commandInputActive_(false),
      commandInputBuffer_(""),
      userMessage_(""),
      userMessageDisplayTime_(0),
      userMessageIsMultiLine_(false),
      showBrushInfo_(true)
       {
}

// Destructor
Application::~Application() {
    cleanupSubsystems();
    cleanupSDL();
}

// --- Initialization and Cleanup ---

bool Application::initializeSDL() {
    auto logger = Logger::getLogger(Logger::Module::Core);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        ErrorHandler::failure("SDL_Init failed.");
        return false;
    }

    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        if (logger) logger->error("IMG_Init failed to initialize all requested image formats. SDL_image Error: {}",std::string(IMG_GetError()));
    } else {
        if (logger) logger->info("SDL_image initialized successfully");
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
        ErrorHandler::failure("SDL_CreateWindow failed");
        return false;
    }
    if (logger) logger->info("SDL initialized (Video & Timer).");
    return true;
}

bool Application::initializeSubsystems(const std::string& configPath) {
    auto logger = Logger::getLogger(Logger::Module::Core);
    currentConfigPath_ = configPath; // Store the initial config path

    if (logger) logger->debug("initializeSubsystems loading config: {}", currentConfigPath_);

    if (!rule_.loadFromFile(currentConfigPath_)) {

        ErrorHandler::failure("Failed to load rule file: " + currentConfigPath_ + ". Using fallbacks.");
        // Attempt to initialize with some defaults if config load fails
        rule_ = Rule(); // Create a default/empty config
    } else {
        postMessageToUser("Config loaded: " + currentConfigPath_.substr(currentConfigPath_.find_last_of("/\\") + 1));
    }

    int configDefaultState = rule_.getDefaultState(); // Will use internal default if not loaded
    std::vector<Point> configNeighborhood = rule_.getNeighborhood();
    cellSpace_ = CellSpace(configDefaultState, configNeighborhood);

    const auto& availableStates = rule_.getStates();

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

        if (logger) logger->info("Config 'states' array is empty. Setting brush state to {}",currentBrushState_);
    }

    if (!ruleEngine_.initialize(rule_)) {
        ErrorHandler::failure("Failed to initialize RuleEngine.");
        return false;
    }

    // Renderer initialization depends on a valid window and config for colors
    if (!renderer_.initialize(window_, rule_)) { // Pass the loaded or default config
        ErrorHandler::failure("Failed to initialize Renderer.");
        return false;
    }

    setSimulationSpeed(simulationSpeed_);

    setAutoFitView(true);
    if (cellSpace_.areBoundsInitialized() && !cellSpace_.getNonDefaultCells().empty()) {
        centerViewOnGrid();
        viewport_.updateAutoFit(cellSpace_);
    } else {
        viewport_.setCenter({0.0f, 0.0f});
        float targetCellSize = viewport_.getDefaultCellSize();
        float currentActualCellSize = viewport_.getCurrentCellSize();

        if (std::abs(currentActualCellSize - targetCellSize) > 1e-5f) {
             viewport_.zoomToCellSize(targetCellSize, Point(viewport_.getScreenWidth()/2, viewport_.getScreenHeight()/2));
        }
         if (logger) logger->info("No initial cells, centering view on origin with default zoom.");
    }

    if (logger) logger->info("All subsystems initialized successfully.");
    return true;
}

void Application::cleanupSDL() {
    auto logger = Logger::getLogger(Logger::Module::Core);
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
    if (logger) logger->info("SDL cleaned up.");
}

void Application::cleanupSubsystems() {
    auto logger = Logger::getLogger(Logger::Module::Core);
    renderer_.cleanup();
    if (logger) logger->info("Subsystems cleaned up.");
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

void Application::loadRule(const std::string& configPath) {
    auto logger = Logger::getLogger(Logger::Module::Core);
    bool wasPaused = simulationPaused_;
    if (!wasPaused) pauseSimulation();

    postMessageToUser("Loading new rule: " + configPath + "...", 0, false);

    Rule newConfig;
    if (!newConfig.loadFromFile(configPath)) {
        if (logger) logger->error("Failed to load new rule file: " + configPath);
        postMessageToUser("Error: Failed to load config: " + configPath.substr(configPath.find_last_of("/\\") + 1), 5000);
        if(!wasPaused && isRunning_) resumeSimulation();
        return;
    }

    rule_ = newConfig; // Assign new config
    currentConfigPath_ = configPath; // Update current config path

    // Re-initialize CellSpace
    int newDefaultState = rule_.getDefaultState();
    std::vector<Point> newNeighborhood = rule_.getNeighborhood();
    cellSpace_ = CellSpace(newDefaultState, newNeighborhood); // Creates a new CellSpace, clearing old one

    // Re-initialize RuleEngine
    if (!ruleEngine_.initialize(rule_)) {
        ErrorHandler::failure("Failed to re-initialize RuleEngine with new config.");
        postMessageToUser("Error: Failed to apply new rules from config.", 5000);
        // Potentially revert to old config or stop simulation
    } else {
        if (logger) logger->info("RuleEngine re-initialized with new config.");
    }

    // Re-initialize Renderer colors
    renderer_.reinitializeColors(rule_);
    if (logger) logger->info("Renderer colors re-initialized.");

    // Update brush state based on new config
    const auto& availableStates = rule_.getStates();
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
    if (logger) logger->info("Brush state updated for new config: ", currentBrushState_);

    // Reset view
    if (viewport_.isAutoFitEnabled()) {
        viewport_.updateAutoFit(cellSpace_);
    } else {
        centerViewOnGrid(); // Center on (potentially empty) grid
    }

    postMessageToUser("Rule loaded: " + configPath.substr(configPath.find_last_of("/\\") + 1), 3000);
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
        refreshLag_ += elapsedTime;

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

        if (timePerFrame_ > 0) {
            while (refreshLag_ >= timePerFrame_) {
                renderScene();
                refreshLag_ -= timePerFrame_;
            }
        } else if (timePerFrame_ == 0) {
            renderScene();
            refreshLag_ = 0;
        }
    }
}

void Application::processInput() {
    inputHandler_.processEvents(viewport_);
}

void Application::updateSimulation() {
    if (simulationPaused_ || !ruleEngine_.isInitialized()) {
        return;
    }
    std::unordered_map<Point, int> changes = ruleEngine_.calculateForUpdate(cellSpace_);
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
    auto logger = Logger::getLogger(Logger::Module::Core);
    isRunning_ = false;
    if (logger) logger->info("Quit signal received.");
}

void Application::togglePause() {
    auto logger = Logger::getLogger(Logger::Module::Core);
    simulationPaused_ = !simulationPaused_;
    if (simulationPaused_) {
        postMessageToUser("Simulation Paused.");
        simulationLag_ = 0;
        if (logger) logger->info("Simulation paused.");
    } else {
        lastUpdateTime_ = SDL_GetTicks();
        simulationLag_ = 0;
        postMessageToUser("Simulation Resumed.");
        if (logger) logger->info("Simulation resumed.");
    }
}
void Application::pauseSimulation() {
    auto logger = Logger::getLogger(Logger::Module::Core);
    if (!simulationPaused_) {
        simulationPaused_ = true;
        simulationLag_ = 0;
        postMessageToUser("Simulation Paused.");
        if (logger) logger->info("Simulation paused by command.");
    }
}
void Application::resumeSimulation() {
    auto logger = Logger::getLogger(Logger::Module::Core);
    if (simulationPaused_) {
        simulationPaused_ = false;
        lastUpdateTime_ = SDL_GetTicks();
        simulationLag_ = 0;
        postMessageToUser("Simulation Resumed.");
        if (logger) logger->info("Simulation resumed by command.");
    }
}


void Application::setBrushState(int state) {
    auto logger = Logger::getLogger(Logger::Module::Core);
    bool isValidState = false;
    const auto& availableStates = rule_.getStates();

    if (rule_.isLoaded() && !availableStates.empty()) {
        for (int validState : availableStates) {
            if (state == validState) {
                isValidState = true;
                break;
            }
        }
    } else if (!rule_.isLoaded()) {
        if (state == 0 || state == 1) isValidState = true;
    }


    if (isValidState) {
        currentBrushState_ = state;
        postMessageToUser("Brush state: " + std::to_string(state));
    } else {
        if (logger) logger->warn("Attempted to set invalid brush state {}", std::to_string(state));
        std::string validStatesStr = "Valid states: ";
        if (availableStates.empty()) validStatesStr += "(none defined in config or config load failed)";
        for(size_t i=0; i<availableStates.size(); ++i) {validStatesStr += std::to_string(availableStates[i]) + (i < availableStates.size()-1 ? ", " : "");}
        postMessageToUser("Error: Invalid state " + std::to_string(state) + " for brush. " + validStatesStr);
    }
}

void Application::setBrushSize(int size) {
    auto logger = Logger::getLogger(Logger::Module::Core);
    if (size >= 1 && size <= 50) {
        currentBrushSize_ = size;
        if (logger) logger->info("Brush size set to {}", std::to_string(size));
        postMessageToUser("Brush size: " + std::to_string(size));
    } else {
        if (logger) logger->warn("Invalid brush size {}. Must be >= 1 and <=50.", std::to_string(size));
        postMessageToUser("Error: Brush size must be 1-50.");
    }
}

void Application::applyBrush(Point worldPos) {
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
    auto logger = Logger::getLogger(Logger::Module::Core);
    commandInputActive_ = !commandInputActive_;
    if (commandInputActive_) {
        commandInputBuffer_.clear();
        SDL_StartTextInput();
        if (logger) logger->info("Command input activated.");
        postMessageToUser("Command input ON. Press '/' or Esc to close.", 2000);
    } else {
        SDL_StopTextInput();
        if (logger) logger->info("Command input deactivated.");
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
    auto logger = Logger::getLogger(Logger::Module::Core);
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

    if (logger) logger->info("Executing command: {}", commandToExecute);

    commandParser_.parseAndExecute(commandToExecute);

    if (commandInputActive_) {
        toggleCommandInput();
    }
    // CommandParser posts "Unknown command..." if not recognized
}

void Application::setAppFontSize(int size) {
    auto logger = Logger::getLogger(Logger::Module::Core);
    if (size > 0 && size < 100) {
        if (renderer_.setFontSize(size)) {
            postMessageToUser("Font size set to " + std::to_string(size));
            if (logger) logger->info("Font size set to {}", std::to_string(size));
        } else {
            postMessageToUser("Error: Could not apply font size " + std::to_string(size) + ". Check logs.");
        }
    } else {
        postMessageToUser("Error: Invalid font size " + std::to_string(size) + ". Must be >0 and <100.", 3000);
        if (logger) logger->warn("Invalid font size requested: ", std::to_string(size));
    }
}

void Application::setAppFontPath(const std::string& path) {
    auto logger = Logger::getLogger(Logger::Module::Core);
    if (path.empty()) {
        postMessageToUser("Error: Font path cannot be empty.");
        return;
    }
    int fontSizeToUse = renderer_.isUiReady() ? renderer_.getCurrentFontSize() : DEFAULT_FONT_SIZE;

    if (renderer_.setFontPath(path, fontSizeToUse)) {
        postMessageToUser("Font set to: " + path.substr(path.find_last_of("/\\") + 1));
        if (logger) logger->info("Font set to path: {}", path);
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
    auto logger = Logger::getLogger(Logger::Module::Core);
    viewport_.setAutoFit(enabled, cellSpace_);
    if (enabled) {
        if (logger) logger->info("Autofit enabled.");
        postMessageToUser("Autofit ON.");
    } else {
        if (logger) logger->info("Autofit disabled.");
        postMessageToUser("Autofit OFF.");
    }
}

void Application::centerViewOnGrid() {
    auto logger = Logger::getLogger(Logger::Module::Core);
    if (cellSpace_.areBoundsInitialized() && !cellSpace_.getNonDefaultCells().empty()) {
        Point minB = cellSpace_.getMinBounds();
        Point maxB = cellSpace_.getMaxBounds();
        Viewport::PointF center(
            static_cast<float>(minB.x) + static_cast<float>(maxB.x - minB.x +1) / 2.0f,
            static_cast<float>(minB.y) + static_cast<float>(maxB.y - minB.y +1) / 2.0f
        );
        viewport_.setCenter(center);
        if (logger) logger->info("View centered on grid.");
        postMessageToUser("View centered.");
    } else {
        viewport_.setCenter({0.0f, 0.0f});
        if (logger) logger->info("Grid empty or no bounds, view centered on origin.");
        postMessageToUser("Grid is empty. Centered on origin (0,0).");
    }
}

void Application::saveSnapshot(const std::string& filename) {
    auto logger = Logger::getLogger(Logger::Module::Core);
    if (snapshotManager_.saveState(filename, cellSpace_)) {
        if (logger) logger->info("Snapshot saved to {}",filename);
        postMessageToUser("Snapshot saved: " + filename);
    } else {
        if (logger) logger->error("Failed to save snapshot to " + filename);
        postMessageToUser("Error: Failed to save snapshot " + filename);
    }
}

void Application::loadSnapshot(const std::string& filename) {
    auto logger = Logger::getLogger(Logger::Module::Core);
    bool wasPaused = simulationPaused_;
    if(!wasPaused) pauseSimulation();

    if (snapshotManager_.loadState(filename, cellSpace_)) {
        if (logger) logger->info("Snapshot loaded from {}", filename);
        postMessageToUser("Snapshot loaded: " + filename);
        if (viewport_.isAutoFitEnabled()) {
            viewport_.updateAutoFit(cellSpace_);
        } else {
            centerViewOnGrid();
        }
    } else {
        if (logger) logger->error("Failed to load snapshot from " + filename);
        postMessageToUser("Error: Failed to load snapshot " + filename);
    }

    if (!wasPaused && isRunning_) resumeSimulation();
}

void Application::clearSimulation() {
    auto logger = Logger::getLogger(Logger::Module::Core);
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
    if (logger) logger->info("Simulation grid cleared.");

    if (!wasPaused && isRunning_) resumeSimulation();
}

void Application::setSimulationSpeed(float updatesPerSecond) {
    auto logger = Logger::getLogger(Logger::Module::Core);
    if (updatesPerSecond <= 0.0f) {
        simulationSpeed_ = 0.1f;
        if (logger) logger->info("Invalid speed. Setting to minimum {} UPS.", std::to_string(simulationSpeed_));
    } else if (updatesPerSecond > 200.0f) {
        simulationSpeed_ = 200.0f;
         if (logger) logger->info("Speed too high. Setting to maximum {} UPS.", std::to_string(simulationSpeed_));
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


    if (logger) logger->info("Simulation speed set to " + std::to_string(simulationSpeed_) +
                           " UPS (Time per update: " + std::to_string(timePerUpdate_) + "ms).");
    postMessageToUser("Speed: " + std::to_string(simulationSpeed_) + " UPS");
}

void Application::onWindowResized(int newWidth, int newHeight) {
    auto logger = Logger::getLogger(Logger::Module::Core);
    if (newWidth > 0 && newHeight > 0) {
        viewport_.setScreenDimensions(newWidth, newHeight, cellSpace_);
        if (logger) logger->info("Window resized to {}x{}", std::to_string(newWidth), std::to_string(newHeight));
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
