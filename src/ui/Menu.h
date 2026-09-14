#pragma once
#include <glm/glm.hpp>

class Camera;
class Scene;
enum class GameState;

class Menu {
public:
    void draw(GameState& state, Camera& camera, bool& wantsQuit, Scene& scene);

    bool m_pendingFullscreenToggle = false;
    bool m_fullscreen = false;

private:
    void drawMainMenu(GameState& state, bool& wantsQuit);
    void drawMultiplayerMenu(GameState& state);
    void drawSettings(GameState& state, Camera& camera);
    void drawPauseMenu(GameState& state, bool& wantsQuit);
    void drawHUD(Scene& scene);
    void drawCrosshair();
    void drawHitMarker(float timer);

    bool m_showSettings = false;
    float m_volumeMaster = 0.8f;
    float m_volumeMusic = 0.5f;
    float m_volumeSfx = 0.7f;
    int   m_resolutionIdx = 1;

    // Wybrany serwer na liscie multiplayer
    int m_selectedServer = 0;
};