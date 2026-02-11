// About: GLFW keyboard and mouse input polling with edge detection and cursor capture.

#pragma once

struct GLFWwindow;

namespace luna::input {

class InputManager {
public:
    explicit InputManager(GLFWwindow* window);

    void update();

    bool isKeyDown(int key) const;
    bool isKeyPressed(int key) const;

    double mouseX()  const { return mouseX_; }
    double mouseY()  const { return mouseY_; }
    double mouseDX() const { return mouseDX_; }
    double mouseDY() const { return mouseDY_; }
    double scrollDY() const { return scrollDY_; }

    bool isCursorCaptured() const { return cursorCaptured_; }
    void setCursorCaptured(bool captured);
    void toggleCursorCapture();

    GLFWwindow* window() const { return window_; }

private:
    static void scrollCallback(GLFWwindow* window, double xoff, double yoff);

    GLFWwindow* window_;
    bool cursorCaptured_ = false;
    bool firstMouse_     = true;

    double mouseX_  = 0.0, mouseY_  = 0.0;
    double lastMX_  = 0.0, lastMY_  = 0.0;
    double mouseDX_ = 0.0, mouseDY_ = 0.0;
    double scrollDY_ = 0.0;
    double pendingScrollDY_ = 0.0;

    static constexpr int MAX_KEYS = 512;
    bool keysCurrent_[MAX_KEYS]  = {};
    bool keysPrevious_[MAX_KEYS] = {};
};

} // namespace luna::input
