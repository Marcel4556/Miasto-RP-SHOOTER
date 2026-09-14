#include "core/Window.h"
#include <stdexcept>

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