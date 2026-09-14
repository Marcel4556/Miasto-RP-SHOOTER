#pragma once
#include <GLFW/glfw3.h>
#include <string>

class Window {
public:
    Window(int w, int h, const std::string& title);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const { return glfwWindowShouldClose(m_window); }
    void pollEvents() const { glfwPollEvents(); }
    bool wasResized() const { return m_resized; }
    void resetResizedFlag() const { m_resized = false; }   // <-- const

    GLFWwindow* handle() const { return m_window; }
    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    GLFWwindow* m_window = nullptr;
    int m_width, m_height;
    mutable bool m_resized = false;   // <-- mutable
};