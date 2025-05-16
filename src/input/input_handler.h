#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <SDL.h>
#include "../utils/point.h"

// Forward declarations of classes that InputHandler interacts with or calls methods on.
class Application;
class CellSpace;
class Viewport;

/**
 * @class InputHandler
 * @brief Processes raw SDL input events (mouse, keyboard) and translates them
 * into actions or commands for the application.
 */
class InputHandler {
private:
    Application& application_;

    bool middleMouseDown_;
    Point lastMousePos_;
    bool leftMouseDown_;

    // Helper methods to delegate specific event types
    void handleKeyDown(const SDL_KeyboardEvent& keyEvent);
    // void handleKeyUp(const SDL_KeyboardEvent& keyEvent); // Kept for completeness, but not used much currently
    void handleTextInput(const SDL_TextInputEvent& textEvent);
    void handleMouseButtonDown(const SDL_MouseButtonEvent& buttonEvent, Viewport& viewport);
    void handleMouseButtonUp(const SDL_MouseButtonEvent& buttonEvent);
    void handleMouseMotion(const SDL_MouseMotionEvent& motionEvent, Viewport& viewport);
    void handleMouseWheel(const SDL_MouseWheelEvent& wheelEvent, Viewport& viewport);

public:
    /**
     * @brief Constructor for InputHandler.
     * @param app A reference to the main Application object.
     */
    InputHandler(Application& app);

    /**
     * @brief Polls and processes all pending SDL events.
     * @param cellSpace Reference to the CellSpace for brush operations.
     * @param viewport Reference to the Viewport for mouse coordinate conversions and view changes.
     */
    void processEvents(Viewport& viewport);
};

#endif // INPUT_HANDLER_H
