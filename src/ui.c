#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "cartridge.h"
#include "nes.h"
#include "cpu.h"
#include "cpu_debug.h"
#include "ppu.h"
#include "ppu_debug.h"
#include "apu.h"
#include "gui.h"
#include "controller.h"

#include "IconsCodicon.h"

#define nes (app.runtime.nes)

#define UI_FONT_SIZE 17.0f
#define UI_MONO_FONT_SIZE 15.0f
#define UI_PAGE_TITLE_FONT_SIZE 26.0f
#define UI_WELCOME_HEADLINE_FONT_SIZE 28.0f

typedef enum ThemeMode {
    THEME_DARK = 0,
    THEME_LIGHT,
} ThemeMode;

typedef struct UiFontSet {
    ImFont* ui;
    ImFont* mono;
} UiFontSet;

global UiFontSet uiFonts = {0};
global ThemeMode uiThemeMode = THEME_DARK;

static const ImWchar codicon_ranges[] = {ICON_MIN_CI, ICON_MAX_CI, 0};

internal bool UiFileExists(const char* path)
{
    FILE* file = path ? fopen(path, "rb") : NULL;
    if (!file) return false;
    fclose(file);
    return true;
}

internal const char* FindUIFontPath(void)
{
#if defined(_WIN32)
    static const char* candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        NULL,
    };
#elif defined(__APPLE__)
    static const char* candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Arial.ttf",
        NULL,
    };
#else
    static const char* candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        NULL,
    };
#endif

    for (s32 i = 0; candidates[i]; ++i) {
        if (UiFileExists(candidates[i])) return candidates[i];
    }

    return NULL;
}

internal ImFont* AddFontIfPresent(ImFontAtlas* atlas, const char* path, f32 size, const ImFontConfig* cfg,
                                  const ImWchar* ranges)
{
    if (!path || !UiFileExists(path)) return NULL;
    return ImFontAtlas_AddFontFromFileTTF(atlas, path, size, cfg, ranges);
}

internal UiFontSet LoadUIFontSet(const char* uiFontPath, const char* iconFontPath, const char* monoFontPath,
                                 const ImWchar* iconRanges)
{
    UiFontSet fonts = {0};
    ImGuiIO* io = igGetIO_Nil();

    fonts.ui = AddFontIfPresent(io->Fonts, uiFontPath, UI_FONT_SIZE, NULL, NULL);
    if (!fonts.ui) {
        ImFontConfig* defaultCfg = ImFontConfig_ImFontConfig();
        defaultCfg->SizePixels = UI_FONT_SIZE;
        fonts.ui = ImFontAtlas_AddFontDefault(io->Fonts, defaultCfg);
        ImFontConfig_destroy(defaultCfg);
    }
    io->FontDefault = fonts.ui;

    if (iconFontPath && iconRanges) {
        ImFontConfig* iconCfg = ImFontConfig_ImFontConfig();
        iconCfg->MergeMode = true;
        iconCfg->PixelSnapH = true;
        iconCfg->GlyphOffset = (ImVec2){0.0f, 4.0f};
        AddFontIfPresent(io->Fonts, iconFontPath, UI_FONT_SIZE, iconCfg, iconRanges);
        ImFontConfig_destroy(iconCfg);
    }

    fonts.mono = AddFontIfPresent(io->Fonts, monoFontPath, UI_MONO_FONT_SIZE, NULL, NULL);
    return fonts;
}

internal ImVec4 UiAccentForMode(ThemeMode mode)
{
    return mode == THEME_DARK ? (ImVec4){0.35f, 0.83f, 0.72f, 1.0f} : (ImVec4){0.04f, 0.42f, 0.35f, 1.0f};
}

internal ImVec4 ui_accent(void)
{
    return UiAccentForMode(uiThemeMode);
}

internal ImVec4 ui_text_muted(void)
{
    return igGetStyle()->Colors[ImGuiCol_TextDisabled];
}

internal ImVec4 ui_text_danger(void)
{
    return (ImVec4){0.65f, 0.20f, 0.22f, 1.0f};
}

internal ImU32 UiColorToU32(ImVec4 color)
{
    return igColorConvertFloat4ToU32(color);
}

internal f32 UiCalcButtonWidth(const char* label)
{
    ImVec2 textSize = igCalcTextSize(label, NULL, false, -1.0f);
    return textSize.x + igGetStyle()->FramePadding.x * 2.0f;
}

internal void ApplyUiTheme(ThemeMode mode)
{
    const bool dark = mode == THEME_DARK;
    ImVec4 accent = UiAccentForMode(mode);

    if (dark) {
        igStyleColorsDark(NULL);
    } else {
        igStyleColorsLight(NULL);
    }

    ImGuiStyle* style = igGetStyle();
    style->WindowPadding = (ImVec2){16.0f, 14.0f};
    style->FramePadding = (ImVec2){10.0f, 7.0f};
    style->ItemSpacing = (ImVec2){10.0f, 10.0f};
    style->ItemInnerSpacing = (ImVec2){8.0f, 6.0f};

    style->WindowRounding = 10.0f;
    style->ChildRounding = 10.0f;
    style->FrameRounding = 6.0f;
    style->PopupRounding = 8.0f;
    style->GrabRounding = 6.0f;
    style->ScrollbarRounding = 8.0f;
    style->ScrollbarSize = 12.0f;

    style->FrameBorderSize = 1.0f;
    style->WindowBorderSize = 0.0f;
    style->ChildBorderSize = 1.0f;

    ImVec4* c = style->Colors;
    c[ImGuiCol_Text] = dark ? (ImVec4){0.89f, 0.92f, 0.94f, 1.0f} : (ImVec4){0.15f, 0.20f, 0.23f, 1.0f};
    c[ImGuiCol_TextDisabled] = dark ? (ImVec4){0.59f, 0.66f, 0.70f, 1.0f} : (ImVec4){0.40f, 0.46f, 0.49f, 1.0f};
    c[ImGuiCol_WindowBg] = dark ? (ImVec4){0.075f, 0.10f, 0.12f, 1.0f} : (ImVec4){0.94f, 0.95f, 0.94f, 1.0f};
    c[ImGuiCol_ChildBg] = dark ? (ImVec4){0.105f, 0.135f, 0.155f, 1.0f} : (ImVec4){0.99f, 0.99f, 0.98f, 1.0f};
    c[ImGuiCol_PopupBg] = c[ImGuiCol_ChildBg];
    c[ImGuiCol_Border] = dark ? (ImVec4){0.22f, 0.28f, 0.31f, 0.65f} : (ImVec4){0.76f, 0.81f, 0.79f, 0.75f};
    c[ImGuiCol_BorderShadow] = (ImVec4){0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_FrameBg] = dark ? (ImVec4){0.075f, 0.105f, 0.125f, 1.0f} : (ImVec4){0.95f, 0.97f, 0.96f, 1.0f};
    c[ImGuiCol_FrameBgHovered] = dark ? (ImVec4){0.15f, 0.23f, 0.25f, 1.0f} : (ImVec4){0.88f, 0.94f, 0.92f, 1.0f};
    c[ImGuiCol_FrameBgActive] = c[ImGuiCol_FrameBgHovered];
    c[ImGuiCol_TitleBg] = c[ImGuiCol_ChildBg];
    c[ImGuiCol_TitleBgActive] = dark ? (ImVec4){0.14f, 0.25f, 0.26f, 1.0f} : (ImVec4){0.83f, 0.91f, 0.87f, 1.0f};
    c[ImGuiCol_TitleBgCollapsed] = c[ImGuiCol_TitleBg];
    c[ImGuiCol_MenuBarBg] = c[ImGuiCol_WindowBg];
    c[ImGuiCol_ScrollbarBg] = c[ImGuiCol_FrameBg];
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Button] = dark ? (ImVec4){0.17f, 0.24f, 0.27f, 1.0f} : (ImVec4){0.87f, 0.92f, 0.90f, 1.0f};
    c[ImGuiCol_ButtonHovered] = dark ? (ImVec4){0.22f, 0.34f, 0.36f, 1.0f} : (ImVec4){0.77f, 0.87f, 0.83f, 1.0f};
    c[ImGuiCol_ButtonActive] = dark ? (ImVec4){0.20f, 0.40f, 0.38f, 1.0f} : (ImVec4){0.67f, 0.81f, 0.76f, 1.0f};
    c[ImGuiCol_ScrollbarGrab] = c[ImGuiCol_Button];
    c[ImGuiCol_ScrollbarGrabHovered] = c[ImGuiCol_ButtonHovered];
    c[ImGuiCol_ScrollbarGrabActive] = c[ImGuiCol_ButtonActive];
    c[ImGuiCol_Header] = dark ? (ImVec4){0.16f, 0.32f, 0.30f, 1.0f} : (ImVec4){0.80f, 0.91f, 0.86f, 1.0f};
    c[ImGuiCol_HeaderHovered] = c[ImGuiCol_ButtonHovered];
    c[ImGuiCol_HeaderActive] = c[ImGuiCol_ButtonActive];
    c[ImGuiCol_Separator] = c[ImGuiCol_Border];
    c[ImGuiCol_SeparatorHovered] = c[ImGuiCol_ButtonHovered];
    c[ImGuiCol_SeparatorActive] = c[ImGuiCol_ButtonActive];
    c[ImGuiCol_ResizeGrip] = c[ImGuiCol_Border];
    c[ImGuiCol_ResizeGripHovered] = c[ImGuiCol_ButtonHovered];
    c[ImGuiCol_ResizeGripActive] = c[ImGuiCol_ButtonActive];
    c[ImGuiCol_InputTextCursor] = c[ImGuiCol_Text];
    c[ImGuiCol_TabHovered] = c[ImGuiCol_ButtonHovered];
    c[ImGuiCol_Tab] = c[ImGuiCol_ChildBg];
    c[ImGuiCol_TabSelected] = c[ImGuiCol_TitleBgActive];
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = c[ImGuiCol_TitleBg];
    c[ImGuiCol_TabDimmedSelected] = c[ImGuiCol_Header];
    c[ImGuiCol_TabDimmedSelectedOverline] = accent;
    c[ImGuiCol_DockingPreview] = (ImVec4){accent.x, accent.y, accent.z, 0.28f};
    c[ImGuiCol_DockingEmptyBg] = c[ImGuiCol_WindowBg];
    c[ImGuiCol_PlotLines] = accent;
    c[ImGuiCol_PlotLinesHovered] = c[ImGuiCol_ButtonHovered];
    c[ImGuiCol_PlotHistogram] = accent;
    c[ImGuiCol_PlotHistogramHovered] = c[ImGuiCol_ButtonActive];
    c[ImGuiCol_TableHeaderBg] = c[ImGuiCol_FrameBg];
    c[ImGuiCol_TableBorderStrong] = c[ImGuiCol_Border];
    c[ImGuiCol_TableBorderLight] = c[ImGuiCol_Border];
    c[ImGuiCol_TableRowBgAlt] =
        dark ? (ImVec4){0.12f, 0.15f, 0.17f, 0.35f} : (ImVec4){0.89f, 0.92f, 0.91f, 0.45f};
    c[ImGuiCol_TextLink] = accent;
    c[ImGuiCol_TextSelectedBg] = (ImVec4){0.22f, 0.65f, 0.53f, 0.35f};
    c[ImGuiCol_TreeLines] = c[ImGuiCol_Border];
    c[ImGuiCol_DragDropTarget] = accent;
    c[ImGuiCol_DragDropTargetBg] = (ImVec4){accent.x, accent.y, accent.z, 0.35f};
    c[ImGuiCol_UnsavedMarker] = accent;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_NavWindowingHighlight] = accent;
    c[ImGuiCol_NavWindowingDimBg] = (ImVec4){0.02f, 0.04f, 0.05f, dark ? 0.65f : 0.30f};
    c[ImGuiCol_ModalWindowDimBg] = (ImVec4){0.02f, 0.04f, 0.05f, dark ? 0.65f : 0.30f};
}

void SetupImGui(void)
{
    ImGuiIO* io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    uiFonts = LoadUIFontSet(FindUIFontPath(), "fonts/codicon.ttf", "fonts/JetBrainsMono-Regular.ttf", codicon_ranges);
    ApplyUiTheme(uiThemeMode);
}

internal void UiPushPrimaryButtonStyle(void)
{
    igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0.10f, 0.46f, 0.37f, 1.0f});
    igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){0.13f, 0.56f, 0.45f, 1.0f});
    igPushStyleColor_Vec4(ImGuiCol_ButtonActive, (ImVec4){0.08f, 0.38f, 0.30f, 1.0f});
    igPushStyleColor_Vec4(ImGuiCol_Text, (ImVec4){1.0f, 1.0f, 1.0f, 1.0f});
}

internal void UiPopPrimaryButtonStyle(void)
{
    igPopStyleColor(4);
}

bool ui_primary_button(const char* label, ImVec2 size)
{
    UiPushPrimaryButtonStyle();
    bool clicked = igButton(label, size);
    UiPopPrimaryButtonStyle();
    return clicked;
}

bool ui_danger_button(const char* label, ImVec2 size)
{
    igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0.65f, 0.20f, 0.22f, 1.0f});
    igPushStyleColor_Vec4(ImGuiCol_Text, (ImVec4){1.0f, 1.0f, 1.0f, 1.0f});
    bool clicked = igButton(label, size);
    igPopStyleColor(2);
    return clicked;
}

internal bool UiPushMonoFont(void)
{
    if (!uiFonts.mono) return false;
    igPushFont(uiFonts.mono, UI_MONO_FONT_SIZE);
    return true;
}

internal void UiSectionTitle(const char* label)
{
    igPushStyleColor_Vec4(ImGuiCol_Text, ui_accent());
    igSeparatorText(label);
    igPopStyleColor(1);
}

internal void UiCenteredText(const char* text, f32 fontSize, ImVec4 color)
{
    if (fontSize > 0.0f) igPushFont(NULL, fontSize);

    ImVec2 size = igCalcTextSize(text, NULL, false, -1.0f);
    f32 offsetX = (igGetContentRegionAvail().x - size.x) * 0.5f;
    if (offsetX > 0.0f) {
        igSetCursorPosX(igGetCursorPosX() + offsetX);
    }

    igTextColored(color, "%s", text);

    if (fontSize > 0.0f) igPopFont();
}

internal void DrawEmptyState(const char* headline, const char* title, const char* body)
{
    ImVec2 avail = igGetContentRegionAvail();
    f32 blockHeight = UI_WELCOME_HEADLINE_FONT_SIZE + UI_PAGE_TITLE_FONT_SIZE + UI_FONT_SIZE + 40.0f;
    f32 top = igGetCursorPosY() + MAX(0.0f, (avail.y - blockHeight) * 0.5f);

    igSetCursorPosY(top);
    UiCenteredText(headline, UI_WELCOME_HEADLINE_FONT_SIZE, ui_accent());
    igSpacing();
    UiCenteredText(title, UI_PAGE_TITLE_FONT_SIZE, igGetStyle()->Colors[ImGuiCol_Text]);
    igSpacing();
    UiCenteredText(body, 0.0f, ui_text_muted());
}


/* ------------------------------------------------------------------------- */
/* UI Sections                                                               */
/* ------------------------------------------------------------------------- */

internal void DrawTopBar(SDL_Window* win, f32 dt)
{
    igPushStyleVar_Vec2(ImGuiStyleVar_FramePadding, (ImVec2){10, 10});
    if (igBeginMainMenuBar()) {
        const char* romName = "No ROM";
        if (nes && loadedFilePath[0]) {
            const char* s = SDL_strrchr(loadedFilePath, '\\');
            if (!s) s = SDL_strrchr(loadedFilePath, '/');
            romName = s ? s + 1 : loadedFilePath;
        }

        igTextColored(ui_text_muted(), "ROM:");
        igTextColored(ui_accent(), " %s", romName);
        igSeparator();

        bool hitF5 = igIsKeyPressed_Bool(ImGuiKey_F5, false);
        bool hitF8 = igIsKeyPressed_Bool(ImGuiKey_F8, false);
        bool hitF9 = igIsKeyPressed_Bool(ImGuiKey_F9, false);
        bool hitF10 = igIsKeyPressed_Bool(ImGuiKey_F10, false);
        bool hitF11 = igIsKeyPressed_Bool(ImGuiKey_F11, true);
        bool hitF12 = igIsKeyPressed_Bool(ImGuiKey_F12, false);

        f32 windowWidth = igGetWindowWidth();
        const char* oneCycleLabel = ICON_CI_CLOCK " 1-cycle (F8)";
        const char* themeLabel = uiThemeMode == THEME_DARK ? ICON_CI_SYMBOL_COLOR " Light theme"
                                                            : ICON_CI_SYMBOL_COLOR " Dark theme";

        f32 centerWidth = 570.0f;
        f32 startX = (windowWidth - centerWidth) * 0.5f;
        if (startX > igGetCursorPosX()) {
            igSetCursorPosX(startX);
        } else {
            igSameLine(0, 0);
        }

        if (debugging) {
            if (ui_primary_button(ICON_CI_PLAY " Run (F5)", (ImVec2){0, 0}) || hitF5) {
                if (!nes) {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Run", "Load a ROM first.", win);
                } else {
                    hitRun = true;
                    debugging = false;
                    stepping = false;

                    igSetWindowFocus_Str(ICON_CI_DEVICE_DESKTOP " NES Screen");
                }
            }
        } else {
            if (igButton(ICON_CI_DEBUG_PAUSE " Pause (F5)", (ImVec2){0, 0}) || hitF5) {
                debugging = true;
                stepping = false;
            }
        }

        igSameLine(0, 5);

        if (igButton(ICON_CI_DEBUG_RESTART " Reset (F9)", (ImVec2){0, 0}) || hitF9) {
            if (nes) {
                ResetNES(nes);
                debugging = true;
            }
        }

        igSameLine(0, 5);

        if (ui_primary_button(ICON_CI_SAVE " Save (F10)", (ImVec2){0, 0}) || hitF10) {
            if (nes) {
                debugging = true;
                stepping = false;
                if (saveFilePath[0]) Save(nes, saveFilePath);
                else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Save", "Load a ROM first.", win);
            }
        }

        igSameLine(0, 5);

        bool dbgToggle = (bool)app.ui.debugToggle;
        if (hitF12) dbgToggle = !dbgToggle;

        if (dbgToggle) UiPushPrimaryButtonStyle();
        bool clickedDebug = igButton(ICON_CI_BUG " Debug (F12)", (ImVec2){0, 0});
        if (dbgToggle) UiPopPrimaryButtonStyle();
        if (clickedDebug) dbgToggle = !dbgToggle;
        app.ui.debugToggle = dbgToggle;
        debugMode = app.ui.debugToggle;

        if (debugMode) {
            igSameLine(0, 5);

            if (igButton(ICON_CI_DEBUG_STEP_OVER " Step (F11)", (ImVec2){0, 0}) || hitF11) {
                if (nes) stepping = true;
            }
        }

        char fpsText[64];
        char dtText[64];
        snprintf(fpsText, sizeof(fpsText), "FPS: %d", (s32)(1.0f / dt));
        snprintf(dtText, sizeof(dtText), "dt: %.4f", dt);

        const f32 rightSpacing = 5.0f;
        f32 rightWidth = UiCalcButtonWidth(oneCycleLabel) + UiCalcButtonWidth(fpsText) + UiCalcButtonWidth(dtText) +
                         UiCalcButtonWidth(themeLabel) + rightSpacing * 3.0f;
        f32 rightX = windowWidth - rightWidth;
        if (rightX > igGetCursorPosX()) {
            igSetCursorPosX(rightX);
        } else {
            igSameLine(0, 0);
        }

        bool oneCyc = (bool)app.ui.oneCycleToggle;
        if (hitF8) oneCyc = !oneCyc;
        if (oneCyc) UiPushPrimaryButtonStyle();
        bool clickedOneCyc = igButton(oneCycleLabel, (ImVec2){0, 0});
        if (oneCyc) UiPopPrimaryButtonStyle();
        if (clickedOneCyc) oneCyc = !oneCyc;
        app.ui.oneCycleToggle = oneCyc;
        oneCycleAtTime = app.ui.oneCycleToggle;

        igSameLine(0, rightSpacing);
        igButton(fpsText, (ImVec2){0, 0});
        igSameLine(0, rightSpacing);
        igButton(dtText, (ImVec2){0, 0});
        igSameLine(0, rightSpacing);

        if (igButton(themeLabel, (ImVec2){0, 0})) {
            uiThemeMode = uiThemeMode == THEME_DARK ? THEME_LIGHT : THEME_DARK;
            ApplyUiTheme(uiThemeMode);
        }

        igEndMainMenuBar();
    }
    igPopStyleVar(1);
}

internal void DrawLeftSidebar(f32 dt)
{
    (void)dt;
    UiSectionTitle("SYSTEM");

    if (nes) {
        if (igCollapsingHeader_TreeNodeFlags("CPU Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
            CPU* cpu = &nes->cpu;
            igTextColored(ui_text_muted(), "PC:");
            igSameLine(40, 0);
            igTextColored(ui_accent(), "$%04X", cpu->pc);
            igTextColored(ui_text_muted(), "SP:");
            igSameLine(40, 0);
            igTextColored(ui_accent(), "$%02X", cpu->sp);
            igTextColored(ui_text_muted(), "A:");
            igSameLine(40, 0);
            igTextColored(ui_accent(), "$%02X", cpu->a);
            igTextColored(ui_text_muted(), "X:");
            igSameLine(40, 0);
            igTextColored(ui_accent(), "$%02X", cpu->x);
            igTextColored(ui_text_muted(), "Y:");
            igSameLine(40, 0);
            igTextColored(ui_accent(), "$%02X", cpu->y);
            igTextColored(ui_text_muted(), "P:");
            igSameLine(40, 0);
            igTextColored(ui_accent(), "$%02X", cpu->p);
        }

        if (igCollapsingHeader_TreeNodeFlags("Flags", ImGuiTreeNodeFlags_DefaultOpen)) {
            CPU* cpu = &nes->cpu;
            igText("N V - B D I Z C");
            igText("%d %d %d %d %d %d %d %d", GetBitFlag(cpu->p, NEGATIVE_FLAG) ? 1 : 0,
                   GetBitFlag(cpu->p, OVERFLOW_FLAG) ? 1 : 0, GetBitFlag(cpu->p, EMPTY_FLAG) ? 1 : 0,
                   GetBitFlag(cpu->p, BREAK_FLAG) ? 1 : 0, GetBitFlag(cpu->p, DECIMAL_FLAG) ? 1 : 0,
                   GetBitFlag(cpu->p, INTERRUPT_FLAG) ? 1 : 0, GetBitFlag(cpu->p, ZERO_FLAG) ? 1 : 0,
                   GetBitFlag(cpu->p, CARRY_FLAG) ? 1 : 0);
        }

        if (igCollapsingHeader_TreeNodeFlags("PPU State", ImGuiTreeNodeFlags_DefaultOpen)) {
            PPU* ppu = &nes->ppu;
            igText("CTRL: %02X", ppu->control);
            igText("MASK: %02X", ppu->mask);
            igText("STAT: %02X", ppu->status);
            igText("SL:   %3d", ppu->scanline);
            igText("CYC:  %3d", ppu->cycle);
        }
    } else {
        igTextDisabled("No ROM loaded");
    }
}

internal void UpdatePatternTableTextures(Device* device, NES* nesPtr)
{
    if (!nesPtr) return;

    u32 pixels[2][128 * 128]; // 2 tables, 128x128 pixels, 4 bytes/pixel (RGBA)

    u32 palette[4] = {
        0xFF000000, // Black
        0xFF555555, // Dark gray
        0xFFAAAAAA, // Light gray
        0xFFFFFFFF  // White
    };

    for (s32 t = 0; t < 2; ++t) {
        u16 baseAddr = t * 0x1000;
        for (s32 tileY = 0; tileY < 16; ++tileY) {
            for (s32 tileX = 0; tileX < 16; ++tileX) {
                s32 tileIndex = tileY * 16 + tileX;
                u16 tileAddr = baseAddr + tileIndex * 16;

                for (s32 y = 0; y < 8; ++y) {
                    u8 plane0 = ReadPPUU8(nesPtr, tileAddr + y);
                    u8 plane1 = ReadPPUU8(nesPtr, tileAddr + y + 8);

                    for (s32 x = 0; x < 8; ++x) {
                        u8 bit0 = (plane0 >> (7 - x)) & 1;
                        u8 bit1 = (plane1 >> (7 - x)) & 1;
                        u8 colorIndex = (bit1 << 1) | bit0;

                        s32 px = tileX * 8 + x;
                        s32 py = tileY * 8 + y;
                        pixels[t][py * 128 + px] = palette[colorIndex];
                    }
                }
            }
        }

        glBindTexture(GL_TEXTURE_2D, device->patterns[t]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, pixels[t]);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

internal void UpdatePatternHoverTexture(Device* device, NES* nesPtr, s32 tableIndex, s32 tileIndex)
{
    if (!nesPtr) return;

    u32 pixels[8 * 8]; // 8x8 pixels, 4 bytes/pixel (RGBA)

    u32 palette[4] = {
        0xFF000000, // Black
        0xFF555555, // Dark gray
        0xFFAAAAAA, // Light gray
        0xFFFFFFFF  // White
    };

    u16 baseAddr = tableIndex * 0x1000;
    u16 tileAddr = baseAddr + tileIndex * 16;

    for (s32 y = 0; y < 8; ++y) {
        u8 plane0 = ReadPPUU8(nesPtr, tileAddr + y);
        u8 plane1 = ReadPPUU8(nesPtr, tileAddr + y + 8);

        for (s32 x = 0; x < 8; ++x) {
            u8 bit0 = (plane0 >> (7 - x)) & 1;
            u8 bit1 = (plane1 >> (7 - x)) & 1;
            u8 colorIndex = (bit1 << 1) | bit0;

            pixels[y * 8 + x] = palette[colorIndex];
        }
    }

    glBindTexture(GL_TEXTURE_2D, device->patternHover);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

internal void DrawNametable(Device* device, NES* nesPtr, u16 address)
{
    if (!nesPtr) return;

    u32 pixels[256 * 240];

    for (s32 tileY = 0; tileY < 30; ++tileY) {
        for (s32 tileX = 0; tileX < 32; ++tileX) {
            u16 tileIndex = tileY * 32 + tileX;
            u8 patternIndex = ReadPPUU8(nesPtr, address + tileIndex);

            u16 attributeX = tileX / 4;
            u16 attributeOffsetX = tileX % 4;

            u16 attributeY = tileY / 4;
            u16 attributeOffsetY = tileY % 4;

            u16 attributeIndex = attributeY * 8 + attributeX;
            u8 attributeByte = ReadPPUU8(nesPtr, address + 0x3C0 + attributeIndex);

            u8 lookupValue = attributeTableLookup[attributeOffsetY][attributeOffsetX];
            u8 shiftValue = (lookupValue / 4) * 2;
            u8 highColorBits = (attributeByte >> shiftValue) & 0x03;

            u16 baseAddress = 0x1000 * GetBitFlag(nesPtr->ppu.control, BACKGROUND_ADDR_FLAG);
            u16 patternAddress = baseAddress + patternIndex * 16;

            for (s32 y = 0; y < 8; ++y) {
                u8 plane0 = ReadPPUU8(nesPtr, patternAddress + y);
                u8 plane1 = ReadPPUU8(nesPtr, patternAddress + y + 8);

                for (s32 x = 0; x < 8; ++x) {
                    u8 bit0 = (plane0 >> (7 - x)) & 1;
                    u8 bit1 = (plane1 >> (7 - x)) & 1;
                    u8 colorIndex = (bit1 << 1) | bit0;

                    u8 paletteIndex = ReadPPUU8(nesPtr, 0x3F00 + highColorBits * 4 + colorIndex);
                    if (colorIndex == 0) paletteIndex = ReadPPUU8(nesPtr, 0x3F00);

                    Color c = systemPalette[paletteIndex % 64];
                    u32 color = (0xFF << 24) | (c.b << 16) | (c.g << 8) | c.r;

                    s32 px = tileX * 8 + x;
                    s32 py = tileY * 8 + y;
                    pixels[py * 256 + px] = color;
                }
            }
        }
    }

    glBindTexture(GL_TEXTURE_2D, device->nametable);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

internal void DrawAudioWaveform(s16* buffer, s32 pointCount)
{
    f32 rectHeight = 150.0f;
    ImVec2 avail = igGetContentRegionAvail();
    ImVec2 p_min = igGetCursorScreenPos();
    ImVec2 p_max = (ImVec2){p_min.x + avail.x, p_min.y + rectHeight};

    ImDrawList* draw_list = igGetWindowDrawList();
    ImDrawList_AddRectFilled(draw_list, p_min, p_max, UiColorToU32((ImVec4){0.15f, 0.17f, 0.20f, 1.0f}), 6.0f, 0);
    ImDrawList_AddRect(draw_list, p_min, p_max, UiColorToU32((ImVec4){0.25f, 0.28f, 0.32f, 1.0f}), 6.0f, 0, 1.0f);

    if (pointCount > 0) {
        ImVec2 points[1024]; // APU_BUFFER_LENGTH
        if (pointCount > 1024) pointCount = 1024;

        f32 horizontalSpacing = avail.x / (f32)pointCount;
        for (s32 i = 0; i < pointCount; i++) {
            f32 x = horizontalSpacing * i;
            // APU_AMPLIFIER_VALUE is 10000. Values can be negative, so map roughly -10000..10000 to rectHeight
            f32 normalized = (f32)buffer[i] / 10000.0f; // roughly -1.0 to 1.0
            // Clamp
            if (normalized < -1.0f) normalized = -1.0f;
            if (normalized > 1.0f) normalized = 1.0f;

            f32 y = (rectHeight * 0.5f) - (normalized * rectHeight * 0.5f);

            points[i] = (ImVec2){p_min.x + x, p_min.y + y};
        }

        ImDrawList_AddPolyline(draw_list, points, pointCount, UiColorToU32(ui_accent()), 0, 1.5f);
    }

    igDummy((ImVec2){avail.x, rectHeight}); // Advance cursor
}

internal void DrawOAMTable(NES* nesPtr)
{
    if (igBeginTable("OAMTable", 5, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg,
                     (ImVec2){0, 0}, 0)) {
        igTableSetupScrollFreeze(0, 1);
        igTableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 20.0f, 0);
        igTableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 30.0f, 0);
        igTableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 30.0f, 0);
        igTableSetupColumn("Tile", ImGuiTableColumnFlags_WidthFixed, 30.0f, 0);
        igTableSetupColumn("Attr", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
        igTableHeadersRow();

        for (s32 i = 0; i < 64; ++i) {
            u8 spriteY = ReadU8(&nesPtr->oamMemory, i * 4 + 0);
            u8 spriteIdx = ReadU8(&nesPtr->oamMemory, i * 4 + 1);
            u8 spriteAttr = ReadU8(&nesPtr->oamMemory, i * 4 + 2);
            u8 spriteX = ReadU8(&nesPtr->oamMemory, i * 4 + 3);

            igTableNextRow(ImGuiTableRowFlags_None, 0);
            igTableNextColumn();
            igTextColored(ui_text_muted(), "%d", i);
            igTableNextColumn();
            igText("%02X", spriteX);
            igTableNextColumn();
            igText("%02X", spriteY);
            igTableNextColumn();
            igTextColored(ui_accent(), "%02X", spriteIdx);
            igTableNextColumn();
            igTextColored(ui_text_muted(), "%02X", spriteAttr);
        }
        igEndTable();
    }
}

internal void DrawVideoPanel(Device* device)
{
    if (nes) {
        UiSectionTitle("Pattern Tables");
        UpdatePatternTableTextures(device, nes);

        for (int i = 0; i < 2; i++) {
            igImage((ImTextureRef_c){NULL, (ImTextureID)(intptr_t)device->patterns[i]}, (ImVec2){128, 128},
                    (ImVec2){0, 0}, (ImVec2){1, 1});

            if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                ImVec2 mousePos = igGetMousePos();
                ImVec2 itemPos = igGetItemRectMin();

                float x = mousePos.x - itemPos.x;
                float y = mousePos.y - itemPos.y;

                int tileX = (int)(x / 8.0f);
                int tileY = (int)(y / 8.0f);
                int tileIdx = tileY * 16 + tileX;
                u16 addr = i * 0x1000 + tileIdx * 16;

                igBeginTooltip();
                igText("Tile: %02X", tileIdx);
                igText("Address: $%04X", addr);

                UpdatePatternHoverTexture(device, nes, i, tileIdx);
                igImage((ImTextureRef_c){NULL, (ImTextureID)(intptr_t)device->patternHover}, (ImVec2){64, 64},
                        (ImVec2){0, 0}, (ImVec2){1, 1});

                igEndTooltip();
            }
            if (i == 0) igSameLine(0, 10);
        }

        igSpacing();

        UiSectionTitle("Nametables");
        igRadioButton_IntPtr("$2000", &app.ui.nametableOption, 0);
        igSameLine(0, 10);
        igRadioButton_IntPtr("$2400", &app.ui.nametableOption, 1);
        igSameLine(0, 10);
        igRadioButton_IntPtr("$2800", &app.ui.nametableOption, 2);
        igSameLine(0, 10);
        igRadioButton_IntPtr("$2C00", &app.ui.nametableOption, 3);

        u16 ntAddr = 0x2000 + app.ui.nametableOption * 0x400;
        DrawNametable(device, nes, ntAddr);
        igImage((ImTextureRef_c){NULL, (ImTextureID)(intptr_t)device->nametable}, (ImVec2){256, 240}, (ImVec2){0, 0},
                (ImVec2){1, 1});

        igSpacing();

        UiSectionTitle("Object Attribute Memory");
        bool usingMono = UiPushMonoFont();
        DrawOAMTable(nes);
        if (usingMono) igPopFont();
    } else {
        igTextDisabled("No ROM loaded");
    }
}

internal void DrawAudioPanel()
{
    if (nes) {
        APU* apu = &nes->apu;

        UiSectionTitle("General");
        igText("CYCLES: %lld", apu->cycles);
        igText("FRAME MODE: %02X", apu->frameMode);
        igText("SAMPLE RATE: %04X", APU_SAMPLES_PER_SECOND);
        igText("FRAME IRQ: %02X", apu->frameIRQ);
        igText("FRAME VALUE: %02X", apu->frameValue);
        igText("SAMPLE COUNTER: %02X", apu->sampleCounter);
        igText("DMC IRQ: %02X", apu->dmcIRQ);
        igText("FRAME COUNTER: %02X", apu->frameCounter);
        igText("BUFFER INDEX: %04X", apu->bufferIndex);

        igSpacing();

        UiSectionTitle("Channels");

        bool sq1 = app.ui.square1Enabled;
        bool sq2 = app.ui.square2Enabled;
        bool tri = app.ui.triangleEnabled;
        bool noi = app.ui.noiseEnabled;
        bool dmc = app.ui.dmcEnabled;

        igCheckbox("SQUARE 1", &sq1);
        igCheckbox("SQUARE 2", &sq2);
        igCheckbox("TRIANGLE", &tri);
        igCheckbox("NOISE", &noi);
        igCheckbox("DMC", &dmc);

        igSpacing();

        UiSectionTitle("Audio Filters");

        bool hp1 = app.ui.hpFilter1Enabled;
        bool hp2 = app.ui.hpFilter2Enabled;
        bool lp = app.ui.lpFilterEnabled;

        igCheckbox("High-pass filter 1 (DC offset)", &hp1);
        if (hp1) {
            igIndent(10.0f);
            igSliderInt("HP1 Freq (Hz)", &app.ui.hpFilter1Freq, 10, 500, "%d", 0);
            igUnindent(10.0f);
        }

        igCheckbox("High-pass filter 2", &hp2);
        if (hp2) {
            igIndent(10.0f);
            igSliderInt("HP2 Freq (Hz)", &app.ui.hpFilter2Freq, 10, 2000, "%d", 0);
            igUnindent(10.0f);
        }

        igCheckbox("Low-pass filter (Treble roll-off)", &lp);
        if (lp) {
            igIndent(10.0f);
            igSliderInt("LP Freq (Hz)", &app.ui.lpFilterFreq, 1000, 20000, "%d", 0);
            igUnindent(10.0f);
        }

        app.ui.hpFilter1Enabled = hp1;
        app.ui.hpFilter2Enabled = hp2;
        app.ui.lpFilterEnabled = lp;

        igSpacing();

        DrawAudioWaveform(apu->buffer, apu->bufferIndex);

        app.ui.square1Enabled = sq1;
        app.ui.square2Enabled = sq2;
        app.ui.triangleEnabled = tri;
        app.ui.noiseEnabled = noi;
        app.ui.dmcEnabled = dmc;

        apu->hpFilter1.enabled = hp1;
        apu->hpFilter1.freq = app.ui.hpFilter1Freq;
        apu->hpFilter2.enabled = hp2;
        apu->hpFilter2.freq = app.ui.hpFilter2Freq;
        apu->lpFilter.enabled = lp;
        apu->lpFilter.freq = app.ui.lpFilterFreq;

        apu->pulse1.globalEnabled = sq1;
        apu->pulse2.globalEnabled = sq2;
        apu->triangle.globalEnabled = tri;
        apu->noise.globalEnabled = noi;
        apu->dmc.globalEnabled = dmc;
    } else {
        igTextDisabled("No ROM loaded");
    }
}

internal void DrawInstructionsPanel()
{
    igInputText("Address", app.ui.instructionAddressText, sizeof(app.ui.instructionAddressText),
                ImGuiInputTextFlags_CharsHexadecimal, NULL, NULL);
    igInputText("Breakpoint", app.ui.instructionBreakpointText, sizeof(app.ui.instructionBreakpointText),
                ImGuiInputTextFlags_CharsHexadecimal, NULL, NULL);
    UiSectionTitle("Disassembly");

    if (nes) {
        CPU* cpu = &nes->cpu;
        u16 pc = cpu->pc;
        if (app.ui.instructionAddressText[0] != '\0') {
            u16 inputPc = (u16)strtol(app.ui.instructionAddressText, NULL, 16);
            if (inputPc >= 0x8000 && inputPc <= 0xFFFF) {
                pc = inputPc;
            }
        }

        if (app.ui.instructionBreakpointText[0] != '\0') {
            breakpoint = (u16)strtol(app.ui.instructionBreakpointText, NULL, 16);
        }

        bool usingMono = UiPushMonoFont();
        bool child_visible = igBeginChild_Str("DisassemblyList", (ImVec2){0, 0}, false, ImGuiWindowFlags_None);
        if (child_visible) {
            for (s32 i = 0; i < 100; ++i) {
                memset(debugBuffer, 0, sizeof(debugBuffer));

                u8 opcode = ReadCPUU8(nes, pc);
                CPUInstruction* instruction = &cpuInstructions[opcode];
                s32 col = 0;
                bool currentInstr = (pc == cpu->pc);
                bool breakpointHit = (pc == breakpoint);

                if (currentInstr) {
                    igPushStyleColor_Vec4(ImGuiCol_Text, ui_accent());
                } else if (breakpointHit) {
                    igPushStyleColor_Vec4(ImGuiCol_Text, ui_text_danger());
                }

                col += currentInstr ? (breakpointHit ? sprintf(debugBuffer, "O>%04X %02X", pc, instruction->opcode)
                                                     : sprintf(debugBuffer, "> %04X %02X", pc, instruction->opcode))
                                    : (breakpointHit ? sprintf(debugBuffer, "O %04X %02X", pc, instruction->opcode)
                                                     : sprintf(debugBuffer, "  %04X %02X", pc, instruction->opcode));

                switch (instruction->bytesCount) {
                    case 2:
                        col += sprintf(debugBuffer + col, " %02X", ReadCPUU8(nes, pc + 1));
                        break;
                    case 3:
                        col += sprintf(debugBuffer + col, " %02X %02X", ReadCPUU8(nes, pc + 1), ReadCPUU8(nes, pc + 2));
                        break;
                    default:
                        break;
                }

                memset(debugBuffer + col, ' ', 18 - col);
                col = 18;
                col += sprintf(debugBuffer + col, "%s", GetInstructionStr(instruction->mnemonic));

                switch (instruction->addressingMode) {
                    case AM_IMM:
                        col += sprintf(debugBuffer + col, " #$%02X", ReadCPUU8(nes, pc + 1));
                        break;
                    case AM_ABS: {
                        u8 lo = ReadCPUU8(nes, pc + 1);
                        u8 hi = ReadCPUU8(nes, pc + 2);
                        col += sprintf(debugBuffer + col, " $%04X", (hi << 8) | lo);
                        break;
                    }
                    case AM_ABX: {
                        u8 lo = ReadCPUU8(nes, pc + 1);
                        u8 hi = ReadCPUU8(nes, pc + 2);
                        col += sprintf(debugBuffer + col, " $%04X, X", (hi << 8) | lo);
                        break;
                    }
                    case AM_ABY: {
                        u8 lo = ReadCPUU8(nes, pc + 1);
                        u8 hi = ReadCPUU8(nes, pc + 2);
                        col += sprintf(debugBuffer + col, " $%04X, Y", (hi << 8) | lo);
                        break;
                    }
                    case AM_ZPA:
                        col += sprintf(debugBuffer + col, " $%02X", ReadCPUU8(nes, pc + 1));
                        break;
                    case AM_ZPX:
                        col += sprintf(debugBuffer + col, " $%02X, X", ReadCPUU8(nes, pc + 1));
                        break;
                    case AM_ZPY:
                        col += sprintf(debugBuffer + col, " $%02X, Y", ReadCPUU8(nes, pc + 1));
                        break;
                    case AM_IND: {
                        u8 lo = ReadCPUU8(nes, pc + 1);
                        u8 hi = ReadCPUU8(nes, pc + 2);
                        col += sprintf(debugBuffer + col, " ($%04X)", (hi << 8) | lo);
                        break;
                    }
                    case AM_IZX:
                        col += sprintf(debugBuffer + col, " ($%02X, X)", ReadCPUU8(nes, pc + 1));
                        break;
                    case AM_IZY:
                        col += sprintf(debugBuffer + col, " ($%02X), Y", ReadCPUU8(nes, pc + 1));
                        break;
                    case AM_REL: {
                        s8 address = (s8)ReadCPUU8(nes, pc + 1);
                        u16 jumpAddress = pc + instruction->bytesCount + address;
                        col += sprintf(debugBuffer + col, " $%04X", jumpAddress);
                        break;
                    }
                    default:
                        break;
                }

                if (instruction->mnemonic == CPU_RTS) {
                    col += sprintf(debugBuffer + col, " -------------");
                }

                igText("%s", debugBuffer);

                if (currentInstr || breakpointHit) {
                    igPopStyleColor(1);
                }

                pc += instruction->bytesCount;
            }
        }
        igEndChild();
        if (usingMono) igPopFont();
    } else {
        igTextDisabled("No ROM loaded");
    }
}

internal void DrawMemoryPanel()
{
    igRadioButton_IntPtr("CPU", &app.ui.memoryOption, 0);
    igSameLine(0, 10);
    igRadioButton_IntPtr("PPU", &app.ui.memoryOption, 1);
    igSameLine(0, 10);
    igRadioButton_IntPtr("OAM", &app.ui.memoryOption, 2);
    igSameLine(0, 10);
    igRadioButton_IntPtr("OAM2", &app.ui.memoryOption, 3);

    igInputText("Address", app.ui.memoryAddressText, sizeof(app.ui.memoryAddressText),
                ImGuiInputTextFlags_CharsHexadecimal, NULL, NULL);
    UiSectionTitle("Hex Dump");

    if (nes) {
        u16 baseAddress = 0x0000;
        if (app.ui.memoryAddressText[0] != '\0') {
            baseAddress = (u16)strtol(app.ui.memoryAddressText, NULL, 16);
        }

        bool usingMono = UiPushMonoFont();
        bool child_visible = igBeginChild_Str("MemoryView", (ImVec2){0, 0}, false, ImGuiWindowFlags_None);
        if (child_visible) {
            if (igBeginTable("HexDump", 33, ImGuiTableFlags_HighlightHoveredColumn | ImGuiTableFlags_PadOuterX,
                             (ImVec2){-20.0f, 0}, 0)) {
                igTableSetupColumn("ADDRESS", ImGuiTableColumnFlags_WidthFixed, 40.0f, 0);

                for (s32 j = 0; j < 16; j++) {
                    char columnName[4];
                    snprintf(columnName, sizeof(columnName), "%02X", j);
                    igTableSetupColumn(columnName, ImGuiTableColumnFlags_WidthStretch, 1.0f, 0);
                }

                for (s32 j = 0; j < 16; j++) {
                    char columnName[4];
                    snprintf(columnName, sizeof(columnName), "%01X", j);
                    igTableSetupColumn(columnName, ImGuiTableColumnFlags_WidthFixed, 4.0f, 0);
                }

                igTableHeadersRow();

                for (s32 i = 0; i < 64; ++i) {
                    igTableNextRow(ImGuiTableRowFlags_None, 0);
                    igTableNextColumn();
                    igTextColored(ui_accent(), "%04X", baseAddress + i * 16);

                    for (s32 j = 0; j < 16; ++j) {
                        igTableNextColumn();
                        u16 addr = baseAddress + i * 16 + j;
                        u8 v = 0;

                        if (app.ui.memoryOption == 0) {
                            v = PeekCPUU8(nes, addr);
                        } else if (app.ui.memoryOption == 1) {
                            v = ReadPPUU8(nes, addr);
                        } else if (app.ui.memoryOption == 2) {
                            v = ReadU8(&nes->oamMemory, addr % 256);
                        } else if (app.ui.memoryOption == 3) {
                            v = ReadU8(&nes->oamMemory2, addr % 32);
                        }

                        igText("%02X", v);
                    }

                    for (s32 j = 0; j < 16; ++j) {
                        igTableNextColumn();
                        u16 addr = baseAddress + i * 16 + j;
                        u8 v = 0;

                        if (app.ui.memoryOption == 0) {
                            v = PeekCPUU8(nes, addr);
                        } else if (app.ui.memoryOption == 1) {
                            v = ReadPPUU8(nes, addr);
                        } else if (app.ui.memoryOption == 2) {
                            v = ReadU8(&nes->oamMemory, addr % 256);
                        } else if (app.ui.memoryOption == 3) {
                            v = ReadU8(&nes->oamMemory2, addr % 32);
                        }

                        igText("%c", (v >= 32 && v < 127) ? v : '.');
                    }
                }
                igEndTable();
            }
        }
        igEndChild();
        if (usingMono) igPopFont();
    } else {
        igTextDisabled("No ROM loaded");
    }
}

internal void DrawGameScreen(Device* device)
{
    if (nes) {
        GUI* gui = &nes->gui;

        glBindTexture(GL_TEXTURE_2D, device->screen);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGBA, GL_UNSIGNED_BYTE, gui->pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        ImVec2 avail = igGetContentRegionAvail();
        f32 aspect = 256.0f / 240.0f;
        f32 drawW = avail.x;
        f32 drawH = drawW / aspect;
        if (drawH > avail.y) {
            drawH = avail.y;
            drawW = drawH * aspect;
        }
        f32 ox = (avail.x - drawW) * 0.5f;
        f32 oy = (avail.y - drawH) * 0.5f;

        f32 startX = igGetCursorPosX();
        f32 startY = igGetCursorPosY();
        igSetCursorPos((ImVec2){startX + ox, startY + oy});
        igImage((ImTextureRef_c){NULL, (ImTextureID)(intptr_t)device->screen}, (ImVec2){drawW, drawH}, (ImVec2){0, 0},
                (ImVec2){1, 1});
    } else {
        DrawEmptyState("NES Emulator", "Drop a ROM to get started",
                       "Drag a .nes or .nsave file into the window, or launch with a file path.");
    }
}


internal void DrawPalettesPanel()
{
    if (nes) {
        UiSectionTitle("Background Palettes");
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                u16 addr = 0x3F00 + i * 4 + j;
                u8 val = ReadPPUU8(nes, addr);
                Color c = systemPalette[val % 64];
                ImVec4 color = (ImVec4){c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f};

                char id[32];
                snprintf(id, sizeof(id), "##bg_%d_%d", i, j);
                igColorButton(id, color,
                              ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker |
                                  ImGuiColorEditFlags_NoDragDrop,
                              (ImVec2){30, 30});
                if (j < 3) igSameLine(0, 4);
            }
            if (i < 3) igSameLine(0, 16);
        }

        igSpacing();
        UiSectionTitle("Sprite Palettes");
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                u16 addr = 0x3F10 + i * 4 + j;
                u8 val = ReadPPUU8(nes, addr);
                Color c = systemPalette[val % 64];
                ImVec4 color = (ImVec4){c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f};

                char id[32];
                snprintf(id, sizeof(id), "##sp_%d_%d", i, j);
                igColorButton(id, color,
                              ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker |
                                  ImGuiColorEditFlags_NoDragDrop,
                              (ImVec2){30, 30});
                if (j < 3) igSameLine(0, 4);
            }
            if (i < 3) igSameLine(0, 16);
        }
    } else {
        igTextDisabled("No ROM loaded");
    }
}

internal void DrawControllerPanel()
{
    if (!nes) {
        igTextDisabled("No ROM loaded");
        return;
    }

    u8 state = nes->controllers[0].state;
    ImVec4 activeCol = ui_accent();
    ImVec4 inactiveCol = ui_text_muted();

#define GET_COL(btn) (GetBitFlag(state, btn) ? activeCol : inactiveCol)

    UiSectionTitle("Player 1 Controller");
    igSpacing();
    igSpacing();

    igColumns(3, NULL, false);

    // D-PAD
    igText("    ");
    igSameLine(0, 0);
    igTextColored(GET_COL(BUTTON_UP), " [UP] ");

    igTextColored(GET_COL(BUTTON_LEFT), "[LF]");
    igSameLine(0, 0);
    igText("      ");
    igSameLine(0, 0);
    igTextColored(GET_COL(BUTTON_RIGHT), "[RT]");

    igText("    ");
    igSameLine(0, 0);
    igTextColored(GET_COL(BUTTON_DOWN), "[DN] ");

    igNextColumn();

    // SELECT / START
    igSpacing();
    igSpacing();
    igSpacing();
    igTextColored(GET_COL(BUTTON_SELECT), "[SEL]");
    igSameLine(0, 10);
    igTextColored(GET_COL(BUTTON_START), "[STR]");

    igNextColumn();

    // B / A
    igSpacing();
    igSpacing();
    igSpacing();
    igTextColored(GET_COL(BUTTON_B), "( B )");
    igSameLine(0, 10);
    igTextColored(GET_COL(BUTTON_A), "( A )");

    igColumns(1, NULL, false);
#undef GET_COL
}

void DrawUI(SDL_Window* win, Device* device, f32 dt)
{
    DrawTopBar(win, dt);

    ImGuiViewport* viewport = igGetMainViewport();
    igDockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode, NULL);

    if (debugMode) {
        if (igBegin(ICON_CI_CHIP " SYSTEM", NULL, ImGuiWindowFlags_None)) {
            DrawLeftSidebar(dt);
        }
        igEnd();

        if (igBegin(ICON_CI_DEVICE_CAMERA_VIDEO " VIDEO", NULL, ImGuiWindowFlags_None)) {
            DrawVideoPanel(device);
        }
        igEnd();

        if (igBegin(ICON_CI_MUSIC " AUDIO", NULL, ImGuiWindowFlags_None)) {
            DrawAudioPanel();
        }
        igEnd();

        if (igBegin(ICON_CI_CODE " INSTRUCTIONS", NULL, ImGuiWindowFlags_None)) {
            DrawInstructionsPanel();
        }
        igEnd();

        if (igBegin(ICON_CI_DATABASE " MEMORY", NULL, ImGuiWindowFlags_None)) {
            DrawMemoryPanel();
        }
        igEnd();

        if (igBegin(ICON_CI_SYMBOL_COLOR " PALETTES", NULL, ImGuiWindowFlags_None)) {
            DrawPalettesPanel();
        }
        igEnd();

        if (igBegin(ICON_CI_GAME " CONTROLLER", NULL, ImGuiWindowFlags_None)) {
            DrawControllerPanel();
        }
        igEnd();
    }

    ImGuiWindowFlags screenFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (igBegin(ICON_CI_DEVICE_DESKTOP " NES Screen", NULL, screenFlags)) {
        DrawGameScreen(device);
    }
    igEnd();

    igRender();
}
