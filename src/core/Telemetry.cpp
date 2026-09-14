#include "core/Telemetry.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
#endif

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <vector>

Telemetry::Telemetry() {
#ifdef _WIN32
    // ==================== DXGI (VRAM) ====================
    IDXGIFactory4* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&factory)) && factory) {
        m_dxgiFactory = factory;

        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory->EnumAdapters1(i, adapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if (desc.DedicatedVideoMemory == 0) continue;

            ComPtr<IDXGIAdapter3> adapter3;
            if (SUCCEEDED(adapter.As(&adapter3))) {
                m_dxgiAdapter = adapter3.Detach();
                break;
            }
        }
    }

    // ==================== PDH: GPU load ====================
    {
        PDH_HQUERY query = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS) {
            PDH_HCOUNTER counter = nullptr;
            // GPU Engine – sumaryczny load wszystkich silników 3D
            PDH_STATUS st = PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter);
            if (st != ERROR_SUCCESS) {
                // Fallback: niektore systemy wymagaja innej sciezki
                st = PdhAddCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter);
            }
            if (st == ERROR_SUCCESS) {
                PdhCollectQueryData(query);   // pierwsza probka (do kosza)
                m_gpuQuery = query;
                m_gpuCounter = counter;
                m_gpuOk = true;
            }
            else {
                PdhCloseQuery(query);
            }
        }
    }

    // ==================== PDH: NET ====================
    {
        PDH_HQUERY qIn = nullptr, qOut = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &qIn) == ERROR_SUCCESS &&
            PdhOpenQueryW(nullptr, 0, &qOut) == ERROR_SUCCESS)
        {
            PDH_HCOUNTER cIn = nullptr, cOut = nullptr;
            PDH_STATUS s1 = PdhAddEnglishCounterW(qIn, L"\\Network Interface(*)\\Bytes Received/sec", 0, &cIn);
            PDH_STATUS s2 = PdhAddEnglishCounterW(qOut, L"\\Network Interface(*)\\Bytes Sent/sec", 0, &cOut);
            if (s1 != ERROR_SUCCESS) s1 = PdhAddCounterW(qIn, L"\\Network Interface(*)\\Bytes Received/sec", 0, &cIn);
            if (s2 != ERROR_SUCCESS) s2 = PdhAddCounterW(qOut, L"\\Network Interface(*)\\Bytes Sent/sec", 0, &cOut);

            if (s1 == ERROR_SUCCESS && s2 == ERROR_SUCCESS) {
                PdhCollectQueryData(qIn);
                PdhCollectQueryData(qOut);
                m_netQueryIn = qIn;
                m_netQueryOut = qOut;
                m_netCounterIn = cIn;
                m_netCounterOut = cOut;
                m_netOk = true;
            }
            else {
                if (qIn) PdhCloseQuery(qIn);
                if (qOut) PdhCloseQuery(qOut);
            }
        }
    }
#endif
}

Telemetry::~Telemetry() {
#ifdef _WIN32
    if (m_dxgiAdapter) {
        ((IDXGIAdapter3*)m_dxgiAdapter)->Release();
        m_dxgiAdapter = nullptr;
    }
    if (m_dxgiFactory) {
        ((IDXGIFactory4*)m_dxgiFactory)->Release();
        m_dxgiFactory = nullptr;
    }
    if (m_gpuQuery)  PdhCloseQuery((PDH_HQUERY)m_gpuQuery);
    if (m_netQueryIn)  PdhCloseQuery((PDH_HQUERY)m_netQueryIn);
    if (m_netQueryOut) PdhCloseQuery((PDH_HQUERY)m_netQueryOut);
#endif
}

void Telemetry::update(float dt) {
#ifdef _WIN32
    // ============================================================
    //  CPU – GetSystemTimes z buforowaniem 500ms
    // ============================================================
    {
        m_cpuTimer += dt;
        if (m_cpuTimer >= 0.5f) {
            FILETIME idleFT, kernelFT, userFT;
            if (GetSystemTimes(&idleFT, &kernelFT, &userFT)) {
                auto toU64 = [](const FILETIME& ft) -> uint64_t {
                    return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
                    };
                uint64_t idle = toU64(idleFT);
                uint64_t kernel = toU64(kernelFT);
                uint64_t user = toU64(userFT);

                if (!m_firstCpu) {
                    uint64_t dIdle = idle - m_prevIdle;
                    uint64_t dKernel = kernel - m_prevKernel;
                    uint64_t dUser = user - m_prevUser;

                    uint64_t total = dKernel + dUser;
                    if (total > 0) {
                        uint64_t busy = total - dIdle;
                        float cpu = (float)busy / (float)total * 100.0f;
                        // Srednia kroczaca (mocna) - bez padaczki
                        m_data.cpuPercent = m_data.cpuPercent * 0.4f + cpu * 0.6f;
                    }
                }

                m_prevIdle = idle;
                m_prevKernel = kernel;
                m_prevUser = user;
                m_firstCpu = false;
            }
            m_cpuTimer = 0.0f;
        }
    }

    // ============================================================
    //  RAM – GlobalMemoryStatusEx
    // ============================================================
    {
        MEMORYSTATUSEX mem{};
        mem.dwLength = sizeof(mem);
        if (GlobalMemoryStatusEx(&mem)) {
            m_data.ramPercent = (float)mem.dwMemoryLoad;
            m_data.ramTotalGB = (float)(mem.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));
            m_data.ramUsedGB = m_data.ramTotalGB -
                (float)(mem.ullAvailPhys / (1024.0 * 1024.0 * 1024.0));
        }
    }

    // ============================================================
    //  VRAM – DXGI QueryVideoMemoryInfo
    // ============================================================
    {
        if (m_dxgiAdapter) {
            DXGI_QUERY_VIDEO_MEMORY_INFO info{};
            HRESULT hr = ((IDXGIAdapter3*)m_dxgiAdapter)->QueryVideoMemoryInfo(
                0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
            if (SUCCEEDED(hr) && info.Budget > 0) {
                m_data.vramUsedGB = (float)(info.CurrentUsage / (1024.0 * 1024.0 * 1024.0));
                m_data.vramTotalGB = (float)(info.Budget / (1024.0 * 1024.0 * 1024.0));
            }
        }
    }

    // ============================================================
    //  GPU – PDH GPU Engine Utilization (suma wszystkich instancji)
    // ============================================================
    {
        m_gpuTimer += dt;
        if (m_gpuOk && m_gpuTimer >= 0.5f) {
            PDH_HQUERY q = (PDH_HQUERY)m_gpuQuery;
            if (PdhCollectQueryData(q) == ERROR_SUCCESS) {
                DWORD bufSize = 0, itemCount = 0;
                PDH_STATUS st = PdhGetFormattedCounterArrayW(
                    (PDH_HCOUNTER)m_gpuCounter, PDH_FMT_DOUBLE,
                    &bufSize, &itemCount, nullptr);

                if (st == PDH_MORE_DATA && bufSize > 0) {
                    std::vector<uint8_t> buf(bufSize);
                    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data());
                    if (PdhGetFormattedCounterArrayW(
                        (PDH_HCOUNTER)m_gpuCounter, PDH_FMT_DOUBLE,
                        &bufSize, &itemCount, items) == ERROR_SUCCESS)
                    {
                        // Sumuj tylko instancje "engtype_3D" (rzeczywiste renderowanie)
                        // albo wszystko jesli nie ma 3D
                        double total3D = 0.0, totalAll = 0.0;
                        for (DWORD i = 0; i < itemCount; ++i) {
                            if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
                                items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA)
                                continue;
                            double v = items[i].FmtValue.doubleValue;
                            totalAll += v;
                            if (items[i].szName && wcsstr(items[i].szName, L"engtype_3D"))
                                total3D += v;
                        }
                        // Jesli brak 3D to uzyj sumy wszystkich
                        double use = (total3D > 0.1) ? total3D : totalAll;
                        // Clamp do 100
                        if (use > 100.0) use = 100.0;

                        m_data.gpuPercent = m_data.gpuPercent * 0.4f + (float)use * 0.6f;
                    }
                }
            }
            m_gpuTimer = 0.0f;
        }
    }

    // ============================================================
    //  NET – PDH (suma wszystkich interfejsow)
    // ============================================================
    {
        m_netTimer += dt;
        if (m_netOk && m_netTimer >= 1.0f) {
            auto sumCounter = [](PDH_HQUERY q, PDH_HCOUNTER c) -> double {
                if (PdhCollectQueryData(q) != ERROR_SUCCESS) return 0.0;

                DWORD bufSize = 0, itemCount = 0;
                PDH_STATUS st = PdhGetFormattedCounterArrayW(
                    c, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
                if (st != PDH_MORE_DATA || bufSize == 0) return 0.0;

                std::vector<uint8_t> buf(bufSize);
                auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data());
                if (PdhGetFormattedCounterArrayW(
                    c, PDH_FMT_DOUBLE, &bufSize, &itemCount, items) != ERROR_SUCCESS)
                    return 0.0;

                double total = 0.0;
                for (DWORD i = 0; i < itemCount; ++i) {
                    if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
                        items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA)
                        continue;
                    total += items[i].FmtValue.doubleValue;
                }
                return total;   // bajty/sek
                };

            double bytesIn = sumCounter((PDH_HQUERY)m_netQueryIn, (PDH_HCOUNTER)m_netCounterIn);
            double bytesOut = sumCounter((PDH_HQUERY)m_netQueryOut, (PDH_HCOUNTER)m_netCounterOut);

            float mbpsIn = (float)(bytesIn / (1024.0 * 1024.0));
            float mbpsOut = (float)(bytesOut / (1024.0 * 1024.0));

            // Wygładzenie
            m_data.netDownMBps = m_data.netDownMBps * 0.5f + mbpsIn * 0.5f;
            m_data.netUpMBps = m_data.netUpMBps * 0.5f + mbpsOut * 0.5f;

            m_netTimer = 0.0f;
        }
    }
#else
    (void)dt;
#endif
}