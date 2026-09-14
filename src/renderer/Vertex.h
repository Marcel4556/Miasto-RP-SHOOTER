#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <array>
#include <cstddef>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;

    static vk::VertexInputBindingDescription binding() {
        return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    }
    static std::array<vk::VertexInputAttributeDescription, 4> attributes() {
        return {{
            { 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos) },
            { 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal) },
            { 2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) },
            { 3, 0, vk::Format::eR32G32Sfloat,    offsetof(Vertex, uv) }
        }};
    }
};