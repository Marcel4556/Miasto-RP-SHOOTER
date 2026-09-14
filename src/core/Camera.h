#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/Input.h"
#include <GLFW/glfw3.h>

class Camera {
public:
    glm::vec3 position{ 0.0f, 1.7f, 8.0f };
    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 6.0f;
    float sensitivity = 0.1f;
    glm::vec3 front{ 0, 0, -1 };
    glm::vec3 up{ 0, 1, 0 };

    void update(float dt) {
        auto md = Input::mouseDelta();
        yaw += md.x * sensitivity;
        pitch -= md.y * sensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);

        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);

        glm::vec3 right = glm::normalize(glm::cross(front, up));
        float v = speed * dt;
        if (Input::keyDown(GLFW_KEY_W)) position += front * v;
        if (Input::keyDown(GLFW_KEY_S)) position -= front * v;
        if (Input::keyDown(GLFW_KEY_A)) position -= right * v;
        if (Input::keyDown(GLFW_KEY_D)) position += right * v;
        if (Input::keyDown(GLFW_KEY_SPACE)) position.y += v;
        if (Input::keyDown(GLFW_KEY_LEFT_SHIFT)) position.y -= v;
    }

    glm::mat4 view() const {
        return glm::lookAt(position, position + front, up);
    }
    glm::mat4 projection(float aspect) const {
        glm::mat4 p = glm::perspective(glm::radians(70.0f), aspect, 0.1f, 200.0f);
        p[1][1] *= -1.0f;
        return p;
    }
};