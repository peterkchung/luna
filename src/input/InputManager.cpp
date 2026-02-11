// About: InputManager implementation — key polling, mouse tracking, scroll, cursor capture.

#include "input/InputManager.h"
#include <GLFW/glfw3.h>
#include <cstring>

namespace luna::input {

InputManager::InputManager(GLFWwindow* window) : window_(window) {
    glfwSetWindowUserPointer(window_, this);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwGetCursorPos(window_, &mouseX_, &mouseY_);
    lastMX_ = mouseX_;
    lastMY_ = mouseY_;
}

void InputManager::update() {
    // Save previous key state
    std::memcpy(keysPrevious_, keysCurrent_, sizeof(keysCurrent_));

    // Poll current key state
    for (int i = 0; i < MAX_KEYS; i++)
        keysCurrent_[i] = (glfwGetKey(window_, i) == GLFW_PRESS);

    // Mouse position and delta
    glfwGetCursorPos(window_, &mouseX_, &mouseY_);
    if (firstMouse_) {
        lastMX_ = mouseX_;
        lastMY_ = mouseY_;
        firstMouse_ = false;
    }
    mouseDX_ = mouseX_ - lastMX_;
    mouseDY_ = mouseY_ - lastMY_;
    lastMX_ = mouseX_;
    lastMY_ = mouseY_;

    // Consume accumulated scroll
    scrollDY_ = pendingScrollDY_;
    pendingScrollDY_ = 0.0;
}

bool InputManager::isKeyDown(int key) const {
    return key >= 0 && key < MAX_KEYS && keysCurrent_[key];
}

bool InputManager::isKeyPressed(int key) const {
    return key >= 0 && key < MAX_KEYS && keysCurrent_[key] && !keysPrevious_[key];
}

void InputManager::setCursorCaptured(bool captured) {
    cursorCaptured_ = captured;
    glfwSetInputMode(window_, GLFW_CURSOR,
                     captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (captured) firstMouse_ = true;
}

void InputManager::toggleCursorCapture() {
    setCursorCaptured(!cursorCaptured_);
}

void InputManager::scrollCallback(GLFWwindow* window, double /*xoff*/, double yoff) {
    auto* mgr = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (mgr) mgr->pendingScrollDY_ += yoff;
}

} // namespace luna::input
