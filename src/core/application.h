#ifndef APPLICATION_H
#define APPLICATION_H

#include <string>
#include <vector>
#include <SDL.h>

#include "config.h"
#include "../ca/cell_space.h"
#include "../ca/rule_engine.h"
#include "../rendering/renderer.h"
#include "../rendering/viewport.h"
#include "../input/input_handler.h"
#include "../input/command_parser.h"
#include "../file_io/snapshot_manager.h"
#include "../utils/point.h"


const int DEFAULT_SCREEN_WIDTH = 1280;
const int DEFAULT_SCREEN_HEIGHT = 720;
const float DEFAULT_CELL_PIXEL_SIZE = 10.0f;
const int DEFAULT_FONT_SIZE = 16;

/**
 * @class Application
 * @brief Main application class, orchestrates the entire simulation.
 */
class Application {
private:
    bool isRunning_;
    SDL_Window* window_;
    SDL_GLContext glContext_;

    Config config_; // Current configuration
    std::string currentConfigPath_; // Path to the current configuration file

    InputHandler inputHandler_;
    CommandParser commandParser_;
    CellSpace cellSpace_;
    RuleEngine ruleEngine_;
    Renderer renderer_;
    Viewport viewport_;
    SnapshotManager snapshotManager_;

    bool simulationPaused_;
    Uint32 lastUpdateTime_;
    float simulationSpeed_;
    Uint32 timePerUpdate_;
    Uint32 simulationLag_;

    int currentBrushState_;
    int currentBrushSize_;

    bool commandInputActive_;
    std::string commandInputBuffer_;

    std::string userMessage_;
    Uint32 userMessageDisplayTime_;
    bool userMessageIsMultiLine_;

    bool showBrushInfo_;


    bool initializeSDL();
    bool initializeSubsystems(const std::string& configPath); // Takes path for initial load
    void cleanupSDL();
    void cleanupSubsystems();

    void processInput();
    void updateSimulation();
    void renderScene();

public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool initialize(const std::string& configPath);
    void run();
    void quit();

    // Configuration
    /**
     * @brief Loads a new configuration file and reinitializes the simulation.
     * @param configPath Path to the new JSON configuration file.
     */
    void loadConfiguration(const std::string& configPath);

    // Simulation control
    void togglePause();
    void pauseSimulation();
    void resumeSimulation();
    void setSimulationSpeed(float updatesPerSecond);

    // Brush control
    void setBrushState(int state);
    void setBrushSize(int size);
    void applyBrush(Point worldPos, CellSpace& cs);
    int getCurrentBrushState() const;
    int getCurrentBrushSize() const;
    void toggleBrushInfoDisplay();
    bool shouldShowBrushInfo() const;


    // Command input
    void toggleCommandInput();
    bool isCommandInputActive() const;
    void appendCommandText(const std::string& text, bool isBackspace = false);
    void executeCommand();

    // Font and UI settings
    void setAppFontSize(int size);
    void setAppFontPath(const std::string& path);
    void setGridDisplayMode(const std::string& mode);
    void setGridHideThreshold(int threshold);
    void setGridLineWidth(int width); // New: sets grid line width
    void setGridLineColor(int r, int g, int b, int a = 255); // New: sets grid line color


    // Viewport control
    void setAutoFitView(bool enabled);
    bool isViewportAutoFitEnabled() const;
    void centerViewOnGrid();

    // File operations
    void saveSnapshot(const std::string& filename);
    void loadSnapshot(const std::string& filename);

    // Grid operations
    void clearSimulation();

    // System events
    void onWindowResized(int newWidth, int newHeight);

    // User feedback
    void postMessageToUser(const std::string& message, Uint32 durationMs = 3000, bool isMultiLine = false);
    void displayHelp();
    std::string getHelpString() const;
};

#endif // APPLICATION_H
