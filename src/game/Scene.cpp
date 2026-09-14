#include "game/Scene.h"
#include <random>
#include <algorithm>

// ========== Tekstury ID ==========
enum TexID {
    TEX_GRASS = 0,
    TEX_BRICK,
    TEX_STONE,
    TEX_WOOD,
    TEX_LEAVES,
    TEX_METAL,
    TEX_CHECKER,
    TEX_BULLETHOLE,
    TEX_COUNT
};

// ========== Pomocnicze ==========
static bool rayAABB(glm::vec3 ro, glm::vec3 rd, glm::vec3 mn, glm::vec3 mx,
    float& tHit, glm::vec3& nrm)
{
    glm::vec3 inv = 1.0f / rd;
    glm::vec3 t0 = (mn - ro) * inv;
    glm::vec3 t1 = (mx - ro) * inv;
    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);
    float tNear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
    float tFar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
    if (tNear > tFar || tFar < 0.0f) return false;
    tHit = tNear;
    if (tNear == tmin.x)      nrm = glm::vec3(rd.x > 0 ? -1.0f : 1.0f, 0, 0);
    else if (tNear == tmin.y) nrm = glm::vec3(0, rd.y > 0 ? -1.0f : 1.0f, 0);
    else                      nrm = glm::vec3(0, 0, rd.z > 0 ? -1.0f : 1.0f);
    return true;
}

static std::mt19937& rng() {
    static std::mt19937 r(std::random_device{}());
    return r;
}
static float rndf(float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng());
}

// ========== Init ==========
void Scene::init(VulkanContext& ctx) {
    const uint32_t T = 256;

    auto upload = [&](std::vector<uint8_t> data) {
        Texture tex;
        tex.createFromPixels(ctx, T, T, data.data());
        m_textures.push_back(std::move(tex));
        };

    upload(Texture::genGrass(T));
    upload(Texture::genBricks(T));
    upload(Texture::genStone(T));
    upload(Texture::genWood(T));
    upload(Texture::genLeaves(T));
    upload(Texture::genMetal(T));
    upload(Texture::genChecker(T, 8, { 0.9f,0.9f,0.9f }, { 0.15f,0.15f,0.15f }));
    upload(Texture::genBulletHole(T));

    // Meshe: [0]=ground, [1]=cube (bialy), [2]=quad
    m_meshes.push_back(Mesh::makeGround(ctx, 40.0f, { 1,1,1 }));
    m_meshes.push_back(Mesh::makeCube(ctx, { 1,1,1 }));
    m_meshes.push_back(Mesh::makeQuad(ctx, { 1,1,1 }));

    auto addBox = [&](glm::vec3 pos, glm::vec3 scale, int mesh, int tex) -> int {
        GameObject g;
        g.meshIndex = mesh; g.textureIndex = tex;
        g.position = pos; g.scale = scale;
        g.updateModel();
        m_objects.push_back(g);
        return (int)m_objects.size() - 1;
        };

    // ---- Swiat ----
    addBox({ 0, 0, 0 }, { 1, 1, 1 }, MESH_GROUND, TEX_GRASS);

    // Budynek
    addBox({ 6, 1.5f, -3 }, { 6,    3, 0.4f }, MESH_CUBE, TEX_BRICK);
    addBox({ 3, 1.5f,  0 }, { 0.4f, 3, 6 }, MESH_CUBE, TEX_BRICK);
    addBox({ 9, 1.5f,  0 }, { 0.4f, 3, 6 }, MESH_CUBE, TEX_BRICK);
    addBox({ 6, 1.5f,  3 }, { 2.5f, 3, 0.4f }, MESH_CUBE, TEX_BRICK);
    addBox({ 8.5f,1.5f,3 }, { 1.5f, 3, 0.4f }, MESH_CUBE, TEX_BRICK);

    // Kamienie
    addBox({ -4, 0.5f, 2 }, { 1.5f, 1.0f, 1.5f }, MESH_CUBE, TEX_STONE);
    addBox({ -5, 0.5f, 4 }, { 1.5f, 1.0f, 1.5f }, MESH_CUBE, TEX_STONE);
    addBox({ -3, 1.5f, 3 }, { 1.0f, 1.0f, 1.0f }, MESH_CUBE, TEX_STONE);

    // Metalowe skrzynie
    addBox({ 5, 0.5f, -6 }, { 1.2f, 1.0f, 1.2f }, MESH_CUBE, TEX_METAL);
    addBox({ 5, 1.5f, -6 }, { 1.2f, 1.0f, 1.2f }, MESH_CUBE, TEX_METAL);

    // Drzewka
    auto addTree = [&](glm::vec3 p) {
        addBox(p + glm::vec3(0, 1.0f, 0), { 0.6f, 2.0f, 0.6f }, MESH_CUBE, TEX_WOOD);
        addBox(p + glm::vec3(0, 2.6f, 0), { 2.6f, 2.0f, 2.6f }, MESH_CUBE, TEX_LEAVES);
        };
    addTree({ -8, 0, -4 });
    addTree({ -10, 0,  6 });
    addTree({ 12, 0, -8 });
    addTree({ 15, 0,  5 });
    addTree({ 0, 0, -12 });

    // Rozsypane kamienie
    for (int i = 0; i < 8; ++i) {
        float x = -15.0f + i * 4.5f;
        float z = -12.0f + (i % 3) * 3.0f;
        addBox({ x, 0.3f, z }, { 1.0f, 0.6f, 1.0f }, MESH_CUBE, TEX_STONE);
    }

    // ---- Wrogowie ----
    for (int i = 0; i < 6; ++i) {
        GameObject g;
        g.meshIndex = MESH_CUBE;
        g.textureIndex = TEX_CHECKER;  // bedzie widoczny, ale bedzie tez mial kolor
        g.scale = { 0.8f, 1.6f, 0.8f };
        g.position = { 0, 0.8f, 0 };
        g.isEnemy = true;
        g.hp = 100;
        g.updateModel();
        m_objects.push_back(g);
        spawnEnemy((int)m_objects.size() - 1);
    }
}

// ========== Spawn wroga w losowym miejscu ==========
void Scene::spawnEnemy(int idx) {
    auto& e = m_objects[idx];
    for (int tries = 0; tries < 50; ++tries) {
        float x = rndf(-20.0f, 20.0f);
        float z = rndf(-20.0f, 20.0f);
        // Sprawdz czy nie koliduje z innymi obiektami
        glm::vec3 pos(x, 0.8f, z);
        if (canMoveTo(pos, 1.2f)) {
            e.position = pos;
            // Losowy kierunek
            float ang = rndf(0.0f, 6.28318f);
            e.velocity = glm::vec3(std::cos(ang), 0, std::sin(ang)) * 2.0f;
            e.hp = 100;
            e.hitFlashTimer = 0.0f;
            e.updateModel();
            return;
        }
    }
    // Fallback
    e.position = { 0, 0.8f, -8 };
    e.velocity = { 1.0f, 0, 0 };
    e.hp = 100;
    e.updateModel();
}

// ========== Update ==========
void Scene::update(float dt) {
    m_weapon.update(dt);

    if (m_hitMarkerTimer > 0.0f) m_hitMarkerTimer -= dt;

    for (auto& o : m_objects) {
        if (!o.isEnemy) continue;

        // Ruch
        o.position += o.velocity * dt;

        // Odbicie od granic (proste)
        if (o.position.x > 25.0f) { o.position.x = 25.0f; o.velocity.x *= -1; }
        if (o.position.x < -25.0f) { o.position.x = -25.0f; o.velocity.x *= -1; }
        if (o.position.z > 25.0f) { o.position.z = 25.0f; o.velocity.z *= -1; }
        if (o.position.z < -25.0f) { o.position.z = -25.0f; o.velocity.z *= -1; }

        // Odbicie od przeszkod (uproszczone: check + cofnij)
        glm::vec3 tryPos = o.position;
        glm::vec3 old = o.position - o.velocity * dt;
        if (!canMoveTo(tryPos, 0.8f)) {
            o.position = old;
            o.velocity = -o.velocity;
        }

        // Rotacja (obrot wokol Y dla efektu)
        o.rotation.y += 60.0f * dt;

        // Hit flash
        if (o.hitFlashTimer > 0.0f) o.hitFlashTimer -= dt;

        o.updateModel();
    }

    // Decals - usun stare
    for (auto& d : m_decals) d.age += dt;
    m_decals.erase(
        std::remove_if(m_decals.begin(), m_decals.end(),
            [](const Decal& d) { return d.age >= Decal::kMaxAge; }),
        m_decals.end());
}

// ========== Raycast ==========
RayHit Scene::raycast(glm::vec3 origin, glm::vec3 dir, float maxDist) const {
    RayHit best;
    best.t = maxDist;

    for (size_t i = 0; i < m_objects.size(); ++i) {
        const auto& o = m_objects[i];
        if (o.meshIndex == MESH_GROUND) continue;

        glm::vec3 half = o.scale * 0.5f;
        glm::vec3 mn = o.position - half;
        glm::vec3 mx = o.position + half;

        float tHit;
        glm::vec3 nrm;
        if (rayAABB(origin, dir, mn, mx, tHit, nrm)) {
            if (tHit < best.t && tHit > 0.0f) {
                best.hit = true;
                best.t = tHit;
                best.point = origin + dir * tHit;
                best.normal = nrm;
                best.objectIndex = (int)i;
            }
        }
    }

    // Podloga - duza plaszczyzna
    if (std::abs(dir.y) > 1e-6f) {
        float t = -origin.y / dir.y;
        if (t > 0.0f && t < best.t) {
            best.hit = true;
            best.t = t;
            best.point = origin + dir * t;
            best.normal = { 0, 1, 0 };
            best.objectIndex = -1; // podloga
        }
    }

    if (!best.hit) best.t = 0.0f;
    return best;
}

void Scene::spawnDecal(glm::vec3 pos, glm::vec3 normal) {
    Decal d;
    d.pos = pos + normal * 0.02f;
    d.normal = normal;
    d.age = 0.0f;
    d.scale = 0.15f;
    m_decals.push_back(d);
}

// ========== Kolizje ==========
bool Scene::canMoveTo(glm::vec3 pos, float radius) const {
    for (const auto& o : m_objects) {
        if (o.meshIndex == MESH_GROUND) continue;
        if (o.isEnemy) continue;
        glm::vec3 half = o.scale * 0.5f;
        glm::vec3 mn = o.position - half;
        glm::vec3 mx = o.position + half;
        glm::vec3 closest = glm::clamp(pos, mn, mx);
        if (glm::dot(pos - closest, pos - closest) < radius * radius) return false;
    }
    return true;
}

// ========== Destroy ==========
void Scene::destroy(VulkanContext& ctx) {
    for (auto& m : m_meshes) m.destroy(ctx);
    for (auto& t : m_textures) t.destroy(ctx);
    m_meshes.clear();
    m_textures.clear();
    m_objects.clear();
    m_decals.clear();
}