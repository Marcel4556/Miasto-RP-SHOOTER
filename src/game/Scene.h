#pragma once
#include "renderer/Mesh.h"
#include "renderer/Texture.h"
#include "renderer/VulkanContext.h"
#include "game/Weapon.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

struct GameObject {
    glm::vec3 position{ 0 };
    glm::vec3 scale{ 1 };
    glm::vec3 rotation{ 0 };
    glm::mat4 model{ 1 };
    int meshIndex = 0;
    int textureIndex = 0;

    // Walka
    bool      isEnemy = false;
    int       hp = 100;
    glm::vec3 velocity{ 0 };
    float     hitFlashTimer = 0.0f;

    void updateModel() {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 r = glm::rotate(t, glm::radians(rotation.y), { 0,1,0 });
        r = glm::rotate(r, glm::radians(rotation.x), { 1,0,0 });
        r = glm::rotate(r, glm::radians(rotation.z), { 0,0,1 });
        model = glm::scale(r, scale);
    }
};

struct Decal {
    glm::vec3 pos;
    glm::vec3 normal;
    float     age = 0.0f;
    float     scale = 0.15f;
    static constexpr float kMaxAge = 8.0f;
};

struct RayHit {
    bool      hit = false;
    float     t = 0.0f;
    glm::vec3 point{ 0 };
    glm::vec3 normal{ 0 };
    int       objectIndex = -1;
};

class Scene {
public:
    static constexpr uint32_t MAX_TEXTURES = 16;

    void init(VulkanContext& ctx);
    void destroy(VulkanContext& ctx);

    void update(float dt);

    RayHit raycast(glm::vec3 origin, glm::vec3 dir, float maxDist = 100.0f) const;
    void   spawnDecal(glm::vec3 pos, glm::vec3 normal);

    const std::vector<Mesh>& meshes()   const { return m_meshes; }
    const std::vector<Texture>& textures() const { return m_textures; }
    std::vector<GameObject>& objects() { return m_objects; }
    const std::vector<Decal>& decals()   const { return m_decals; }

    Weapon& weapon() { return m_weapon; }
    const Weapon& weapon() const { return m_weapon; }

    // === Gettery ===
    int   score()           const { return m_score; }
    int   hp()              const { return m_hp; }
    float hitMarkerTimer()  const { return m_hitMarkerTimer; }

    // === Settery (dla main.cpp) ===
    int& scoreRef() { return m_score; }
    int& hpRef() { return m_hp; }
    void  hitMarker(float time) { m_hitMarkerTimer = time; }

    bool canMoveTo(glm::vec3 pos, float radius = 0.4f) const;

private:
    void spawnEnemy(int idx);

    std::vector<Mesh>       m_meshes;
    std::vector<Texture>    m_textures;
    std::vector<GameObject> m_objects;
    std::vector<Decal>      m_decals;
    Weapon m_weapon;

    int   m_score = 0;
    int   m_hp = 100;
    float m_hitMarkerTimer = 0.0f;

    static constexpr int MESH_GROUND = 0;
    static constexpr int MESH_CUBE = 1;
    static constexpr int MESH_QUAD = 2;
};