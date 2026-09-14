#include "core/Window.h"
#include <stdexcept>
#include <iostream>

Window::Window(int w, int h, const std::string& title)
    : m_width(w), m_height(h)
{
    if (!glfwInit()) throw std::runtime_error("GLFW init failed");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(w, h, title.c_str(), nullptr, nullptr);
    if (!m_window) { glfwTerminate(); throw std::runtime_error("Window creation failed"); }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* win, int width, int height) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        self->m_width = width;
        self->m_height = height;
        self->m_resized = true;
        });
}

Window::~Window() {
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Window::setFullscreen(bool enable) {
    if (enable == m_fullscreen) return;

    if (enable) {
        // Zapamietaj obecny stan okna
        glfwGetWindowPos(m_window, &m_savedX, &m_savedY);
        glfwGetWindowSize(m_window, &m_savedW, &m_savedH);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (!monitor) return;

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode) return;

        glfwSetWindowMonitor(m_window, monitor,
            0, 0, mode->width, mode->height, mode->refreshRate);

        std::cout << "[WINDOW] Fullscreen: " << mode->width << "x" << mode->height
            << " @ " << mode->refreshRate << "Hz" << std::endl;
    }
    else {
        glfwSetWindowMonitor(m_window, nullptr,
            m_savedX, m_savedY, m_savedW, m_savedH, 0);
        std::cout << "[WINDOW] Okno: " << m_savedW << "x" << m_savedH << std::endl;
    }

    m_fullscreen = enable;
    m_resized = true;
}