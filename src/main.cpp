#include "core/Window.h"
#include "core/Input.h"
#include "core/Camera.h"
#include "core/GameState.h"
#include "renderer/VulkanContext.h"
#include "renderer/Renderer.h"
#include "game/Scene.h"
#include "ui/Menu.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <chrono>
#include <memory>
#include <cstdio>
#include <cmath>

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    try {
        Window window(1280, 720, "Miasto RP Shooter");
        Input::init(window.handle());

        VulkanContext ctx(window);

        auto scene = std::make_unique<Scene>();
        scene->init(ctx);

        auto renderer = std::make_unique<Renderer>(window, ctx, *scene);
        Camera camera;
        Menu menu;

        GameState state = GameState::MainMenu;
        bool wantsQuit = false;

        camera.position = glm::vec3(0.0f, 1.7f, 12.0f);
        camera.yaw = -90.0f;
        camera.pitch = -8.0f;

        auto last = std::chrono::high_resolution_clock::now();
        std::cout << "[MAIN] Miasto RP Shooter gotowe\n";
        std::cout << "[MAIN] F11 - fullscreen | WASD - ruch | SPACE - skok\n";
        std::cout << "[MAIN] LSHIFT - sprint | LCTRL - kucanie | ESC - pauza\n";

        while (!window.shouldClose() && !wantsQuit) {
            window.pollEvents();

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;
            if (dt > 0.1f) dt = 0.1f;

            // === F11 - fullscreen ===
            if (Input::keyPressed(GLFW_KEY_F11)) {
                window.toggleFullscreen();
                menu.m_fullscreen = window.isFullscreen();
            }

            // === ESC - pauza ===
            if (Input::keyPressed(GLFW_KEY_ESCAPE)) {
                if (state == GameState::Playing)      state = GameState::Paused;
                else if (state == GameState::Paused)  state = GameState::Playing;
            }

            Input::setCursorMode(state == GameState::Playing);

            if (state == GameState::Playing) {
                // === Fizyka + kolizje ===
                camera.update(dt, [&](glm::vec3 p, float r) {
                    return scene->canMoveTo(p, r);
                    });

                // === SHOOT ===
                if (Input::mouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
                    if (scene->weapon().tryFire()) {
                        glm::vec3 origin = camera.position;
                        glm::vec3 dir = glm::normalize(camera.front);

                        RayHit hit = scene->raycast(origin, dir, 100.0f);
                        if (hit.hit) {
                            auto& objs = scene->objects();
                            if (hit.objectIndex >= 0 &&
                                objs[hit.objectIndex].isEnemy)
                            {
                                auto& e = objs[hit.objectIndex];
                                e.hp -= 30;
                                e.hitFlashTimer = 0.15f;
                                scene->hitMarker(0.2f);
                                scene->scoreRef() += 100;

                                if (e.hp <= 0) {
                                    e.hp = 100;
                                    scene->scoreRef() += 500;
                                }
                            }
                            else {
                                scene->spawnDecal(hit.point, hit.normal);
                            }
                        }
                    }
                }

                // === RELOAD ===
                if (Input::keyPressed(GLFW_KEY_R)) {
                    scene->weapon().startReload();
                }
            }
            else if (state == GameState::MainMenu) {
                // Auto-orbit kamery w menu
                camera.yaw += dt * 8.0f;
                camera.pitch = -6.0f;
                float cy = std::cos(glm::radians(camera.yaw));
                float sy = std::sin(glm::radians(camera.yaw));
                float cp = std::cos(glm::radians(camera.pitch));
                float sp = std::sin(glm::radians(camera.pitch));
                camera.front = glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
            }

            scene->update(dt);

            renderer->drawFrame(camera, *scene, state, [&]() {
                menu.draw(state, camera, wantsQuit, *scene);
                });

            // === Fullscreen toggle z menu ===
            if (menu.m_pendingFullscreenToggle) {
                menu.m_pendingFullscreenToggle = false;
                if (window.isFullscreen() != menu.m_fullscreen) {
                    window.setFullscreen(menu.m_fullscreen);
                }
            }

            Input::endFrame();
        }

        renderer->waitIdle();
        renderer.reset();
        scene->destroy(ctx);
        scene.reset();
    }
    catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return -1;
    }
    return 0;
}