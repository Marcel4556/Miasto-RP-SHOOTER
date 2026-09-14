#pragma once
#include <algorithm>

class Weapon {
public:
    int   magSize = 30;
    int   ammoInMag = 30;
    int   reserveAmmo = 90;

    float fireRate = 0.10f;   // sekundy między strzałami
    float reloadTime = 1.5f;

    float fireCooldown = 0.0f;
    float reloadTimer = 0.0f;
    bool  isReloading = false;

    float muzzleFlashTimer = 0.0f;
    float muzzleFlashDuration = 0.06f;
    float recoilKick = 0.0f;

    bool canShoot() const {
        return !isReloading && ammoInMag > 0 && fireCooldown <= 0.0f;
    }

    void update(float dt) {
        if (fireCooldown > 0.0f)     fireCooldown -= dt;
        if (muzzleFlashTimer > 0.0f) muzzleFlashTimer -= dt;
        if (recoilKick > 0.0f)       recoilKick = std::max(0.0f, recoilKick - dt * 6.0f);

        if (isReloading) {
            reloadTimer -= dt;
            if (reloadTimer <= 0.0f) {
                int needed = magSize - ammoInMag;
                int take = std::min(needed, reserveAmmo);
                ammoInMag += take;
                reserveAmmo -= take;
                isReloading = false;
            }
        }
    }

    bool tryFire() {
        if (!canShoot()) return false;
        ammoInMag--;
        fireCooldown = fireRate;
        muzzleFlashTimer = muzzleFlashDuration;
        recoilKick = 1.0f;
        return true;
    }

    void startReload() {
        if (isReloading) return;
        if (ammoInMag >= magSize) return;
        if (reserveAmmo <= 0) return;
        isReloading = true;
        reloadTimer = reloadTime;
    }

    float reloadProgress() const {
        if (!isReloading) return 0.0f;
        return 1.0f - (reloadTimer / reloadTime);
    }
};