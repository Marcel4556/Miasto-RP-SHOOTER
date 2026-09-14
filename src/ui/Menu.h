#pragma once
#include <glm/glm.hpp>

class Camera;
class Scene;
enum class GameState;

class Menu {
public:
    void draw(GameState& state, Camera& camera, bool& wantsQuit, Scene& scene);

private:
    void drawMainMenu(GameState& state, bool& wantsQuit);
    void drawSettings(GameState& state, Camera& camera);
    void drawPauseMenu(GameState& state, bool& wantsQuit);
    void drawHUD(Scene& scene);
    void drawCrosshair();
    void drawHitMarker(float timer);

    bool m_showSettings = false;
    float m_volumeMaster = 0.8f;
    float m_volumeMusic = 0.5f;
    float m_volumeSfx = 0.7f;
    bool  m_fullscreen = false;
    int   m_resolutionIdx = 1;
};