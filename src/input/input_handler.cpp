#include "input_handler.h"
#include "../core/application.h"
#include "../ca/cell_space.h"
#include "../rendering/viewport.h"
#include "../utils/error_handler.h"
#include <cstring> // For strlen in SDL_TEXTINPUT handling
#include <iostream> // For std::cout debugging

/**
 * @brief Constructor for InputHandler.
 * @param app Reference to the main Application object.
 */
InputHandler::InputHandler(Application& app)
    : application_(app),
      middleMouseDown_(false),
      lastMousePos_(0,0),
      leftMouseDown_(false) {
}

/**
 * @brief Polls and processes SDL events.
 * @param cellSpace Reference to CellSpace for brush operations.
 * @param viewport Reference to Viewport for view manipulations.
 */
void InputHandler::processEvents(CellSpace& cellSpace, Viewport& viewport) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                application_.quit();
                break;
            case SDL_KEYDOWN:
                handleKeyDown(event.key, viewport);
                break;
            case SDL_TEXTINPUT:
                if (application_.isCommandInputActive()) {
                    // std::cout << "SDL_TEXTINPUT event: " << event.text.text << std::endl; // Debug
                    handleTextInput(event.text);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                // std::cout << "SDL_MOUSEBUTTONDOWN event: " << (int)event.button.button << " at (" << event.button.x << "," << event.button.y << ")" << std::endl; // Debug
                handleMouseButtonDown(event.button, cellSpace, viewport);
                break;
            case SDL_MOUSEBUTTONUP:
                handleMouseButtonUp(event.button);
                break;
            case SDL_MOUSEMOTION:
                // std::cout << "SDL_MOUSEMOTION event to (" << event.motion.x << "," << event.motion.y << ")" << std::endl; // Debug
                handleMouseMotion(event.motion, cellSpace, viewport);
                break;
            case SDL_MOUSEWHEEL:
                handleMouseWheel(event.wheel, viewport);
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    application_.onWindowResized(event.window.data1, event.window.data2);
                }
                break;
            default:
                break;
        }
    }
}

/**
 * @brief Handles text input events (for command input).
 * @param textEvent The SDL_TextInputEvent containing the input character(s).
 */
void InputHandler::handleTextInput(const SDL_TextInputEvent& textEvent) {
    application_.appendCommandText(textEvent.text);
}


/**
 * @brief Handles key down events.
 * @param keyEvent The SDL_KeyboardEvent.
 * @param viewport Reference to the Viewport.
 */
void InputHandler::handleKeyDown(const SDL_KeyboardEvent& keyEvent, Viewport& viewport) {
    if (application_.isCommandInputActive()) {
        switch (keyEvent.keysym.sym) {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                // std::cout << "Enter pressed in command mode." << std::endl; // Debug
                application_.executeCommand();
                break;
            case SDLK_ESCAPE:
                application_.toggleCommandInput();
                break;
            case SDLK_BACKSPACE:
                application_.appendCommandText("", true);
                break;
            default:
                break;
        }
    } else {
        if (keyEvent.repeat != 0) return;

        switch (keyEvent.keysym.sym) {
            case SDLK_SPACE:
                application_.togglePause();
                break;
            case SDLK_SLASH:
                if (!(SDL_GetModState() & KMOD_SHIFT)) {
                    application_.toggleCommandInput();
                }
                break;
            case SDLK_ESCAPE:
                application_.quit();
                break;
            case SDLK_h:
                 application_.displayHelp();
                 break;
            default:
                break;
        }
    }
}

/**
 * @brief Handles mouse button down events.
 */
void InputHandler::handleMouseButtonDown(const SDL_MouseButtonEvent& buttonEvent, CellSpace& cellSpace, Viewport& viewport) {
    lastMousePos_ = Point(buttonEvent.x, buttonEvent.y);

    if (buttonEvent.button == SDL_BUTTON_LEFT) {
        // std::cout << "Left mouse down." << std::endl; // Debug
        leftMouseDown_ = true;
        Point worldPos = viewport.screenToWorld(lastMousePos_);
        // std::cout << "Brush applied at screen (" << lastMousePos_.x << "," << lastMousePos_.y << ") -> world (" << worldPos.x << "," << worldPos.y << ")" << std::endl; // Debug
        application_.applyBrush(worldPos, cellSpace);
    } else if (buttonEvent.button == SDL_BUTTON_MIDDLE) {
        middleMouseDown_ = true;
    }
}

/**
 * @brief Handles mouse button up events.
 */
void InputHandler::handleMouseButtonUp(const SDL_MouseButtonEvent& buttonEvent) {
    if (buttonEvent.button == SDL_BUTTON_LEFT) {
        // std::cout << "Left mouse up." << std::endl; // Debug
        leftMouseDown_ = false;
    } else if (buttonEvent.button == SDL_BUTTON_MIDDLE) {
        middleMouseDown_ = false;
    }
}

/**
 * @brief Handles mouse motion events.
 */
void InputHandler::handleMouseMotion(const SDL_MouseMotionEvent& motionEvent, CellSpace& cellSpace, Viewport& viewport) {
    Point currentMousePos(motionEvent.x, motionEvent.y);
    if (middleMouseDown_) {
        Point delta(currentMousePos.x - lastMousePos_.x, currentMousePos.y - lastMousePos_.y);
        viewport.pan(delta);
    } else if (leftMouseDown_) {
        // std::cout << "Left mouse drag." << std::endl; // Debug
        Point worldPos = viewport.screenToWorld(currentMousePos);
        // std::cout << "Brush dragged at screen (" << currentMousePos.x << "," << currentMousePos.y << ") -> world (" << worldPos.x << "," << worldPos.y << ")" << std::endl; // Debug
        application_.applyBrush(worldPos, cellSpace);
    }
    lastMousePos_ = currentMousePos; // Update lastMousePos for both panning and next drag if LMD is still down
}

/**
 * @brief Handles mouse wheel events for zooming.
 */
void InputHandler::handleMouseWheel(const SDL_MouseWheelEvent& wheelEvent, Viewport& viewport) {
    float zoomFactor = 1.0f;
    int effectiveY = wheelEvent.y;
    if (wheelEvent.direction == SDL_MOUSEWHEEL_FLIPPED) {
        effectiveY *= -1;
    }

    if (effectiveY > 0) {
        zoomFactor = 1.2f;
    } else if (effectiveY < 0) {
        zoomFactor = 1.0f / 1.2f;
    }

    if (zoomFactor != 1.0f) {
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        viewport.zoom(zoomFactor, Point(mouseX, mouseY));
    }
}
