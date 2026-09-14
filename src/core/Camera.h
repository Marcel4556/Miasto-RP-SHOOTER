#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/Input.h"
#include <GLFW/glfw3.h>
#include <functional>
#include <algorithm>
#include <cmath>

class Camera {
public:
    // ============ Pozycja i rotacja ============
    glm::vec3 position{ 0.0f, 1.7f, 8.0f };
    float yaw = -90.0f;
    float pitch = 0.0f;
    glm::vec3 front{ 0, 0, -1 };
    glm::vec3 up{ 0, 1, 0 };

    // ============ Sterowanie ============
    float sensitivity = 0.1f;
    float speed = 6.0f;      // bazowa predkosc (slider w menu)
    float jumpSpeed = 7.0f;      // sila skoku (m/s)
    float gravity = -22.0f;    // przyspieszenie grawitacyjne (m/s^2)

    // ============ Gracz ============
    float vy = 0.0f;
    bool  onGround = true;
    bool  crouching = false;
    bool  sprinting = false;

    float standEye = 1.7f;      // wzrok stojaco
    float crouchEye = 0.9f;      // wzrok kucajac
    float eyeHeight = 1.7f;      // aktualna (lerp)
    float playerRadius = 0.35f;

    // ============ Stamina ============
    float stamina = 100.0f;
    float staminaMax = 100.0f;
    float staminaDrain = 20.0f;   // /s podczas sprintu
    float staminaRegen = 25.0f;   // /s podczas regeneracji
    float staminaRegenDelay = 0.0f;   // opoznienie przed regeneracja

    // ============ Update (fizyka + kolizje) ============
    void update(float dt, const std::function<bool(glm::vec3, float)>& canMoveTo) {
        // --- Mysz ---
        auto md = Input::mouseDelta();
        yaw += md.x * sensitivity;
        pitch -= md.y * sensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);

        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);

        // --- Crouch ---
        crouching = Input::keyDown(GLFW_KEY_LEFT_CONTROL);
        float targetEye = crouching ? crouchEye : standEye;
        float eyeLerp = std::min(1.0f, dt * 12.0f);
        eyeHeight += (targetEye - eyeHeight) * eyeLerp;

        // --- Sprint / stamina ---
        bool wantsSprint = Input::keyDown(GLFW_KEY_LEFT_SHIFT) && !crouching;

        if (wantsSprint && stamina > 0.0f && staminaRegenDelay <= 0.0f) {
            sprinting = true;
            stamina -= staminaDrain * dt;
            if (stamina <= 0.0f) {
                stamina = 0.0f;
                sprinting = false;
                staminaRegenDelay = 0.5f;   // musisz odpoczac
            }
        }
        else {
            sprinting = false;
        }

        // Regen
        if (!sprinting) {
            if (staminaRegenDelay > 0.0f) {
                staminaRegenDelay -= dt;
            }
            else {
                stamina += staminaRegen * dt;
                if (stamina > staminaMax) stamina = staminaMax;
            }
        }

        // --- Predkosc pozioma ---
        float baseSpeed = speed;
        if (crouching)      baseSpeed = speed * 0.40f;
        else if (sprinting) baseSpeed = speed * 1.65f;

        // --- Ruch poziomy (WASD) ---
        glm::vec3 right = glm::normalize(glm::cross(front, up));
        glm::vec3 moveDir{ 0.0f };
        if (Input::keyDown(GLFW_KEY_W)) moveDir += front;
        if (Input::keyDown(GLFW_KEY_S)) moveDir -= front;
        if (Input::keyDown(GLFW_KEY_A)) moveDir -= right;
        if (Input::keyDown(GLFW_KEY_D)) moveDir += right;

        // Wylacz pionowa skladowa – chodzimy po plaszczyznie
        moveDir.y = 0.0f;
        if (glm::length(moveDir) > 0.001f)
            moveDir = glm::normalize(moveDir);

        glm::vec3 horizMove = moveDir * baseSpeed * dt;

        // --- Kolizja pozioma (X i Z osobno - sliding) ---
        {
            glm::vec3 p = position;
            p.x += horizMove.x;
            if (canMoveTo(p, playerRadius))
                position.x = p.x;
        }
        {
            glm::vec3 p = position;
            p.z += horizMove.z;
            if (canMoveTo(p, playerRadius))
                position.z = p.z;
        }

        // --- Grawitacja ---
        vy += gravity * dt;

        // --- Skok (tylko z ziemi) ---
        if (onGround && Input::keyDown(GLFW_KEY_SPACE)) {
            vy = jumpSpeed;
            onGround = false;
        }

        // --- Ruch pionowy ---
        position.y += vy * dt;

        // --- Podloga (plaska, y = eyeHeight) ---
        float minY = eyeHeight;
        if (position.y <= minY) {
            position.y = minY;
            vy = 0.0f;
            onGround = true;
        }
        else {
            onGround = false;
        }
    }

    // ============ Macierze ============
    glm::mat4 view() const {
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 projection(float aspect) const {
        glm::mat4 p = glm::perspective(glm::radians(70.0f), aspect, 0.1f, 200.0f);
        p[1][1] *= -1.0f;
        return p;
    }

    // ============ Stamina helper ============
    float staminaPercent() const { return stamina / staminaMax; }
};