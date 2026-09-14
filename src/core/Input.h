#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class Input {
public:
    static void init(GLFWwindow* w);
    static bool keyDown(int key);
    static bool keyPressed(int key);
    static bool mouseDown(int button);
    static bool mousePressed(int button);
    static glm::vec2 mouseDelta();
    static void setCursorMode(bool captured);
    static void endFrame();

private:
    static GLFWwindow* s_window;
    static glm::vec2 s_lastMouse;
    static glm::vec2 s_delta;
    static bool s_firstMouse;
    static bool s_captured;
    static bool s_keyState[512];
    static bool s_keyPrev[512];
    static bool s_mouseState[8];
    static bool s_mousePrev[8];
};