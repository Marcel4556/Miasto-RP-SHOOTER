#include "core/Input.h"

GLFWwindow* Input::s_window = nullptr;
glm::vec2 Input::s_lastMouse{ 0 };
glm::vec2 Input::s_delta{ 0 };
bool Input::s_firstMouse = true;
bool Input::s_captured = false;
bool Input::s_keyState[512] = {};
bool Input::s_keyPrev[512] = {};
bool Input::s_mouseState[8] = {};
bool Input::s_mousePrev[8] = {};

void Input::init(GLFWwindow* w) {
    s_window = w;
    glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    s_captured = true;

    glfwSetCursorPosCallback(w, [](GLFWwindow*, double x, double y) {
        if (s_firstMouse) {
            s_lastMouse = { (float)x, (float)y };
            s_firstMouse = false;
            return;
        }
        if (!s_captured) return;
        glm::vec2 cur{ (float)x, (float)y };
        s_delta += cur - s_lastMouse;
        s_lastMouse = cur;
        });
}

bool Input::keyDown(int key) {
    if (key < 0 || key >= 512) return false;
    return glfwGetKey(s_window, key) == GLFW_PRESS;
}

bool Input::keyPressed(int key) {
    if (key < 0 || key >= 512) return false;
    return s_keyState[key] && !s_keyPrev[key];
}

bool Input::mouseDown(int button) {
    if (button < 0 || button >= 8) return false;
    return glfwGetMouseButton(s_window, button) == GLFW_PRESS;
}

bool Input::mousePressed(int button) {
    if (button < 0 || button >= 8) return false;
    return s_mouseState[button] && !s_mousePrev[button];
}

glm::vec2 Input::mouseDelta() {
    return s_captured ? s_delta : glm::vec2{ 0, 0 };
}

void Input::setCursorMode(bool captured) {
    if (captured == s_captured) return;
    s_captured = captured;
    glfwSetInputMode(s_window, GLFW_CURSOR,
        captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (!captured) s_firstMouse = true;
}

void Input::endFrame() {
    s_delta = { 0, 0 };
    for (int i = 0; i < 512; ++i) {
        s_keyPrev[i] = s_keyState[i];
        s_keyState[i] = keyDown(i);
    }
    for (int i = 0; i < 8; ++i) {
        s_mousePrev[i] = s_mouseState[i];
        s_mouseState[i] = mouseDown(i);
    }
}