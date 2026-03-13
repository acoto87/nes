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

#define nes (app.runtime.nes)

void SetupImGuiStyle()
{
    ImGuiStyle* style = igGetStyle();
    ImVec4* colors = style->Colors;

    colors[ImGuiCol_Text] = (ImVec4){0.78f, 0.78f, 0.78f, 1.00f};
    colors[ImGuiCol_TextDisabled] = (ImVec4){0.40f, 0.40f, 0.40f, 1.00f};
    colors[ImGuiCol_WindowBg] = (ImVec4){0.05f, 0.05f, 0.05f, 1.00f};
    colors[ImGuiCol_ChildBg] = (ImVec4){0.08f, 0.08f, 0.08f, 1.00f};
    colors[ImGuiCol_PopupBg] = (ImVec4){0.08f, 0.08f, 0.08f, 0.94f};
    colors[ImGuiCol_Border] = (ImVec4){0.13f, 0.13f, 0.13f, 1.00f};
    colors[ImGuiCol_BorderShadow] = (ImVec4){0.00f, 0.00f, 0.00f, 0.00f};
    colors[ImGuiCol_FrameBg] = (ImVec4){0.10f, 0.10f, 0.10f, 1.00f};
    colors[ImGuiCol_FrameBgHovered] = (ImVec4){0.14f, 0.14f, 0.14f, 1.00f};
    colors[ImGuiCol_FrameBgActive] = (ImVec4){0.20f, 1.00f, 0.40f, 0.30f};
    colors[ImGuiCol_TitleBg] = (ImVec4){0.07f, 0.07f, 0.07f, 1.00f};
    colors[ImGuiCol_TitleBgActive] = (ImVec4){0.10f, 0.10f, 0.10f, 1.00f};
    colors[ImGuiCol_TitleBgCollapsed] = (ImVec4){0.05f, 0.05f, 0.05f, 1.00f};
    colors[ImGuiCol_MenuBarBg] = (ImVec4){0.07f, 0.07f, 0.07f, 1.00f};
    colors[ImGuiCol_ScrollbarBg] = (ImVec4){0.05f, 0.05f, 0.05f, 1.00f};
    colors[ImGuiCol_ScrollbarGrab] = (ImVec4){0.13f, 0.13f, 0.13f, 1.00f};
    colors[ImGuiCol_ScrollbarGrabHovered] = (ImVec4){0.16f, 0.16f, 0.16f, 1.00f};
    colors[ImGuiCol_ScrollbarGrabActive] = (ImVec4){0.20f, 1.00f, 0.40f, 1.00f};
    colors[ImGuiCol_CheckMark] = (ImVec4){0.20f, 1.00f, 0.40f, 1.00f};
    colors[ImGuiCol_SliderGrab] = (ImVec4){0.13f, 0.13f, 0.13f, 1.00f};
    colors[ImGuiCol_SliderGrabActive] = (ImVec4){0.20f, 1.00f, 0.40f, 1.00f};
    colors[ImGuiCol_Button] = (ImVec4){0.10f, 0.10f, 0.10f, 1.00f};
    colors[ImGuiCol_ButtonHovered] = (ImVec4){0.14f, 0.14f, 0.14f, 1.00f};
    colors[ImGuiCol_ButtonActive] = (ImVec4){0.20f, 1.00f, 0.40f, 1.00f};
    colors[ImGuiCol_Header] = (ImVec4){0.14f, 0.14f, 0.14f, 1.00f};
    colors[ImGuiCol_HeaderHovered] = (ImVec4){0.18f, 0.18f, 0.18f, 1.00f};
    colors[ImGuiCol_HeaderActive] = (ImVec4){0.20f, 1.00f, 0.40f, 0.40f};
    colors[ImGuiCol_Separator] = (ImVec4){0.13f, 0.13f, 0.13f, 1.00f};
    colors[ImGuiCol_SeparatorHovered] = (ImVec4){0.16f, 0.16f, 0.16f, 1.00f};
    colors[ImGuiCol_SeparatorActive] = (ImVec4){0.20f, 1.00f, 0.40f, 1.00f};
    colors[ImGuiCol_ResizeGrip] = (ImVec4){0.13f, 0.13f, 0.13f, 1.00f};
    colors[ImGuiCol_ResizeGripHovered] = (ImVec4){0.16f, 0.16f, 0.16f, 1.00f};
    colors[ImGuiCol_ResizeGripActive] = (ImVec4){0.20f, 1.00f, 0.40f, 1.00f};
    colors[ImGuiCol_Tab] = (ImVec4){0.07f, 0.07f, 0.07f, 1.00f};
    colors[ImGuiCol_TabHovered] = (ImVec4){0.14f, 0.14f, 0.14f, 1.00f};
    colors[ImGuiCol_TabSelected] = (ImVec4){0.10f, 0.10f, 0.10f, 1.00f};
    colors[ImGuiCol_TabDimmed] = (ImVec4){0.05f, 0.05f, 0.05f, 1.00f};
    colors[ImGuiCol_TabDimmedSelected] = (ImVec4){0.07f, 0.07f, 0.07f, 1.00f};

    style->WindowRounding = 0.0f;
    style->ChildRounding = 0.0f;
    style->FrameRounding = 0.0f;
    style->PopupRounding = 0.0f;
    style->ScrollbarRounding = 0.0f;
    style->GrabRounding = 0.0f;
    style->TabRounding = 0.0f;
    style->WindowBorderSize = 1.0f;
    style->ChildBorderSize = 1.0f;
    style->PopupBorderSize = 1.0f;
    style->FrameBorderSize = 1.0f;
    style->TabBorderSize = 1.0f;
}

/* ------------------------------------------------------------------------- */
/* UI Sections                                                               */
/* ------------------------------------------------------------------------- */

internal void DrawTopBar(SDL_Window* win, f32 dt)
{
    if (igBeginMainMenuBar()) {
        const char* romName = "No ROM";
        if (nes && loadedFilePath[0]) {
            const char* s = SDL_strrchr(loadedFilePath, '\\');
            if (!s) s = SDL_strrchr(loadedFilePath, '/');
            romName = s ? s + 1 : loadedFilePath;
        }
        igTextColored((ImVec4){0.4f, 0.4f, 0.4f, 1.0f}, "ROM:");
        igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, " %s", romName);
        igSeparator();

        if (igButton("Open", (ImVec2){0, 0})) {
            debugging = true;
            stepping = false;
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Open ROM",
                                     "Drag & drop a .nes or .nsave file onto the window.", win);
        }

        if (debugging) {
            if (igButton("Run", (ImVec2){0, 0})) {
                if (!nes) {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Run", "Load a ROM first.", win);
                } else {
                    hitRun = true;
                    debugging = false;
                    stepping = false;
                }
            }
            if (igButton("Step", (ImVec2){0, 0})) {
                if (nes) stepping = true;
            }
        } else {
            if (igButton("Pause", (ImVec2){0, 0})) {
                debugging = true;
                stepping = false;
            }
        }

        if (igButton("Reset", (ImVec2){0, 0})) {
            if (nes) {
                ResetNES(nes);
                debugging = true;
            }
        }

        if (igButton("Save", (ImVec2){0, 0})) {
            if (nes) {
                debugging = true;
                stepping = false;
                if (saveFilePath[0]) Save(nes, saveFilePath);
                else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Save", "Load a ROM first.", win);
            }
        }

        igSeparator();
        igTextColored((ImVec4){0.4f, 0.4f, 0.4f, 1.0f}, "FPS:");
        igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "%d", (s32)(1.0f / dt));
        igTextColored((ImVec4){0.4f, 0.4f, 0.4f, 1.0f}, "dt:");
        igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "%.4f", dt);

        igSeparator();
        bool oneCyc = (bool)app.ui.oneCycleToggle;
        if (igCheckbox("1-cycle", &oneCyc)) app.ui.oneCycleToggle = oneCyc;
        oneCycleAtTime = app.ui.oneCycleToggle;

        bool dbgToggle = (bool)app.ui.debugToggle;
        if (igCheckbox("Debug", &dbgToggle)) app.ui.debugToggle = dbgToggle;
        debugMode = app.ui.debugToggle;

        igEndMainMenuBar();
    }
}

internal void DrawLeftSidebar(f32 dt)
{
    igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "SYSTEM");
    igSeparator();

    if (nes) {
        if (igCollapsingHeader_TreeNodeFlags("CPU Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
            CPU* cpu = &nes->cpu;
            igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "PC:");
            igSameLine(40, 0);
            igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "$%04X", cpu->pc);
            igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "SP:");
            igSameLine(40, 0);
            igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "$%02X", cpu->sp);
            igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "A:");
            igSameLine(40, 0);
            igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "$%02X", cpu->a);
            igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "X:");
            igSameLine(40, 0);
            igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "$%02X", cpu->x);
            igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "Y:");
            igSameLine(40, 0);
            igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "$%02X", cpu->y);
            igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "P:");
            igSameLine(40, 0);
            igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "$%02X", cpu->p);
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
    ImDrawList_AddRectFilled(draw_list, p_min, p_max, 0xFF1A1A1A, 0.0f, 0);
    ImDrawList_AddRect(draw_list, p_min, p_max, 0xFF333333, 0.0f, 0, 1.0f);

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

        ImDrawList_AddPolyline(draw_list, points, pointCount, 0xFF33FF66, 0, 1.5f);
    }

    igDummy((ImVec2){avail.x, rectHeight}); // Advance cursor
}

internal void DrawOAMTable(NES* nesPtr)
{
    if (igBeginTable("OAMTable", 5, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg,
                     (ImVec2){0, 200}, 0)) {
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
            igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "%d", i);
            igTableNextColumn();
            igText("%02X", spriteX);
            igTableNextColumn();
            igText("%02X", spriteY);
            igTableNextColumn();
            igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "%02X", spriteIdx);
            igTableNextColumn();
            igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "%02X", spriteAttr);
        }
        igEndTable();
    }
}

internal void DrawVideoPanel(Device* device)
{
    if (nes) {
        igText("Pattern Tables");
        UpdatePatternTableTextures(device, nes);
        igImage((ImTextureRef_c){NULL, (ImTextureID)(intptr_t)device->patterns[0]}, (ImVec2){128, 128}, (ImVec2){0, 0},
                (ImVec2){1, 1});
        igSameLine(0, 4);
        igImage((ImTextureRef_c){NULL, (ImTextureID)(intptr_t)device->patterns[1]}, (ImVec2){128, 128}, (ImVec2){0, 0},
                (ImVec2){1, 1});

        igSpacing();
        igSeparator();
        igSpacing();

        igText("Nametables");
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
        igSeparator();
        igSpacing();

        igText("OAM (Object Attribute Memory)");
        DrawOAMTable(nes);
    } else {
        igTextDisabled("No ROM loaded");
    }
}

internal void DrawAudioPanel()
{
    if (nes) {
        APU* apu = &nes->apu;

        igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "GENERAL");
        igText("CYCLES: %lld", apu->cycles);
        igText("FRAME MODE: %02X", apu->frameMode);
        igText("SAMPLE RATE: %04X", APU_SAMPLES_PER_SECOND);
        igText("FRAME IRQ: %02X", !apu->inhibitIRQ);
        igText("FRAME VALUE: %02X", apu->frameValue);
        igText("SAMPLE COUNTER: %02X", apu->sampleCounter);
        igText("DMC IRQ: %02X", apu->dmcIRQ);
        igText("FRAME COUNTER: %02X", apu->frameCounter);
        igText("BUFFER INDEX: %04X", apu->bufferIndex);

        igSpacing();
        igSeparator();
        igSpacing();

        igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "CHANNELS");

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
        igSeparator();
        igSpacing();

        igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "AUDIO FILTERS");

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
        igSeparator();
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
                    igPushStyleColor_Vec4(ImGuiCol_Text, (ImVec4){0.2f, 1.0f, 0.4f, 1.0f});
                } else if (breakpointHit) {
                    igPushStyleColor_Vec4(ImGuiCol_Text, (ImVec4){1.0f, 0.2f, 0.2f, 1.0f});
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
                col += sprintf(debugBuffer + col, "%s", GetInstructionStr(instruction->instruction));

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

                if (instruction->instruction == CPU_RTS) {
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

    if (nes) {
        u16 baseAddress = 0x0000;
        if (app.ui.memoryAddressText[0] != '\0') {
            baseAddress = (u16)strtol(app.ui.memoryAddressText, NULL, 16);
        }

        bool child_visible = igBeginChild_Str("MemoryView", (ImVec2){0, 0}, false, ImGuiWindowFlags_None);
        if (child_visible) {
            if (igBeginTable("HexDump", 17, ImGuiTableFlags_None, (ImVec2){0, 0}, 0)) {
                for (s32 i = 0; i < 64; ++i) {
                    igTableNextRow(ImGuiTableRowFlags_None, 0);
                    igTableNextColumn();
                    igTextColored((ImVec4){0.2f, 1.0f, 0.4f, 1.0f}, "%04X", baseAddress + i * 16);

                    for (s32 j = 0; j < 16; ++j) {
                        igTableNextColumn();
                        u16 addr = baseAddress + i * 16 + j;
                        u8 v = 0;

                        if (app.ui.memoryOption == 0) {
                            v = ReadCPUU8(nes, addr);
                        } else if (app.ui.memoryOption == 1) {
                            v = ReadPPUU8(nes, addr);
                        } else if (app.ui.memoryOption == 2) {
                            v = ReadU8(&nes->oamMemory, addr % 256);
                        } else if (app.ui.memoryOption == 3) {
                            v = ReadU8(&nes->oamMemory2, addr % 32);
                        }

                        igText("%02X", v);
                    }
                }
                igEndTable();
            }
        }
        igEndChild();
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

        igSetCursorPos((ImVec2){ox, oy});
        igImage((ImTextureRef_c){NULL, (ImTextureID)(intptr_t)device->screen}, (ImVec2){drawW, drawH}, (ImVec2){0, 0},
                (ImVec2){1, 1});
    }
}


void DrawUI(SDL_Window* win, Device* device, f32 dt)
{
    DrawTopBar(win, dt);

    ImGuiViewport* viewport = igGetMainViewport();
    igDockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode, NULL);

    if (debugMode) {
        if (igBegin("SYSTEM", NULL, ImGuiWindowFlags_None)) {
            DrawLeftSidebar(dt);
        }
        igEnd();

        if (igBegin("VIDEO", NULL, ImGuiWindowFlags_None)) {
            DrawVideoPanel(device);
        }
        igEnd();

        if (igBegin("AUDIO", NULL, ImGuiWindowFlags_None)) {
            DrawAudioPanel();
        }
        igEnd();

        if (igBegin("INSTRUCTIONS", NULL, ImGuiWindowFlags_None)) {
            DrawInstructionsPanel();
        }
        igEnd();

        if (igBegin("MEMORY", NULL, ImGuiWindowFlags_None)) {
            DrawMemoryPanel();
        }
        igEnd();
    }

    ImGuiWindowFlags screenFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (igBegin("NES Screen", NULL, screenFlags)) {
        DrawGameScreen(device);
    }
    igEnd();

    igRender();
}
