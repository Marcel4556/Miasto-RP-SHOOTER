#pragma once
#include <cstdint>

struct TelemetryData {
    // CPU
    float cpuPercent = 0.0f;

    // GPU
    float gpuPercent = 0.0f;      // rzeczywisty load GPU (PDH)
    float vramUsedGB = 0.0f;
    float vramTotalGB = 0.0f;

    // RAM
    float ramPercent = 0.0f;
    float ramUsedGB = 0.0f;
    float ramTotalGB = 0.0f;

    // NET
    float netDownMBps = 0.0f;
    float netUpMBps = 0.0f;
};

class Telemetry {
public:
    Telemetry();
    ~Telemetry();

    void update(float dt);
    const TelemetryData& data() const { return m_data; }

private:
    TelemetryData m_data;

    // CPU – buforowanie próbek
    uint64_t m_prevIdle = 0;
    uint64_t m_prevKernel = 0;
    uint64_t m_prevUser = 0;
    bool     m_firstCpu = true;
    float    m_cpuTimer = 0.0f;

    // DXGI (VRAM)
    void* m_dxgiFactory = nullptr;
    void* m_dxgiAdapter = nullptr;

    // PDH – GPU (load)
    void* m_gpuQuery = nullptr;
    void* m_gpuCounter = nullptr;
    bool  m_gpuOk = false;
    float m_gpuTimer = 0.0f;

    // PDH – NET
    void* m_netQueryIn = nullptr;
    void* m_netQueryOut = nullptr;
    void* m_netCounterIn = nullptr;
    void* m_netCounterOut = nullptr;
    bool  m_netOk = false;
    float m_netTimer = 0.0f;
};