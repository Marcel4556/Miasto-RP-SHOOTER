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

        camera.position = glm::vec3(0.0f, 2.5f, 12.0f);
        camera.yaw = -90.0f;
        camera.pitch = -8.0f;

        auto last = std::chrono::high_resolution_clock::now();
        std::cout << "[MAIN] Miasto RP Shooter gotowe\n";

        while (!window.shouldClose() && !wantsQuit) {
            window.pollEvents();

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;
            if (dt > 0.1f) dt = 0.1f;

            // ESC – pauza/wznowienie
            if (Input::keyPressed(GLFW_KEY_ESCAPE)) {
                if (state == GameState::Playing)      state = GameState::Paused;
                else if (state == GameState::Paused)  state = GameState::Playing;
            }

            Input::setCursorMode(state == GameState::Playing);

            if (state == GameState::Playing) {
                // Ruch
                auto oldPos = camera.position;
                camera.update(dt);
                if (!scene->canMoveTo(camera.position))
                    camera.position = oldPos;

                // === SHOOT ===
                if (Input::mouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
                    if (scene->weapon().tryFire()) {
                        // Raycast
                        glm::vec3 origin = camera.position;
                        glm::vec3 dir = glm::normalize(camera.front);

                        RayHit hit = scene->raycast(origin, dir, 100.0f);
                        if (hit.hit) {
                            // Sprawdz czy trafilismy wroga
                            auto& objs = scene->objects();
                            if (hit.objectIndex >= 0 &&
                                objs[hit.objectIndex].isEnemy)
                            {
                                auto& e = objs[hit.objectIndex];
                                e.hp -= 30;
                                e.hitFlashTimer = 0.15f;

                                // HITMARKER
                                // (ustawiane przez scene - dodamy)

                                if (e.hp <= 0) {
                                    // Zabity – respawn
                                    // score += 100
                                    // W tej wersji: respawn w miejscu
                                    e.hp = 100;
                                    // Przesun w losowe miejsce
                                    // (uproszczone – respawn w miejscu)
                                }
                            }
                            else {
                                // Trafiona sciana – decal
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
                // Auto-orbit
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