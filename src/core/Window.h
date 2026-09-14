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
    void resetResizedFlag() const { m_resized = false; }

    // === Fullscreen ===
    bool isFullscreen() const { return m_fullscreen; }
    void setFullscreen(bool enable);
    void toggleFullscreen() { setFullscreen(!m_fullscreen); }

    GLFWwindow* handle() const { return m_window; }
    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    GLFWwindow* m_window = nullptr;
    int m_width, m_height;
    mutable bool m_resized = false;

    bool m_fullscreen = false;

    // Zapamietana pozycja/rozmiar okna sprzed fullscreen
    int m_savedX = 0, m_savedY = 0;
    int m_savedW = 0, m_savedH = 0;
};