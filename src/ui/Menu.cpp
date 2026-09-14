#include "ui/Menu.h"
#include "core/Camera.h"
#include "core/GameState.h"
#include "game/Scene.h"
#include "core/Telemetry.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <ctime>
#include <string>
#include <algorithm>

// ============================================================
//  PALETA
// ============================================================
namespace Col {
    constexpr ImU32 RED = IM_COL32(224, 16, 32, 255);
    constexpr ImU32 RED_BRIGHT = IM_COL32(255, 40, 60, 255);
    constexpr ImU32 RED_DIM = IM_COL32(160, 12, 22, 255);
    constexpr ImU32 RED_DARK = IM_COL32(80, 6, 12, 255);

    constexpr ImU32 WHITE = IM_COL32(230, 232, 236, 255);
    constexpr ImU32 WHITE_DIM = IM_COL32(180, 184, 192, 255);
    constexpr ImU32 GRAY = IM_COL32(110, 114, 122, 255);
    constexpr ImU32 GRAY_DARK = IM_COL32(60, 62, 68, 255);
    constexpr ImU32 GRAY_LINE = IM_COL32(40, 40, 44, 255);
    constexpr ImU32 BLACK = IM_COL32(0, 0, 0, 255);
    constexpr ImU32 GREEN = IM_COL32(80, 220, 100, 255);
    constexpr ImU32 YELLOW = IM_COL32(255, 200, 60, 255);
    constexpr ImU32 CYAN = IM_COL32(80, 200, 220, 255);
}

// ============================================================
//  CZCIONKI
// ============================================================
static ImFont* F_TINY = nullptr;
static ImFont* F_SMALL = nullptr;
static ImFont* F_BODY = nullptr;
static ImFont* F_LARGE = nullptr;
static ImFont* F_HUGE = nullptr;

static void loadFonts() {
    auto& fonts = ImGui::GetIO().Fonts->Fonts;
    auto get = [&](int idx) -> ImFont* {
        if (fonts.Size == 0) return nullptr;
        return (idx < (int)fonts.Size) ? fonts[idx] : fonts[0];
        };
    F_TINY = get(0);
    F_SMALL = get(1);
    F_BODY = get(2);
    F_LARGE = get(3);
    F_HUGE = get(4);

    if (!F_TINY)  F_TINY = ImGui::GetFont();
    if (!F_SMALL) F_SMALL = F_TINY;
    if (!F_BODY)  F_BODY = F_TINY;
    if (!F_LARGE) F_LARGE = F_TINY;
    if (!F_HUGE)  F_HUGE = F_TINY;
}

static Telemetry g_telemetry;

// ============================================================
//  POMOCNICZE
// ============================================================
static std::string typedText(const char* full, float t, float speed = 32.0f, float delay = 0.0f) {
    float elapsed = t - delay;
    if (elapsed < 0) return "";
    int n = (int)(elapsed * speed);
    int len = (int)strlen(full);
    if (n > len) n = len;
    return std::string(full, n);
}

static bool isGlitching(float t) {
    float cycle = std::fmod(t, 4.5f);
    return cycle > 4.30f && cycle < 4.42f;
}

static void drawBracket(ImDrawList* dl, ImVec2 corner, float size, float thick,
    int dirX, int dirY, ImU32 col)
{
    dl->AddRectFilled(corner,
        ImVec2(corner.x + size * dirX, corner.y + thick * dirY), col);
    dl->AddRectFilled(corner,
        ImVec2(corner.x + thick * dirX, corner.y + size * dirY), col);
}

// ============================================================
//  TŁO
// ============================================================
static void drawBackdrop(float t) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 sz = io.DisplaySize;

    dl->AddRectFilled(ImVec2(0, 0), sz, Col::BLACK);

    float pulse = 0.7f + 0.3f * std::sin(t * 0.9f);
    dl->AddRectFilledMultiColor(
        ImVec2(0, sz.y * 0.6f), sz,
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
        IM_COL32(80, 4, 12, (int)(100 * pulse)),
        IM_COL32(80, 4, 12, (int)(100 * pulse)));

    float horizonY = sz.y * 0.58f;
    for (int i = 0; i < 12; ++i) {
        float p = (float)i / 12.0f;
        float y = horizonY + std::pow(p, 2.5f) * (sz.y - horizonY);
        int alpha = (int)(30 * (0.3f + p * 0.7f));
        dl->AddLine(ImVec2(0, y), ImVec2(sz.x, y),
            IM_COL32(200, 20, 40, alpha), 1.0f);
    }
    for (int i = -10; i <= 10; ++i) {
        float xBottom = sz.x * 0.5f + i * (sz.x / 10.0f);
        dl->AddLine(ImVec2(sz.x * 0.5f, horizonY), ImVec2(xBottom, sz.y),
            IM_COL32(200, 20, 40, 20), 1.0f);
    }

    float scanY = std::fmod(t * 60.0f, sz.y + 200.0f) - 100.0f;
    dl->AddRectFilledMultiColor(
        ImVec2(0, scanY - 30), ImVec2(sz.x, scanY),
        IM_COL32(224, 16, 32, 0), IM_COL32(224, 16, 32, 0),
        IM_COL32(224, 16, 32, 18), IM_COL32(224, 16, 32, 18));
    dl->AddLine(ImVec2(0, scanY), ImVec2(sz.x, scanY),
        IM_COL32(224, 16, 32, 60), 1.0f);

    for (int y = 0; y < (int)sz.y; y += 5) {
        dl->AddLine(ImVec2(0, (float)y), ImVec2(sz.x, (float)y),
            IM_COL32(255, 255, 255, 3), 1.0f);
    }
}

// ============================================================
//  NAROŻNE KLAMRY HUD
// ============================================================
static void drawHudCorners(float t) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 sz = io.DisplaySize;

    float margin = 16.0f;
    float bracket = 22.0f;
    float thick = 2.0f;
    float pulse = 0.7f + 0.3f * std::sin(t * 1.5f);
    ImU32 c = IM_COL32(224, 16, 32, (int)(200 * pulse));

    drawBracket(dl, ImVec2(margin, margin), bracket, thick, 1, 1, c);
    drawBracket(dl, ImVec2(sz.x - margin, margin), bracket, thick, -1, 1, c);
    drawBracket(dl, ImVec2(margin, sz.y - margin), bracket, thick, 1, -1, c);
    drawBracket(dl, ImVec2(sz.x - margin, sz.y - margin), bracket, thick, -1, -1, c);

    float dotSpacing = 40.0f;
    for (float x = margin + bracket + 20; x < sz.x - margin - bracket - 20; x += dotSpacing) {
        dl->AddCircleFilled(ImVec2(x, margin + 1), 1.5f, Col::RED_DARK);
        dl->AddCircleFilled(ImVec2(x, sz.y - margin - 1), 1.5f, Col::RED_DARK);
    }
    for (float y = margin + bracket + 20; y < sz.y - margin - bracket - 20; y += dotSpacing) {
        dl->AddCircleFilled(ImVec2(margin + 1, y), 1.5f, Col::RED_DARK);
        dl->AddCircleFilled(ImVec2(sz.x - margin - 1, y), 1.5f, Col::RED_DARK);
    }
}

// ============================================================
//  GÓRNY PASEK
// ============================================================
static void drawTopBar(float t) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* f = F_SMALL ? F_SMALL : ImGui::GetFont();
    float fs = f->FontSize;
    float y = 34.0f;

    {
        float pulse = 0.5f + 0.5f * std::sin(t * 4.0f);
        dl->AddCircleFilled(ImVec2(44, y + fs * 0.5f), 3.5f,
            IM_COL32(224, 16, 32, (int)(150 + 100 * pulse)));
        dl->AddCircle(ImVec2(44, y + fs * 0.5f), 6.0f,
            IM_COL32(224, 16, 32, 60), 16, 1.0f);

        dl->AddText(f, fs, ImVec2(60, y), Col::RED_DIM, "SYS.CORE");
        dl->AddText(f, fs, ImVec2(60 + 80, y), Col::WHITE, "ONLINE");
        dl->AddText(f, fs, ImVec2(60 + 145, y), Col::GRAY_DARK, "|");
        dl->AddText(f, fs, ImVec2(60 + 165, y), Col::GRAY, "v0.5");
    }

    {
        const char* ses = "SESJA #48291-A";
        ImVec2 ss = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, ses);
        dl->AddText(f, fs, ImVec2(io.DisplaySize.x * 0.5f - ss.x * 0.5f, y),
            Col::GRAY_DARK, ses);
    }

    {
        std::time_t now = std::time(nullptr);
        std::tm* tm = std::localtime(&now);
        char clock[16], date[16];
        std::snprintf(clock, sizeof(clock), "%02d:%02d:%02d",
            tm->tm_hour, tm->tm_min, tm->tm_sec);
        std::snprintf(date, sizeof(date), "%04d.%02d.%02d",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

        ImVec2 cs = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, clock);
        dl->AddText(f, fs, ImVec2(io.DisplaySize.x - cs.x - 44, y),
            Col::WHITE, clock);
        ImVec2 ds = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, date);
        dl->AddText(f, fs, ImVec2(io.DisplaySize.x - ds.x - 44 - cs.x - 16, y),
            Col::GRAY, date);
    }

    dl->AddRectFilled(ImVec2(34, 56), ImVec2(io.DisplaySize.x - 34, 57), Col::GRAY_LINE);
    dl->AddRectFilled(ImVec2(34, 56), ImVec2(120, 57), Col::RED_DIM);
    dl->AddRectFilled(ImVec2(io.DisplaySize.x - 120, 56),
        ImVec2(io.DisplaySize.x - 34, 57), Col::RED_DIM);
}

// ============================================================
//  TYTUŁ
// ============================================================
static void drawTitle(float t, const char* subtitle = "TACTICAL URBAN SHOOTER") {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!F_HUGE) return;

    float cx = io.DisplaySize.x * 0.5f;
    float baseY = io.DisplaySize.y * 0.20f;
    float fontSize = F_HUGE->FontSize;

    {
        ImFont* tagF = F_TINY ? F_TINY : F_HUGE;
        float ts = tagF->FontSize * 1.15f;
        const char* tag = "TACTICAL // URBAN WARFARE";
        ImVec2 tagSz = tagF->CalcTextSizeA(ts, FLT_MAX, 0.0f, tag);
        dl->AddText(tagF, ts, ImVec2(cx - tagSz.x * 0.5f, baseY - 34),
            Col::RED_DIM, tag);
    }

    const char* title = "MIASTO RP";
    ImVec2 tsize = F_HUGE->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, title);
    ImVec2 tpos(cx - tsize.x * 0.5f, baseY);

    bool glitch = isGlitching(t);

    if (glitch) {
        float rOffset = std::sin(t * 40.0f) * 4.0f;
        float bOffset = -std::sin(t * 37.0f) * 4.0f;
        float shake = std::sin(t * 55.0f) * 2.0f;

        for (int i = 0; i < 5; ++i) {
            float sliceY = tpos.y + (i / 5.0f) * tsize.y;
            float sliceH = tsize.y / 5.0f;
            float shiftX = std::sin(t * 30.0f + i * 1.5f) * 6.0f;

            dl->PushClipRect(
                ImVec2(0, sliceY), ImVec2(io.DisplaySize.x, sliceY + sliceH), true);

            dl->AddText(F_HUGE, fontSize,
                ImVec2(tpos.x + rOffset + shiftX + shake, tpos.y),
                IM_COL32(255, 40, 40, 180), title);
            dl->AddText(F_HUGE, fontSize,
                ImVec2(tpos.x + bOffset + shiftX - shake, tpos.y),
                IM_COL32(40, 120, 255, 180), title);
            dl->AddText(F_HUGE, fontSize,
                ImVec2(tpos.x + shiftX, tpos.y),
                Col::WHITE, title);

            dl->PopClipRect();
        }
    }
    else {
        dl->AddText(F_HUGE, fontSize, ImVec2(tpos.x + 3, tpos.y + 3),
            IM_COL32(0, 0, 0, 220), title);
        dl->AddText(F_HUGE, fontSize, tpos, Col::WHITE, title);
    }

    {
        float pad = 24.0f;
        float bracketLen = tsize.y * 0.55f;
        float thick = 3.0f;
        ImU32 bcol = glitch ? Col::RED_BRIGHT : Col::RED;

        float leftX = tpos.x - pad;
        float rightX = tpos.x + tsize.x + pad;
        float topY = tpos.y + 4;
        float botY = tpos.y + tsize.y - 4;

        dl->AddRectFilled(ImVec2(leftX, topY),
            ImVec2(leftX + thick, topY + bracketLen), bcol);
        dl->AddRectFilled(ImVec2(leftX, topY),
            ImVec2(leftX + bracketLen * 0.6f, topY + thick), bcol);
        dl->AddRectFilled(ImVec2(leftX, botY - bracketLen),
            ImVec2(leftX + thick, botY), bcol);
        dl->AddRectFilled(ImVec2(leftX, botY - thick),
            ImVec2(leftX + bracketLen * 0.6f, botY), bcol);

        dl->AddRectFilled(ImVec2(rightX - thick, topY),
            ImVec2(rightX, topY + bracketLen), bcol);
        dl->AddRectFilled(ImVec2(rightX - bracketLen * 0.6f, topY),
            ImVec2(rightX, topY + thick), bcol);
        dl->AddRectFilled(ImVec2(rightX - thick, botY - bracketLen),
            ImVec2(rightX, botY), bcol);
        dl->AddRectFilled(ImVec2(rightX - bracketLen * 0.6f, botY - thick),
            ImVec2(rightX, botY), bcol);
    }

    {
        float subY = tpos.y + tsize.y + 22;
        ImFont* subF = F_LARGE ? F_LARGE : F_HUGE;
        float subSize = subF->FontSize * 0.62f;

        std::string typed = typedText(subtitle, t, 22.0f, 0.8f);
        ImVec2 subSz = subF->CalcTextSizeA(subSize, FLT_MAX, 0.0f, typed.c_str());
        dl->AddText(subF, subSize, ImVec2(cx - subSz.x * 0.5f, subY),
            Col::RED_DIM, typed.c_str());
    }

    {
        float decoY = tpos.y + tsize.y + 76;
        float totalW = 340.0f;

        dl->AddRectFilled(
            ImVec2(cx - totalW * 0.5f, decoY),
            ImVec2(cx + totalW * 0.5f, decoY + 1),
            Col::GRAY_LINE);

        float pulse = 0.5f + 0.5f * std::sin(t * 2.5f);
        dl->AddRectFilled(
            ImVec2(cx - 20, decoY), ImVec2(cx + 20, decoY + 1),
            IM_COL32(224, 16, 32, (int)(200 + 55 * pulse)));
    }
}

// ============================================================
//  PRZYCISK TAKTYCZNY
// ============================================================
static bool tacticalButton(const char* number, const char* label, const char* status,
    const char* id, float height, bool primary = false)
{
    ImGui::PushID(id);

    float width = ImGui::GetContentRegionAvail().x;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(width, height);

    ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    float cut = 10.0f;
    ImVec2 pts[5] = {
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + size.x - cut, pos.y),
        ImVec2(pos.x + size.x, pos.y + cut),
        ImVec2(pos.x + size.x, pos.y + size.y),
        ImVec2(pos.x, pos.y + size.y)
    };

    ImU32 bgCol;
    if (held)         bgCol = IM_COL32(35, 6, 10, 255);
    else if (hovered) bgCol = IM_COL32(22, 4, 8, 250);
    else              bgCol = IM_COL32(8, 8, 10, 230);
    dl->AddConvexPolyFilled(pts, 5, bgCol);

    float barW = hovered ? 5.0f : 3.0f;
    ImU32 barCol = primary || hovered ? Col::RED : Col::RED_DARK;
    dl->AddRectFilled(pos, ImVec2(pos.x + barW, pos.y + size.y), barCol);

    if (hovered) {
        for (int i = 1; i <= 3; ++i) {
            int a = 25 / i;
            dl->AddRectFilled(
                ImVec2(pos.x + barW, pos.y),
                ImVec2(pos.x + barW + i * 4, pos.y + size.y),
                IM_COL32(224, 16, 32, a));
        }
    }

    ImU32 borderCol = hovered ? Col::RED : Col::GRAY_LINE;
    dl->AddLine(ImVec2(pos.x + barW, pos.y),
        ImVec2(pos.x + size.x - cut, pos.y), borderCol, 1.0f);
    dl->AddLine(ImVec2(pos.x + size.x - cut, pos.y),
        ImVec2(pos.x + size.x, pos.y + cut),
        hovered ? Col::RED : Col::RED_DARK, 1.0f);
    dl->AddLine(ImVec2(pos.x + size.x, pos.y + cut),
        ImVec2(pos.x + size.x, pos.y + size.y), borderCol, 1.0f);
    dl->AddLine(ImVec2(pos.x + barW, pos.y + size.y),
        ImVec2(pos.x + size.x, pos.y + size.y), borderCol, 1.0f);

    ImFont* numF = F_SMALL ? F_SMALL : ImGui::GetFont();
    float numSize = numF->FontSize * 0.95f;
    ImVec2 numPos(pos.x + 18, pos.y + (size.y - numF->FontSize * 0.95f) * 0.5f);
    dl->AddText(numF, numSize, numPos,
        hovered ? Col::RED : Col::RED_DIM, number);

    float sepX = numPos.x + numF->CalcTextSizeA(numSize, FLT_MAX, 0.0f, number).x + 12;
    dl->AddLine(
        ImVec2(sepX, pos.y + size.y * 0.28f),
        ImVec2(sepX, pos.y + size.y * 0.72f),
        Col::GRAY_LINE, 1.0f);

    ImFont* textF = F_BODY ? F_BODY : ImGui::GetFont();
    float fs = textF->FontSize * (primary ? 1.10f : 1.0f);
    ImVec2 tsize = textF->CalcTextSizeA(fs, FLT_MAX, 0.0f, label);
    ImVec2 tPos(sepX + 18, pos.y + (size.y - tsize.y) * 0.5f);

    ImU32 textCol = hovered ? Col::WHITE : (primary ? Col::WHITE : Col::WHITE_DIM);
    dl->AddText(textF, fs, tPos, textCol, label);

    if (status && strlen(status) > 0) {
        ImVec2 ss = numF->CalcTextSizeA(numSize, FLT_MAX, 0.0f, status);
        ImVec2 sPos(pos.x + size.x - ss.x - 20, pos.y + (size.y - ss.y) * 0.5f);
        ImU32 statusCol = hovered ? Col::RED : Col::GRAY_DARK;
        dl->AddText(numF, numSize, sPos, statusCol, status);
    }

    if (hovered) {
        float ax = pos.x + size.x - 16;
        float ay = pos.y + size.y * 0.5f;
        float a = 5.0f;
        ImVec2 tri[3] = {
            ImVec2(ax, ay - a), ImVec2(ax + a, ay), ImVec2(ax, ay + a)
        };
        dl->AddTriangleFilled(tri[0], tri[1], tri[2], Col::RED);
    }

    ImGui::PopID();
    return clicked;
}

// ============================================================
//  TELEMETRIA PANEL
// ============================================================
static void drawTelemetry(float t) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    g_telemetry.update(ImGui::GetIO().DeltaTime);
    const auto& data = g_telemetry.data();

    float panelW = 240.0f;
    float rowH = 22.0f;
    float headerH = 26.0f;
    float pad = 12.0f;
    float panelH = headerH + rowH * 4 + pad + 14;

    float x = io.DisplaySize.x - panelW - 40;
    float y = io.DisplaySize.y - panelH - 40;

    ImFont* f = F_TINY ? F_TINY : ImGui::GetFont();
    float fs = f->FontSize;

    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + panelW, y + panelH),
        IM_COL32(8, 8, 10, 220));
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + panelW, y + 1), Col::RED);
    dl->AddRectFilled(ImVec2(x, y + panelH - 1), ImVec2(x + panelW, y + panelH),
        Col::RED_DARK);

    dl->AddText(f, fs, ImVec2(x + pad, y + 6), Col::RED_DIM, "TELEMETRIA");

    float blink = std::fmod(t * 2.0f, 1.0f);
    if (blink < 0.5f) {
        dl->AddCircleFilled(ImVec2(x + panelW - pad - 4, y + 6 + fs * 0.5f),
            3.0f, Col::RED_BRIGHT);
    }

    dl->AddRectFilled(ImVec2(x + pad, y + headerH - 4),
        ImVec2(x + panelW - pad, y + headerH - 3),
        Col::GRAY_LINE);

    auto drawRow = [&](int idx, const char* label,
        float percent, const char* valueStr, bool warning)
        {
            float ry = y + headerH + idx * rowH;
            float barX = x + pad + 42;
            float barW = panelW - pad * 2 - 42 - 52;

            dl->AddText(f, fs, ImVec2(x + pad, ry + 3), Col::GRAY, label);
            dl->AddRectFilled(ImVec2(barX, ry + 4), ImVec2(barX + barW, ry + 14),
                IM_COL32(22, 22, 24, 220));

            float p = std::clamp(percent / 100.0f, 0.0f, 1.0f);
            ImU32 fillCol = warning ? Col::RED_BRIGHT : Col::RED;
            dl->AddRectFilled(ImVec2(barX, ry + 4),
                ImVec2(barX + barW * p, ry + 14), fillCol);

            for (int s = 1; s < 10; ++s) {
                float sx = barX + barW * (s / 10.0f);
                dl->AddLine(ImVec2(sx, ry + 4), ImVec2(sx, ry + 14), Col::BLACK, 1.0f);
            }

            dl->AddRect(ImVec2(barX, ry + 4), ImVec2(barX + barW, ry + 14),
                Col::GRAY_LINE, 0, 0, 1.0f);

            ImVec2 vs = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, valueStr);
            dl->AddText(f, fs, ImVec2(x + panelW - pad - vs.x, ry + 3),
                warning ? Col::RED : Col::WHITE_DIM, valueStr);
        };

    char buf[64];

    std::snprintf(buf, sizeof(buf), "%3.0f%%", data.cpuPercent);
    drawRow(0, "CPU", data.cpuPercent, buf, data.cpuPercent > 75.0f);

    std::snprintf(buf, sizeof(buf), "%3.0f%%", data.gpuPercent);
    drawRow(1, "GPU", data.gpuPercent, buf, data.gpuPercent > 75.0f);

    std::snprintf(buf, sizeof(buf), "%4.1fG", data.ramUsedGB);
    drawRow(2, "RAM", data.ramPercent, buf, data.ramPercent > 85.0f);

    {
        float netPercent = std::min(data.netDownMBps / 10.0f * 100.0f, 100.0f);
        std::snprintf(buf, sizeof(buf), "%.1fM", data.netDownMBps);
        drawRow(3, "NET", netPercent, buf, data.netDownMBps > 5.0f);
    }
}

// ============================================================
//  HUD W GRZE
// ============================================================
static void drawInGameHUD(Scene& scene, const Camera& cam, float t) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* f = F_TINY ? F_TINY : ImGui::GetFont();
    ImFont* fSmall = F_SMALL ? F_SMALL : f;
    ImFont* fBody = F_BODY ? F_BODY : f;
    float fs = f->FontSize;

    const Weapon& w = scene.weapon();

    // ==================== LEWY DOL: HP + STAMINA ====================
    {
        float x = 60;
        float y = io.DisplaySize.y - 175;
        float w2 = 220, h2 = 105;

        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w2, y + h2),
            IM_COL32(8, 8, 10, 210));
        dl->AddRect(ImVec2(x, y), ImVec2(x + w2, y + h2),
            Col::GRAY_LINE, 0, 0, 1.0f);
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 4, y + h2), Col::RED);

        // HP
        dl->AddText(f, fs, ImVec2(x + 14, y + 6), Col::RED_DIM, "ZYCIE");

        float hpPct = scene.hp() / 100.0f;
        float barX = x + 14, barY = y + 26;
        float barW = w2 - 28, barH = 10;

        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
            IM_COL32(30, 10, 12, 255));
        ImU32 hpCol = (hpPct > 0.5f) ? Col::GREEN
            : (hpPct > 0.25f) ? Col::YELLOW : Col::RED;
        dl->AddRectFilled(ImVec2(barX, barY),
            ImVec2(barX + barW * hpPct, barY + barH), hpCol);
        for (int s = 1; s < 10; ++s) {
            float sx = barX + barW * (s / 10.0f);
            dl->AddLine(ImVec2(sx, barY), ImVec2(sx, barY + barH),
                Col::BLACK, 1.0f);
        }
        dl->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
            Col::GRAY_LINE, 0, 0, 1.0f);

        // STAMINA
        dl->AddText(f, fs, ImVec2(x + 14, y + 50), Col::RED_DIM, "STAMINA");

        float stamPct = cam.staminaPercent();
        float stamY = y + 70;
        dl->AddRectFilled(ImVec2(barX, stamY), ImVec2(barX + barW, stamY + barH),
            IM_COL32(20, 18, 10, 255));

        ImU32 stamCol;
        if (cam.sprinting)         stamCol = IM_COL32(255, 180, 60, 255);
        else if (stamPct > 0.5f)   stamCol = IM_COL32(0, 255, 255, 255);
        else if (stamPct > 0.25f)  stamCol = IM_COL32(255, 255, 0, 255);
        else                       stamCol = IM_COL32(220, 80, 60, 255);

        dl->AddRectFilled(ImVec2(barX, stamY),
            ImVec2(barX + barW * stamPct, stamY + barH), stamCol);
        for (int s = 1; s < 10; ++s) {
            float sx = barX + barW * (s / 10.0f);
            dl->AddLine(ImVec2(sx, stamY), ImVec2(sx, stamY + barH),
                Col::BLACK, 1.0f);
        }
        dl->AddRect(ImVec2(barX, stamY), ImVec2(barX + barW, stamY + barH),
            Col::GRAY_LINE, 0, 0, 1.0f);

        // Status
        if (cam.sprinting) {
            dl->AddText(f, fs, ImVec2(x + 14, y + h2 - 18),
                IM_COL32(255, 180, 60, 255), ">> SPRINT");
        }
        else if (cam.crouching) {
            dl->AddText(f, fs, ImVec2(x + 14, y + h2 - 18),
                IM_COL32(120, 200, 255, 255), "v  KUCA");
        }
        else if (stamPct < 0.2f) {
            dl->AddText(f, fs, ImVec2(x + 14, y + h2 - 18),
                IM_COL32(220, 80, 60, 255), "!  ZMECZONY");
        }
    }

    // ==================== PRAWY DOL: AMMO ====================
    {
        float w2 = 220, h2 = 76;
        float x = io.DisplaySize.x - w2 - 60;
        float y = io.DisplaySize.y - 175 + 105 + 12;

        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w2, y + h2),
            IM_COL32(8, 8, 10, 210));
        dl->AddRect(ImVec2(x, y), ImVec2(x + w2, y + h2),
            Col::GRAY_LINE, 0, 0, 1.0f);
        dl->AddRectFilled(ImVec2(x + w2 - 4, y), ImVec2(x + w2, y + h2), Col::RED);

        dl->AddText(f, fs, ImVec2(x + 12, y + 6), Col::RED_DIM, "AMUNICJA");

        char ammoBuf[16];
        std::snprintf(ammoBuf, sizeof(ammoBuf), "%02d", w.ammoInMag);
        ImU32 ammoCol = (w.ammoInMag > 0) ? Col::WHITE : Col::RED_BRIGHT;
        dl->AddText(fBody, fBody->FontSize * 1.3f,
            ImVec2(x + 12, y + 26), ammoCol, ammoBuf);

        char maxBuf[16];
        std::snprintf(maxBuf, sizeof(maxBuf), "/ %d", w.reserveAmmo);
        ImVec2 asz = fBody->CalcTextSizeA(fBody->FontSize * 1.3f, FLT_MAX, 0.0f, ammoBuf);
        dl->AddText(fSmall, fSmall->FontSize,
            ImVec2(x + 16 + asz.x, y + 36),
            Col::GRAY, maxBuf);

        int bars = 10;
        int filled = (w.magSize > 0) ? (w.ammoInMag * bars / w.magSize) : 0;
        float barW = (w2 - 24 - (bars - 1) * 3) / bars;
        float barY = y + h2 - 14;
        for (int i = 0; i < bars; ++i) {
            float bx = x + 12 + i * (barW + 3);
            ImU32 c = (i < filled) ? Col::RED : IM_COL32(40, 40, 44, 255);
            dl->AddRectFilled(ImVec2(bx, barY), ImVec2(bx + barW, barY + 6), c);
        }

        if (w.isReloading) {
            float rp = w.reloadProgress();
            dl->AddText(fSmall, fSmall->FontSize,
                ImVec2(x + 12, y + h2 + 6),
                Col::YELLOW, "PRZELADOWANIE...");
            dl->AddRectFilled(ImVec2(x, y + h2 + 22),
                ImVec2(x + w2, y + h2 + 26),
                IM_COL32(30, 30, 32, 255));
            dl->AddRectFilled(ImVec2(x, y + h2 + 22),
                ImVec2(x + w2 * rp, y + h2 + 26),
                Col::YELLOW);
        }
    }

    // ==================== LEWY GORNY: SCORE ====================
    {
        float x = 60;
        float y = 80;
        float w2 = 220, h2 = 50;

        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w2, y + h2),
            IM_COL32(8, 8, 10, 200));
        dl->AddRect(ImVec2(x, y), ImVec2(x + w2, y + h2),
            Col::GRAY_LINE, 0, 0, 1.0f);
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 4, y + h2), Col::RED);

        dl->AddText(f, fs, ImVec2(x + 14, y + 4), Col::RED_DIM, "WYNIK");

        char scoreBuf[16];
        std::snprintf(scoreBuf, sizeof(scoreBuf), "%06d", scene.score());
        dl->AddText(fBody, fBody->FontSize * 1.2f,
            ImVec2(x + 14, y + 20), Col::WHITE, scoreBuf);
    }

    // ==================== PRAWY GORNY: FPS ====================
    {
        float panelW = 100;
        float x = io.DisplaySize.x - panelW - 60;
        float y = 76;

        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + panelW, y + 26),
            IM_COL32(8, 8, 10, 180));
        dl->AddRectFilled(ImVec2(x + panelW - 2, y), ImVec2(x + panelW, y + 26), Col::RED);

        char fpsBuf[16];
        std::snprintf(fpsBuf, sizeof(fpsBuf), "%3.0f FPS", io.Framerate);
        dl->AddText(fSmall, fSmall->FontSize,
            ImVec2(x + 10, y + 5), Col::WHITE, fpsBuf);
    }

    // Muzzle flash
    if (w.muzzleFlashTimer > 0.0f) {
        float flashPct = w.muzzleFlashTimer / w.muzzleFlashDuration;
        int alpha = (int)(80 * flashPct);
        dl->AddRectFilledMultiColor(
            ImVec2(io.DisplaySize.x * 0.3f, io.DisplaySize.y),
            ImVec2(io.DisplaySize.x * 0.7f, io.DisplaySize.y - 200),
            IM_COL32(255, 180, 60, 0), IM_COL32(255, 180, 60, 0),
            IM_COL32(255, 180, 60, alpha), IM_COL32(255, 180, 60, alpha));
    }

    // Hit marker
    if (scene.hitMarkerTimer() > 0.0f) {
        float t01 = scene.hitMarkerTimer() / 0.2f;
        int alpha = (int)(255 * t01);
        ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        float len = 8.0f + (1.0f - t01) * 6.0f;
        float off = 6.0f;

        ImU32 mc = IM_COL32(255, 60, 80, alpha);
        dl->AddLine(ImVec2(c.x - off - len, c.y - off - len),
            ImVec2(c.x - off, c.y - off), mc, 2.0f);
        dl->AddLine(ImVec2(c.x + off, c.y - off),
            ImVec2(c.x + off + len, c.y - off - len), mc, 2.0f);
        dl->AddLine(ImVec2(c.x - off - len, c.y + off + len),
            ImVec2(c.x - off, c.y + off), mc, 2.0f);
        dl->AddLine(ImVec2(c.x + off, c.y + off),
            ImVec2(c.x + off + len, c.y + off + len), mc, 2.0f);
    }
}

// ============================================================
//  GLOWNE WEJSCIE
// ============================================================
void Menu::draw(GameState& state, Camera& camera, bool& wantsQuit, Scene& scene) {
    if (!F_TINY) loadFonts();
    float t = (float)ImGui::GetTime();

    switch (state) {
    case GameState::MainMenu:
        if (m_showSettings) drawSettings(state, camera);
        else                drawMainMenu(state, wantsQuit);
        break;
    case GameState::MultiplayerMenu:
        drawMultiplayerMenu(state);
        break;
    case GameState::Paused:
        if (m_showSettings) drawSettings(state, camera);
        else                drawPauseMenu(state, wantsQuit);
        break;
    case GameState::Playing:
        drawCrosshair();
        drawInGameHUD(scene, camera, t);
        drawHudCorners(t);
        break;
    default: break;
    }
}

// ============================================================
//  EKRAN TYTUŁOWY (4 przyciski)
// ============================================================
void Menu::drawMainMenu(GameState& state, bool& wantsQuit) {
    ImGuiIO& io = ImGui::GetIO();
    float t = (float)ImGui::GetTime();

    drawBackdrop(t);
    drawTitle(t, "TACTICAL URBAN SHOOTER");

    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.76f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    ImGui::Begin("##MenuBox", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 6));

    if (tacticalButton("01", "SINGLE PLAYER", "KAMPANIA", "btn_sp", 58.0f, true))
        state = GameState::Playing;
    if (tacticalButton("02", "MULTIPLAYER", "ONLINE", "btn_mp", 52.0f))
        state = GameState::MultiplayerMenu;
    if (tacticalButton("03", "USTAWIENIA", "OPCJE", "btn_settings", 46.0f))
        m_showSettings = true;
    if (tacticalButton("04", "WYJSCIE", "KONIEC", "btn_quit", 46.0f))
        wantsQuit = true;

    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    drawHudCorners(t);
    drawTopBar(t);
    drawTelemetry(t);
}

// ============================================================
//  EKRAN MULTIPLAYER
// ============================================================
void Menu::drawMultiplayerMenu(GameState& state) {
    ImGuiIO& io = ImGui::GetIO();
    float t = (float)ImGui::GetTime();

    drawBackdrop(t);
    drawTitle(t, "MULTIPLAYER // ONLINE");
    drawTopBar(t);
    drawHudCorners(t);

    // ============ LISTA SERWERÓW (fake) ============
    struct FakeServer {
        const char* name;
        const char* mode;
        const char* region;
        int         players;
        int         maxPlayers;
        int         ping;
    };
    static const FakeServer servers[6] = {
        { "MIASTO RP  #1",     "RP",         "PL",  24, 32,  18 },
        { "MIASTO RP  #2",     "RP",         "PL",  12, 32,  24 },
        { "DEATHMATCH ONLY",   "DEATHMATCH", "EU",   8, 16,  42 },
        { "POLISH FORCES",     "TEAM DM",    "PL",  16, 24,  31 },
        { "NIGHT CITY",        "RP",         "DE",   6, 32,  55 },
        { "HARDCORE SHOOTER",  "TDM",        "EU",   2, 12,  68 }
    };

    // Panel listy serwerow – wysrodkowany, wyzej
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.48f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(720, 0), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.03f, 0.92f));

    ImGui::Begin("##MPServers", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* wdl = ImGui::GetWindowDrawList();

    // Naglowek
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImVec2 hdrSize(w, 42);
        ImGui::InvisibleButton("##mphdr", hdrSize);

        wdl->AddRectFilled(p, ImVec2(p.x + 4, p.y + hdrSize.y), Col::RED);
        wdl->AddRectFilled(p, ImVec2(p.x + w, p.y + 1), Col::GRAY_LINE);
        wdl->AddRectFilled(ImVec2(p.x, p.y + hdrSize.y - 1),
            ImVec2(p.x + w, p.y + hdrSize.y), Col::GRAY_LINE);

        ImFont* fL = F_LARGE ? F_LARGE : ImGui::GetFont();
        if (fL) {
            ImVec2 ts = fL->CalcTextSizeA(fL->FontSize * 0.75f, FLT_MAX, 0.0f, "LISTA SERWEROW");
            wdl->AddText(fL, fL->FontSize * 0.75f,
                ImVec2(p.x + 20, p.y + (hdrSize.y - ts.y) * 0.5f),
                Col::WHITE, "LISTA SERWEROW");
        }

        ImFont* fS = F_SMALL ? F_SMALL : ImGui::GetFont();
        const char* refresh = "ODSWIEZ [F5]";
        ImVec2 rs = fS->CalcTextSizeA(fS->FontSize, FLT_MAX, 0.0f, refresh);
        wdl->AddText(fS, fS->FontSize,
            ImVec2(p.x + w - rs.x - 20, p.y + (hdrSize.y - rs.y) * 0.5f),
            Col::GRAY, refresh);
    }

    // Naglowki kolumn
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImVec2 rowH(0, 24);
        ImGui::InvisibleButton("##mphdrCols", ImVec2(w, rowH.y));

        ImFont* f = F_TINY ? F_TINY : ImGui::GetFont();
        float fs = f->FontSize;

        wdl->AddRectFilled(p, ImVec2(p.x + w, p.y + rowH.y),
            IM_COL32(15, 15, 18, 220));

        wdl->AddText(f, fs, ImVec2(p.x + 20, p.y + 5), Col::GRAY, "NAZWA");
        wdl->AddText(f, fs, ImVec2(p.x + 260, p.y + 5), Col::GRAY, "TRYB");
        wdl->AddText(f, fs, ImVec2(p.x + 400, p.y + 5), Col::GRAY, "REGION");
        wdl->AddText(f, fs, ImVec2(p.x + 490, p.y + 5), Col::GRAY, "GRACZE");
        wdl->AddText(f, fs, ImVec2(p.x + 590, p.y + 5), Col::GRAY, "PING");
        wdl->AddText(f, fs, ImVec2(p.x + 660, p.y + 5), Col::GRAY, "STATUS");
    }

    // Wiersze serwerow
    for (int i = 0; i < 6; ++i) {
        const auto& srv = servers[i];

        ImGui::PushID(i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImVec2 rowSize(w, 38);

        ImGui::InvisibleButton("##srv", rowSize);
        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked();

        if (clicked) m_selectedServer = i;

        bool selected = (m_selectedServer == i);

        ImU32 bg;
        if (selected)      bg = IM_COL32(35, 8, 12, 240);
        else if (hovered)  bg = IM_COL32(20, 5, 8, 230);
        else               bg = IM_COL32(10, 10, 12, 200);

        wdl->AddRectFilled(p, ImVec2(p.x + w, p.y + rowSize.y), bg);

        // Lewy pasek - tylko gdy wybrany/hover
        if (selected || hovered) {
            wdl->AddRectFilled(p, ImVec2(p.x + 3, p.y + rowSize.y),
                selected ? Col::RED : Col::RED_DARK);
        }

        // Separator dolny
        wdl->AddRectFilled(
            ImVec2(p.x, p.y + rowSize.y - 1),
            ImVec2(p.x + w, p.y + rowSize.y),
            IM_COL32(30, 30, 34, 255));

        ImFont* f = F_SMALL ? F_SMALL : ImGui::GetFont();
        float fs = f->FontSize;

        // Nazwa
        wdl->AddText(f, fs, ImVec2(p.x + 20, p.y + 11),
            selected ? Col::WHITE : Col::WHITE_DIM, srv.name);
        // Tryb
        wdl->AddText(f, fs, ImVec2(p.x + 260, p.y + 11),
            Col::GRAY, srv.mode);
        // Region
        wdl->AddText(f, fs, ImVec2(p.x + 400, p.y + 11),
            Col::GRAY_DARK, srv.region);

        // Gracze (kolor: zielony gdy luz, zolty gdy duzo)
        char players[16];
        std::snprintf(players, sizeof(players), "%d / %d", srv.players, srv.maxPlayers);
        float fill = (float)srv.players / (float)srv.maxPlayers;
        ImU32 pCol = (fill > 0.85f) ? Col::YELLOW
            : (fill > 0.6f) ? Col::WHITE_DIM
            : Col::GREEN;
        wdl->AddText(f, fs, ImVec2(p.x + 490, p.y + 11), pCol, players);

        // Ping (zielony < 30, zolty < 60, czerwony >)
        char ping[16];
        std::snprintf(ping, sizeof(ping), "%d ms", srv.ping);
        ImU32 pingCol = (srv.ping < 30) ? Col::GREEN
            : (srv.ping < 60) ? Col::YELLOW
            : Col::RED;
        wdl->AddText(f, fs, ImVec2(p.x + 590, p.y + 11), pingCol, ping);

        // Status
        const char* status = "ONLINE";
        ImU32 statCol = Col::GREEN;
        wdl->AddText(f, fs, ImVec2(p.x + 660, p.y + 11), statCol, status);

        ImGui::PopID();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // ============ PRZYCISKI AKCJI (dol) ============
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.82f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(720, 0), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    ImGui::Begin("##MPActions", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 6));

    if (tacticalButton("H", "HOSTUJ SERWER", "STWORZ", "btn_host", 52.0f, true)) {
        // TODO: state = GameState::HostGame;
        state = GameState::Playing;
    }
    if (tacticalButton("J", "DOLACZ DO SERWERA", "WYBRANY", "btn_join", 52.0f)) {
        // TODO: dolacz do wybranego m_selectedServer
        state = GameState::Playing;
    }
    if (tacticalButton("F5", "ODSWIEZ LISTE", "F5", "btn_refresh", 44.0f)) {
        // Fake refresh
    }
    if (tacticalButton("ESC", "POWROT", "MENU", "btn_back", 44.0f)) {
        state = GameState::MainMenu;
    }

    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Info o wybranym serwerze na dole
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImFont* f = F_TINY ? F_TINY : ImGui::GetFont();
        float fs = f->FontSize;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "WYBRANO: %s  |  %s  |  %s  |  %d/%d GRACZY",
            servers[m_selectedServer].name,
            servers[m_selectedServer].mode,
            servers[m_selectedServer].region,
            servers[m_selectedServer].players,
            servers[m_selectedServer].maxPlayers);

        ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, buf);
        dl->AddText(f, fs,
            ImVec2((io.DisplaySize.x - ts.x) * 0.5f, io.DisplaySize.y - 42),
            Col::GRAY_DARK, buf);
    }
}

// ============================================================
//  USTAWIENIA
// ============================================================
void Menu::drawSettings(GameState& state, Camera& camera) {
    ImGuiIO& io = ImGui::GetIO();
    float t = (float)ImGui::GetTime();

    drawBackdrop(t);

    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30, 26));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.03f, 0.96f));

    ImGui::Begin("##Settings", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_AlwaysAutoResize);

    {
        ImDrawList* wdl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImVec2 hdrSize(w, 48);
        ImGui::InvisibleButton("##shdr", hdrSize);
        wdl->AddRectFilled(p, ImVec2(p.x + 4, p.y + hdrSize.y), Col::RED);

        if (F_LARGE) {
            ImVec2 ts = F_LARGE->CalcTextSizeA(F_LARGE->FontSize, FLT_MAX, 0.0f, "USTAWIENIA");
            wdl->AddText(F_LARGE, F_LARGE->FontSize,
                ImVec2(p.x + 22, p.y + (hdrSize.y - ts.y) * 0.5f),
                Col::WHITE, "USTAWIENIA");
        }
        if (F_SMALL) {
            const char* num = "03 / OPCJE";
            ImVec2 ns = F_SMALL->CalcTextSizeA(F_SMALL->FontSize, FLT_MAX, 0.0f, num);
            wdl->AddText(F_SMALL, F_SMALL->FontSize,
                ImVec2(p.x + w - ns.x - 20, p.y + (hdrSize.y - ns.y) * 0.5f),
                Col::RED_DIM, num);
        }
        wdl->AddRectFilled(ImVec2(p.x, p.y + hdrSize.y - 1),
            ImVec2(p.x + w, p.y + hdrSize.y), Col::GRAY_LINE);
    }

    ImGui::Spacing(); ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.88f, 0.06f, 0.12f, 1.0f), "GRAFIKA");
    ImGui::Spacing();
    bool fsPrev = m_fullscreen;
    ImGui::Checkbox("Pelny ekran (F11)", &m_fullscreen);
    if (m_fullscreen != fsPrev) m_pendingFullscreenToggle = true;

    ImGui::Spacing(); ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.88f, 0.06f, 0.12f, 1.0f), "STEROWANIE");
    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##sens", &camera.sensitivity, 0.02f, 0.50f, "Czulosc myszy: %.2f");
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##speed", &camera.speed, 2.0f, 20.0f, "Predkosc ruchu: %.1f");

    ImGui::Spacing(); ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.88f, 0.06f, 0.12f, 1.0f), "STEROWANIE W GRZE");
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f), "WASD - ruch");
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f), "SPACE - skok");
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f), "LCTRL - kucanie");
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f), "LSHIFT - sprint (stamina!)");
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f), "LPM - strzal | R - przeladuj");
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f), "F11 - fullscreen | ESC - pauza");

    ImGui::Spacing(); ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.88f, 0.06f, 0.12f, 1.0f), "AUDIO");
    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##vm", &m_volumeMaster, 0.0f, 1.0f, "Glowna: %.0f%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##vmu", &m_volumeMusic, 0.0f, 1.0f, "Muzyka: %.0f%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##vsfx", &m_volumeSfx, 0.0f, 1.0f, "Efekty: %.0f%%", ImGuiSliderFlags_AlwaysClamp);

    ImGui::Spacing(); ImGui::Spacing();

    if (tacticalButton("ESC", "POWROT", "BACK", "btn_back", 54.0f))
        m_showSettings = false;

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    drawHudCorners(t);
    drawTopBar(t);
}

// ============================================================
//  PAUZA
// ============================================================
void Menu::drawPauseMenu(GameState& state, bool& wantsQuit) {
    ImGuiIO& io = ImGui::GetIO();
    float t = (float)ImGui::GetTime();

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 200));

    for (int i = 0; i < 3; ++i) {
        float y = std::fmod(t * 200.0f + i * 300.0f, io.DisplaySize.y);
        dl->AddRectFilled(ImVec2(0, y), ImVec2(io.DisplaySize.x, y + 1),
            IM_COL32(224, 16, 32, 40));
    }

    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    ImGui::Begin("##PauseBox", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_AlwaysAutoResize);

    {
        ImDrawList* wdl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImVec2 hdrSize(w, 72);
        ImGui::InvisibleButton("##phdr", hdrSize);
        wdl->AddRectFilled(p, ImVec2(p.x + w, p.y + hdrSize.y),
            IM_COL32(8, 8, 10, 240));
        wdl->AddRectFilled(p, ImVec2(p.x + 4, p.y + hdrSize.y), Col::RED);
        wdl->AddRect(p, ImVec2(p.x + w, p.y + hdrSize.y), Col::GRAY_LINE, 0, 0, 1.0f);

        if (F_LARGE) {
            const char* txt = "PAUZA";
            ImVec2 ts = F_LARGE->CalcTextSizeA(F_LARGE->FontSize, FLT_MAX, 0.0f, txt);
            wdl->AddText(F_LARGE, F_LARGE->FontSize,
                ImVec2(p.x + 22, p.y + (hdrSize.y - ts.y) * 0.5f),
                Col::WHITE, txt);
        }
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 6));

    if (tacticalButton("01", "KONTYNUUJ", "RESUME", "p_resume", 58.0f, true))
        state = GameState::Playing;
    if (tacticalButton("02", "USTAWIENIA", "OPCJE", "p_settings", 50.0f))
        m_showSettings = true;
    if (tacticalButton("03", "MENU GLOWNE", "MENU", "p_menu", 50.0f))
        state = GameState::MainMenu;
    if (tacticalButton("04", "WYJSCIE", "EXIT", "p_quit", 50.0f))
        wantsQuit = true;

    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    drawHudCorners(t);
}

void Menu::drawHUD(Scene&) {}
void Menu::drawHitMarker(float) {}

void Menu::drawCrosshair() {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    float t = (float)ImGui::GetTime();

    ImU32 col = Col::RED;
    float s = 10.0f, gap = 5.0f;

    dl->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x - gap, c.y), col, 1.5f);
    dl->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + s, c.y), col, 1.5f);
    dl->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x, c.y - gap), col, 1.5f);
    dl->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + s), col, 1.5f);

    float pulse = 0.7f + 0.3f * std::sin(t * 4.0f);
    dl->AddCircleFilled(c, 1.2f + pulse * 0.5f, IM_COL32(224, 16, 32, 255));
}