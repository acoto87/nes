#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "utils.h"
#include "types.h"
#include "cartridge.h"
#include "nes.h"
#include "cpu.h"
#include "cpu_debug.h"
#include "ppu.h"
#include "ppu_debug.h"
#include "apu.h"
#include "controller.h"
#include "gui.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GL2_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_gl2.h"

#define SHL_WAVE_WRITER_IMPLEMENTATION
#include "wave_writer.h"

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define NUM_TEXTURES 1 + 2 + 1 + 64 + 8 + 1 + 960

#define NK_SHADER_VERSION "#version 150\n"

typedef struct RuntimeState RuntimeState;
struct RuntimeState
{
    s64 perfCountFrequency;
    NES *nes;
    char loadedFilePath[1024];
    char saveFilePath[1024];
};

typedef struct EmuControlState EmuControlState;
struct EmuControlState
{
    b32 hitRun;
    b32 debugging;
    b32 stepping;
    u16 breakpoint;
};

typedef struct UiState UiState;
struct UiState
{
    char debugBuffer[256];

    b32 coarseButtons[8];
    b32 oneCycleAtTime;
    b32 debugMode;

    s32 fps;
    s32 fpsCount;
    s32 oneCycleToggle;
    s32 debugToggle;

    char instructionAddressText[12];
    s32 instructionAddressLen;
    char instructionBreakpointText[5];
    s32 instructionBreakpointLen;

    s32 memoryOption;
    char memoryAddressText[12];
    s32 memoryAddressLen;

    s32 videoOption;
    s32 nametableOption;
    s32 showSeparatePixels;

    s32 audioOption;
    s32 square1Enabled;
    s32 square2Enabled;
    s32 triangleEnabled;
    s32 noiseEnabled;
    s32 dmcEnabled;
};

typedef struct AppState AppState;
struct AppState
{
    RuntimeState runtime;
    EmuControlState control;
    UiState ui;
};

global AppState app = {
    .control = {
        .debugging = TRUE,
    },
    .ui = {
        .square1Enabled = TRUE,
        .square2Enabled = TRUE,
        .triangleEnabled = TRUE,
        .noiseEnabled = TRUE,
        .dmcEnabled = TRUE,
    },
};

#define globalPerfCountFrequency (app.runtime.perfCountFrequency)
#define debugBuffer (app.ui.debugBuffer)
#define hitRun (app.control.hitRun)
#define debugging (app.control.debugging)
#define stepping (app.control.stepping)
#define breakpoint (app.control.breakpoint)
#define coarseButtons (app.ui.coarseButtons)
#define oneCycleAtTime (app.ui.oneCycleAtTime)
#define debugMode (app.ui.debugMode)
#define nes (app.runtime.nes)
#define loadedFilePath (app.runtime.loadedFilePath)
#define saveFilePath (app.runtime.saveFilePath)

typedef struct UiLayout UiLayout;
struct UiLayout
{
    struct nk_rect fps;
    struct nk_rect cpu;
    struct nk_rect controller;
    struct nk_rect ppu;
    struct nk_rect screen;
    struct nk_rect instructions;
    struct nk_rect memory;
    struct nk_rect video;
    struct nk_rect audio;
};

global PFNGLGENBUFFERSPROC GLGenBuffers;
global PFNGLDELETEBUFFERSPROC GLDeleteBuffers;
global PFNGLBINDBUFFERPROC GLBindBuffer;
global PFNGLGENVERTEXARRAYSPROC GLGenVertexArrays;
global PFNGLDELETEVERTEXARRAYSPROC GLDeleteVertexArrays;
global PFNGLBINDVERTEXARRAYPROC GLBindVertexArray;
global PFNGLGENERATEMIPMAPPROC GLGenerateMipmap;

typedef struct Device Device;
struct Device {
    struct nk_buffer cmds;
    GLuint vbo, vao, ebo;

    struct nk_image screen;
    struct nk_image patterns[2];
    struct nk_image patternHover;
    struct nk_image oam[64];
    struct nk_image oam2[8];
    struct nk_image nametable;
    struct nk_image nametable2[960];
};

#define ONE_MINUTE_OF_SOUND (48000 * 60 * 2 * 8)

internal inline f32 GetSecondsElapsed(u64 start, u64 end)
{
    f32 result = ((f32)(end - start) / (f32)globalPerfCountFrequency);
    return result;
}

internal void* LoadGLProc(const char *name)
{
    void *proc = SDL_GL_GetProcAddress(name);
    ASSERT(proc);
    return proc;
}

internal void LoadGLFunctions(void)
{
    GLGenBuffers = (PFNGLGENBUFFERSPROC)LoadGLProc("glGenBuffers");
    GLDeleteBuffers = (PFNGLDELETEBUFFERSPROC)LoadGLProc("glDeleteBuffers");
    GLBindBuffer = (PFNGLBINDBUFFERPROC)LoadGLProc("glBindBuffer");
    GLGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)LoadGLProc("glGenVertexArrays");
    GLDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)LoadGLProc("glDeleteVertexArrays");
    GLBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)LoadGLProc("glBindVertexArray");
    GLGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)LoadGLProc("glGenerateMipmap");
}

internal void CopyString(char *dest, size_t destSize, const char *src)
{
    if (!destSize)
    {
        return;
    }

    if (!src)
    {
        dest[0] = 0;
        return;
    }

    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = 0;
}

internal b32 EndsWith(const char *text, const char *suffix)
{
    size_t textLen = strlen(text);
    size_t suffixLen = strlen(suffix);
    if (textLen < suffixLen)
    {
        return FALSE;
    }

    return strcmp(text + textLen - suffixLen, suffix) == 0;
}

internal const char* GetFilenamePart(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *name = path;

    if (slash && (!backslash || slash > backslash))
    {
        name = slash + 1;
    }
    else if (backslash)
    {
        name = backslash + 1;
    }

    return name;
}

internal void UpdateWindowTitle(SDL_Window *win, const char *path)
{
    char windowTitle[256] = "Nes emulator";
    if (path && path[0])
    {
        snprintf(windowTitle, sizeof(windowTitle), "Nes emulator: %s", GetFilenamePart(path));
    }

    SDL_SetWindowTitle(win, windowTitle);
}

internal void BuildSavePath(const char *sourcePath, char *dest, size_t destSize)
{
    const char *extension = strrchr(sourcePath, '.');
    size_t prefixLength = extension ? (size_t)(extension - sourcePath) : strlen(sourcePath);

    if (prefixLength >= destSize)
    {
        prefixLength = destSize - 1;
    }

    memcpy(dest, sourcePath, prefixLength);
    dest[prefixLength] = 0;
    strncat(dest, ".nsave", destSize - strlen(dest) - 1);
}

internal b32 LoadFileIntoApp(SDL_Window *win, const char *path)
{
    if (!path || !path[0])
    {
        return FALSE;
    }

    if (EndsWith(path, ".nes"))
    {
        Cartridge cartridge = {0};
        if (!LoadNesRom((char*)path, &cartridge))
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "The file couldn't be loaded!", win);
            return FALSE;
        }

        if (nes)
        {
            Destroy(nes);
        }

        nes = CreateNES(cartridge);
        if (!nes)
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unsupported mapper", "This ROM uses a mapper that is not implemented yet.", win);
            return FALSE;
        }

        CopyString(loadedFilePath, sizeof(loadedFilePath), path);
        BuildSavePath(path, saveFilePath, sizeof(saveFilePath));
        UpdateWindowTitle(win, path);
        return TRUE;
    }

    if (EndsWith(path, ".nsave"))
    {
        NES *loaded = LoadNESSave((char*)path);
        if (!loaded)
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "The file couldn't be loaded!", win);
            return FALSE;
        }

        if (nes)
        {
            Destroy(nes);
        }

        nes = loaded;
        CopyString(loadedFilePath, sizeof(loadedFilePath), path);
        CopyString(saveFilePath, sizeof(saveFilePath), path);
        if (nes->cartridge.path[0])
        {
            CopyString(loadedFilePath, sizeof(loadedFilePath), nes->cartridge.path);
        }
        UpdateWindowTitle(win, loadedFilePath);
        return TRUE;
    }

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Only .nes and .nsave files are supported.", win);
    return FALSE;
}

internal inline char* DebugText(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    memset(debugBuffer, 0, sizeof(debugBuffer));
    vsprintf(debugBuffer, fmt, ap);
    return debugBuffer;
}

internal void InitDevice(struct Device *dev)
{
    nk_buffer_init_default(&dev->cmds);

    GLGenBuffers(1, &dev->vbo);
    GLGenBuffers(1, &dev->ebo);
    GLGenVertexArrays(1, &dev->vao);

    GLBindVertexArray(dev->vao);
    GLBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
    GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

    glBindTexture(GL_TEXTURE_2D, 0);
    GLBindBuffer(GL_ARRAY_BUFFER, 0);
    GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    GLBindVertexArray(0);
}

internal struct nk_image InitTexture(GLuint tex, GLsizei width, GLsizei height)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    GLGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    return nk_image_id((s32)tex);
}

internal void InitTextures(struct Device *dev)
{
    glEnable(GL_TEXTURE_2D);

    s32 index = 0;
    GLuint textures[NUM_TEXTURES]; // init 1036 textures

    glGenTextures(NUM_TEXTURES, textures);

    dev->screen = InitTexture(textures[index++], 256, 240);

    for (s32 i = 0; i < 2; ++i)
    {
        dev->patterns[i] = InitTexture(textures[index++], 128, 128);
    }

    dev->patternHover = InitTexture(textures[index++], 8, 8);

    for (s32 i = 0; i < 64; ++i)
    {
        dev->oam[i] = InitTexture(textures[index++], 8, 16);
    }

    for (s32 i = 0; i < 8; ++i)
    {
        dev->oam2[i] = InitTexture(textures[index++], 8, 16);
    }

    dev->nametable = InitTexture(textures[index++], 256, 240);

    for (s32 i = 0; i < 960; ++i)
    {
        dev->nametable2[i] = InitTexture(textures[index++], 8, 8);
    }
}

internal void DeleteTextures(Device *dev)
{
    s32 index = 0;
    GLuint textures[NUM_TEXTURES]; // delete 1036 textures

    textures[index++] = dev->screen.handle.id;

    for (s32 i = 0; i < 2; ++i)
    {
        textures[index++] = dev->patterns[i].handle.id;
    }

    textures[index++] = dev->patternHover.handle.id;

    for (s32 i = 0; i < 64; ++i)
    {
        textures[index++] = dev->oam[i].handle.id;
    }

    for (s32 i = 0; i < 8; ++i)
    {
        textures[index++] = dev->oam2[i].handle.id;
    }

    textures[index++] = dev->nametable.handle.id;

    for (s32 i = 0; i < 960; ++i)
    {
        textures[index++] = dev->nametable2[i].handle.id;
    }

    glDeleteTextures(NUM_TEXTURES, textures);
    glDisable(GL_TEXTURE_2D);

    if (dev->vbo)
    {
        GLDeleteBuffers(1, &dev->vbo);
    }

    if (dev->ebo)
    {
        GLDeleteBuffers(1, &dev->ebo);
    }

    if (dev->vao)
    {
        GLDeleteVertexArrays(1, &dev->vao);
    }
}

internal void UpdateControllerInput(struct nk_context *ctx, SDL_GameController *controller)
{
    const Uint8 *keyboard = SDL_GetKeyboardState(NULL);

    SetButton(nes, 0, BUTTON_UP, ctx->input.keyboard.keys[NK_KEY_UP].down || coarseButtons[1]);
    SetButton(nes, 0, BUTTON_DOWN, ctx->input.keyboard.keys[NK_KEY_DOWN].down || coarseButtons[3]);
    SetButton(nes, 0, BUTTON_LEFT, ctx->input.keyboard.keys[NK_KEY_LEFT].down || coarseButtons[0]);
    SetButton(nes, 0, BUTTON_RIGHT, ctx->input.keyboard.keys[NK_KEY_RIGHT].down || coarseButtons[2]);
    SetButton(nes, 0, BUTTON_SELECT, keyboard[SDL_SCANCODE_SPACE] || coarseButtons[4]);
    SetButton(nes, 0, BUTTON_START, ctx->input.keyboard.keys[NK_KEY_ENTER].down || coarseButtons[5]);
    SetButton(nes, 0, BUTTON_B, keyboard[SDL_SCANCODE_S] || coarseButtons[6]);
    SetButton(nes, 0, BUTTON_A, keyboard[SDL_SCANCODE_A] || coarseButtons[7]);

    if (controller)
    {
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A))
        {
            SetButton(nes, 0, BUTTON_A, TRUE);
        }

        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X))
        {
            SetButton(nes, 0, BUTTON_B, TRUE);
        }

        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START))
        {
            SetButton(nes, 0, BUTTON_START, TRUE);
        }

        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK))
        {
            SetButton(nes, 0, BUTTON_SELECT, TRUE);
        }

        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP))
        {
            SetButton(nes, 0, BUTTON_UP, TRUE);
        }

        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
        {
            SetButton(nes, 0, BUTTON_DOWN, TRUE);
        }

        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
        {
            SetButton(nes, 0, BUTTON_LEFT, TRUE);
        }

        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        {
            SetButton(nes, 0, BUTTON_RIGHT, TRUE);
        }
    }
}

internal void QueueAudioBuffer(APU *apu, SDL_AudioDeviceID dev, SDL_AudioStream *audioStream)
{
    if (!dev)
    {
        return;
    }

    s32 bytesToQueue = apu->bufferIndex * APU_BYTES_PER_SAMPLE;

    if (audioStream)
    {
        if (SDL_AudioStreamPut(audioStream, apu->buffer, bytesToQueue) == 0)
        {
            s32 convertedBytes = SDL_AudioStreamAvailable(audioStream);
            if (convertedBytes > 0)
            {
                u8 *convertedBuffer = (u8*)SDL_malloc(convertedBytes);
                if (convertedBuffer)
                {
                    s32 readBytes = SDL_AudioStreamGet(audioStream, convertedBuffer, convertedBytes);
                    if (readBytes > 0)
                    {
                        SDL_QueueAudio(dev, convertedBuffer, readBytes);
                    }
                    SDL_free(convertedBuffer);
                }
            }
        }
    }
    else
    {
        SDL_QueueAudio(dev, apu->buffer, bytesToQueue);
    }
}

internal void RunEmulatorFrame(struct nk_context *ctx, SDL_GameController *controller, SDL_AudioDeviceID dev, SDL_AudioStream *audioStream)
{
    if (!nes)
    {
        return;
    }

    CPU *cpu = &nes->cpu;
    APU *apu = &nes->apu;

    UpdateControllerInput(ctx, controller);

    // Frame
    // I'm targeting 60fps, so run the amount of cpu cycles for 0.0167 seconds
    //
    // This could be calculated with the frequency of the PPU, and run based on
    // the amount of cycles that we want the PPU to run in 0.0167 seconds. For now
    // i'm using the CPU frequency.
    s32 cycles = 0.0167 * CPU_FREQ;

    // If we are stepping in the debugger, run only 1 instruction
    // so set cycles = 1 to enter the while only one time
    if (stepping || oneCycleAtTime)
    {
        cycles = 1;
    }

    // Reset the index of the audio buffer in the APU
    apu->bufferIndex = 0;
    apu->pulse1.bufferIndex = 0;
    apu->pulse2.bufferIndex = 0;
    apu->triangle.bufferIndex = 0;
    apu->noise.bufferIndex = 0;
    apu->dmc.bufferIndex = 0;

    while (cycles > 0)
    {
        if (!debugging)
        {
            if (!hitRun)
            {
                if (cpu->pc == breakpoint)
                {
                    debugging = TRUE;
                    stepping = FALSE;
                }
            }
            else
            {
                hitRun = FALSE;
            }
        }

        if (debugging && !stepping)
        {
            break;
        }

        CPUStep step = StepCPU(nes);
        cycles -= step.cycles;

        if (debugging)
        {
            stepping = FALSE;
        }
    }

    QueueAudioBuffer(apu, dev, audioStream);
}

internal UiLayout ComputeUiLayout(s32 winWidth, s32 winHeight)
{
    UiLayout layout;
    f32 w = (f32)winWidth;
    f32 h = (f32)winHeight;

    f32 gap = 10.0f;
    f32 left = 10.0f;
    f32 top = 10.0f;
    f32 right = 10.0f;
    f32 bottom = 10.0f;

    if (!debugMode)
    {
        layout.fps = nk_rect(left, top, 290, 190);

        f32 sx = layout.fps.x + layout.fps.w + gap;
        f32 sy = top;
        f32 sw = w - sx - right;
        f32 sh = h - sy - bottom;

        if (sw < 320) sw = 320;
        if (sh < 240) sh = 240;

        layout.screen = nk_rect(sx, sy, sw, sh);

        layout.cpu = nk_rect(0, 0, 0, 0);
        layout.controller = nk_rect(0, 0, 0, 0);
        layout.ppu = nk_rect(0, 0, 0, 0);
        layout.instructions = nk_rect(0, 0, 0, 0);
        layout.memory = nk_rect(0, 0, 0, 0);
        layout.video = nk_rect(0, 0, 0, 0);
        layout.audio = nk_rect(0, 0, 0, 0);
        return layout;
    }

    f32 colLeftW = MAX(260.0f, w * 0.23f);
    f32 colRightW = MAX(330.0f, w * 0.37f);
    f32 contentW = w - left - right;
    if (colLeftW + colRightW + gap > contentW)
    {
        colLeftW = contentW * 0.34f;
        colRightW = contentW * 0.42f;
    }
    f32 colMidW = contentW - colLeftW - colRightW - gap * 2.0f;
    if (colMidW < 220)
    {
        colMidW = 220;
    }

    f32 x0 = left;
    f32 x1 = x0 + colLeftW + gap;
    f32 x2 = x1 + colMidW + gap;

    f32 y = top;

    layout.fps = nk_rect(x0, y, colLeftW, 190);
    y += 190 + gap;
    layout.instructions = nk_rect(x0, y, colLeftW, h - y - bottom);

    f32 y1 = top;
    layout.cpu = nk_rect(x1, y1, colMidW, 170);
    y1 += 170 + gap;
    layout.controller = nk_rect(x1, y1, colMidW, 120);
    y1 += 120 + gap;
    layout.memory = nk_rect(x1, y1, colMidW, h - y1 - bottom);

    f32 topRowH = MAX(300.0f, h * 0.36f);
    if (topRowH > h - 220) topRowH = h - 220;
    layout.ppu = nk_rect(x2, top, colRightW * 0.58f, topRowH);
    layout.screen = nk_rect(x2 + layout.ppu.w + gap, top, colRightW - layout.ppu.w - gap, topRowH);

    f32 y2 = top + topRowH + gap;
    f32 remH = h - y2 - bottom;
    f32 audioH = MAX(250.0f, remH * 0.48f);
    if (audioH > remH - 120) audioH = remH - 120;
    layout.audio = nk_rect(x2, y2, colRightW, audioH);
    layout.video = nk_rect(x2, y2 + audioH + gap, colRightW, h - (y2 + audioH + gap) - bottom);

    return layout;
}

internal void DrawMainPanels(struct nk_context *ctx, nk_flags flags, SDL_Window *win, Device *device, const UiLayout *layout, f32 dt, f32 d1, f32 d2, u64 *initialCounter)
{
    if (nk_begin(ctx, "FPS INFO", layout->fps, flags))
    {
        nk_layout_row_dynamic(ctx, 20, 2);

        u64 fpsCounter = SDL_GetPerformanceCounter();
        f32 fpsdt = GetSecondsElapsed(*initialCounter, fpsCounter);
        if (fpsdt > 1)
        {
            *initialCounter = fpsCounter;
            app.ui.fps = app.ui.fpsCount;
            app.ui.fpsCount = 0;
        }

        ++app.ui.fpsCount;

        nk_label(ctx, DebugText("fps: %d", app.ui.fps), NK_TEXT_LEFT);
        nk_label(ctx, DebugText("dt: %.4f", dt), NK_TEXT_LEFT);

        nk_checkbox_label(ctx, "debug mode", &app.ui.debugToggle);
        debugMode = app.ui.debugToggle;

        if (debugMode)
        {
            nk_checkbox_label(ctx, "1 cycle", &app.ui.oneCycleToggle);
            oneCycleAtTime = app.ui.oneCycleToggle;

            nk_label(ctx, DebugText("d1: %.4f", d1), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("d2: %.4f", d2), NK_TEXT_LEFT);
        }

        nk_layout_row_dynamic(ctx, 20, 2);

        if (nk_button_label(ctx, "Open"))
        {
            debugging = TRUE;
            stepping = FALSE;

            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                "Open ROM",
                "Open a ROM by passing a file path on the command line or by dragging a .nes/.nsave file onto the window.",
                win);
        }

        if (nk_button_label(ctx, "Save"))
        {
            if (nes)
            {
                debugging = TRUE;
                stepping = FALSE;
                if (saveFilePath[0])
                {
                    Save(nes, saveFilePath);
                }
                else
                {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                        "Save State",
                        "Load a ROM first so the emulator can derive a save-state path.",
                        win);
                }
            }
        }

        if (nk_button_label(ctx, "Run"))
        {
            hitRun = TRUE;
            debugging = FALSE;
            stepping = FALSE;
        }

        if (nk_button_label(ctx, "Reset"))
        {
            if (nes)
            {
                ResetNES(nes);
                debugging = TRUE;
            }
        }

        if (nk_button_label(ctx, "Pause"))
        {
            debugging = TRUE;
            stepping = FALSE;
        }

        if (debugMode)
        {
            if (nk_button_label(ctx, "Step"))
            {
                debugging = TRUE;
                stepping = TRUE;
            }
        }
    }
    nk_end(ctx);

    if (debugMode)
    {
        if (nk_begin(ctx, "CPU INFO", layout->cpu, flags) && nes)
        {
            CPU *cpu = &nes->cpu;

            nk_layout_row_dynamic(ctx, 20, 2);

            nk_label(ctx, DebugText("%s:%02X", "A", cpu->a), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("%s:%02X", "P", cpu->p), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("%s:%02X", "X", cpu->x), NK_TEXT_LEFT);
            nk_label(ctx, "N V   B D I Z C", NK_TEXT_LEFT);
            nk_label(ctx, DebugText("%s:%02X", "Y", cpu->y), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("%d %d %d %d %d %d %d %d",
                GetBitFlag(cpu->p, NEGATIVE_FLAG) ? 1 : 0,
                GetBitFlag(cpu->p, OVERFLOW_FLAG) ? 1 : 0,
                GetBitFlag(cpu->p, EMPTY_FLAG) ? 1 : 0,
                GetBitFlag(cpu->p, BREAK_FLAG) ? 1 : 0,
                GetBitFlag(cpu->p, DECIMAL_FLAG) ? 1 : 0,
                GetBitFlag(cpu->p, INTERRUPT_FLAG) ? 1 : 0,
                GetBitFlag(cpu->p, ZERO_FLAG) ? 1 : 0,
                GetBitFlag(cpu->p, CARRY_FLAG) ? 1 : 0), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("%2s:%02X", "SP", cpu->sp), NK_TEXT_LEFT);

            nk_label(ctx, DebugText("%2s:%02X", "PC", cpu->pc), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("%s:%d", "CYCLES", cpu->cycles), NK_TEXT_LEFT);
        }
        nk_end(ctx);
    }

    if (debugMode)
    {
        if (nk_begin(ctx, "CONTROLLER 0", layout->controller, flags) && nes)
        {
            s32 buttons[8] =
            {
                GetButton(nes, 0, BUTTON_LEFT),
                GetButton(nes, 0, BUTTON_UP),
                GetButton(nes, 0, BUTTON_RIGHT),
                GetButton(nes, 0, BUTTON_DOWN),
                GetButton(nes, 0, BUTTON_SELECT),
                GetButton(nes, 0, BUTTON_START),
                GetButton(nes, 0, BUTTON_B),
                GetButton(nes, 0, BUTTON_A)
            };

            nk_layout_row_static(ctx, 20, 20, 2);
            nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
            nk_checkbox_label(ctx, "", &buttons[1]);

            nk_layout_row_static(ctx, 20, 20, 9);
            nk_checkbox_label(ctx, "", &buttons[0]);
            nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
            nk_checkbox_label(ctx, "", &buttons[2]);
            nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
            nk_checkbox_label(ctx, "", &buttons[4]);
            nk_checkbox_label(ctx, "", &buttons[5]);
            nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
            nk_checkbox_label(ctx, "", &buttons[6]);
            nk_checkbox_label(ctx, "", &buttons[7]);

            nk_layout_row_static(ctx, 20, 20, 2);
            nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
            nk_checkbox_label(ctx, "", &buttons[3]);
        }
        nk_end(ctx);
    }

    if (debugMode)
    {
        if (nk_begin(ctx, "PPU INFO", layout->ppu, flags) && nes)
        {
            PPU *ppu = &nes->ppu;

            nk_layout_row_dynamic(ctx, 20, 2);

            nk_label(ctx, DebugText("CTRL (0x2000):%02X", ppu->control), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("SCANLINE:%3d", ppu->scanline), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("MASK (0x2001):%02X", ppu->mask), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("CYCLE:%3d", ppu->cycle), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("STAT (0x2002):%02X", ppu->status), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("CYCLES:%lld", ppu->totalCycles), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("SPRA (0x2003):%02X", ppu->oamAddress), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("FRAMES:%lld", ppu->frameCount), NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, DebugText("SPRD (0x2004):%02X", ppu->oamData), NK_TEXT_LEFT);

            nk_label(ctx, DebugText("SCRR (0x2005):%02X", ppu->scroll), NK_TEXT_LEFT);

            nk_label(ctx, DebugText("MEMA (0x2006):%02X", ppu->address), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("    %s:%04X  %s:%04X", "v", ppu->v, "t", ppu->t), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("    %s:%04X  %s:%01X", "x", ppu->x, "w", ppu->w), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("MEMD (0x2007):%02X", ppu->data), NK_TEXT_LEFT);
        }
        nk_end(ctx);
    }

    struct nk_rect screenRect = layout->screen;

    if (nk_begin(ctx, "SCREEN", screenRect, flags))
    {
        ctx->current->bounds = screenRect;

        if (nes)
        {
            GUI *gui = &nes->gui;

            s32 width = 256, height = 240;

            if (!debugMode)
            {
                width *= 3;
                height *= 3;
            }

            nk_layout_row_static(ctx, height, width, 1);

            struct nk_command_buffer *canvas;
            canvas = nk_window_get_canvas(ctx);

            struct nk_rect space;
            enum nk_widget_layout_states state;
            state = nk_widget(&space, ctx);
            if (state)
            {
                if (state != NK_WIDGET_ROM)
                {
                    // update_your_widget_by_user_input(...);
                }

                glBindTexture(GL_TEXTURE_2D, device->screen.handle.id);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGBA, GL_UNSIGNED_BYTE, gui->pixels);
                glBindTexture(GL_TEXTURE_2D, 0);

                nk_draw_image(canvas, space, &device->screen, nk_rgb(255, 255, 255));
            }
        }
    }
    nk_end(ctx);
}

internal void DrawInstructionsPanel(struct nk_context *ctx, nk_flags flags, const UiLayout *layout)
{
    if (!debugMode)
    {
        return;
    }

    if (nk_begin(ctx, "INSTRUCTIONS", layout->instructions, flags) && nes)
    {
        CPU* cpu = &nes->cpu;
        const float ratio[] = { 100, 100 };

        nk_layout_row(ctx, NK_STATIC, 25, 3, ratio);
        nk_label(ctx, "  Address: ", NK_TEXT_LEFT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, app.ui.instructionAddressText, &app.ui.instructionAddressLen, 12, nk_filter_hex);

        nk_layout_row(ctx, NK_STATIC, 25, 3, ratio);
        nk_label(ctx, "  Breakpoint: ", NK_TEXT_LEFT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, app.ui.instructionBreakpointText, &app.ui.instructionBreakpointLen, 5, nk_filter_hex);

        nk_layout_row_dynamic(ctx, 20, 1);

        u16 pc = cpu->pc;
        if (app.ui.instructionAddressLen > 0)
        {
            app.ui.instructionAddressText[app.ui.instructionAddressLen] = 0;
            pc = (u16)strtol(app.ui.instructionAddressText, NULL, 16);
            if (pc < 0x8000 || pc > 0xFFFF)
            {
                pc = cpu->pc;
            }
        }

        if (app.ui.instructionBreakpointLen > 0)
        {
            app.ui.instructionBreakpointText[app.ui.instructionBreakpointLen] = 0;
            breakpoint = (u16)strtol(app.ui.instructionBreakpointText, NULL, 16);
        }

        for (s32 i = 0; i < 100; ++i)
        {
            memset(debugBuffer, 0, sizeof(debugBuffer));

            u8 opcode = ReadCPUU8(nes, pc);
            CPUInstruction *instruction = &cpuInstructions[opcode];
            s32 col = 0;
            b32 currentInstr = (pc == cpu->pc);
            b32 breakpointHit = (pc == breakpoint);

            col += currentInstr
                ? (breakpointHit
                    ? sprintf(debugBuffer, "O>%04X %02X", pc, instruction->opcode)
                    : sprintf(debugBuffer, "> %04X %02X", pc, instruction->opcode))
                : (breakpointHit
                    ? sprintf(debugBuffer, "O %04X %02X", pc, instruction->opcode)
                    : sprintf(debugBuffer, "  %04X %02X", pc, instruction->opcode));

            switch (instruction->bytesCount)
            {
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

            switch (instruction->addressingMode)
            {
                case AM_IMM:
                    col += sprintf(debugBuffer + col, " #$%02X", ReadCPUU8(nes, pc + 1));
                    break;
                case AM_ABS:
                {
                    u8 lo = ReadCPUU8(nes, pc + 1);
                    u8 hi = ReadCPUU8(nes, pc + 2);
                    col += sprintf(debugBuffer + col, " $%04X", (hi << 8) | lo);
                    break;
                }
                case AM_ABX:
                {
                    u8 lo = ReadCPUU8(nes, pc + 1);
                    u8 hi = ReadCPUU8(nes, pc + 2);
                    col += sprintf(debugBuffer + col, " $%04X, X", (hi << 8) | lo);
                    break;
                }
                case AM_ABY:
                {
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
                case AM_IND:
                {
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
                case AM_REL:
                {
                    s8 address = (s8)ReadCPUU8(nes, pc + 1);
                    u16 jumpAddress = pc + instruction->bytesCount + address;
                    col += sprintf(debugBuffer + col, " $%04X", jumpAddress);
                    break;
                }
                default:
                    break;
            }

            if (instruction->instruction == CPU_RTS)
            {
                col += sprintf(debugBuffer + col, " -------------");
            }

            nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
            pc += instruction->bytesCount;
        }
    }
    nk_end(ctx);
}

int main(int argc, char **argv)
{
    /* Platform */
    SDL_Window *win;
    SDL_GLContext glContext;
    int win_width, win_height;
    b32 running, quit = FALSE;
    f32 dt = 0;

    u64 initialCounter;

    /* GUI */
    struct nk_context *ctx;
    struct Device device;
    SDL_GameController *controller = NULL;

    initialCounter = SDL_GetPerformanceCounter();
    globalPerfCountFrequency = SDL_GetPerformanceFrequency();

    /* SDL setup */
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    win = SDL_CreateWindow(
        "Nes emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    glContext = SDL_GL_CreateContext(win);
    SDL_GetWindowSize(win, &win_width, &win_height);
    LoadGLFunctions();

    s32 numJoysticks = SDL_NumJoysticks();
    if (numJoysticks > 0)
    {
        for (s32 i = 0; i < numJoysticks; ++i)
        {
            if (SDL_IsGameController(i))
            {
                controller = SDL_GameControllerOpen(i);
            }
        }
    }

    /* GUI */
    ctx = nk_sdl_init(win);
    {
        struct nk_font_atlas *atlas;
        nk_sdl_font_stash_begin(&atlas);
        struct nk_font *future = nk_font_atlas_add_from_file(atlas, "fonts/consola.ttf", 14, 0);
        nk_sdl_font_stash_end();
        /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
        if (future)
        {
            nk_style_set_font(ctx, &future->handle);
        }
    }

    InitDevice(&device);
    InitTextures(&device);

    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
    SDL_AudioStream *audioStream = NULL;

    SDL_memset(&want, 0, sizeof(want));
    want.freq = APU_SAMPLES_PER_SECOND;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = NULL;

    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (dev)
    {
        audioStream = SDL_NewAudioStream(
            want.format, want.channels, want.freq,
            have.format, have.channels, have.freq);

        if (!audioStream)
        {
            SDL_Log("Failed to create SDL audio stream: %s", SDL_GetError());
        }

        if (have.format != want.format || have.channels != want.channels || have.freq != want.freq)
        {
            SDL_Log("Audio device format changed: freq=%d channels=%d format=0x%X", have.freq, have.channels, have.format);
        }

        SDL_PauseAudioDevice(dev, 0);
    }
    else
    {
        SDL_Log("Failed to open SDL audio device: %s", SDL_GetError());
    }

    u64 startCounter = SDL_GetPerformanceCounter();
    dt = GetSecondsElapsed(initialCounter, startCounter);
    running = TRUE;

    /*
    * Emulator authors :
    *
    * This test program, when run on "automation", (i.e.set your program counter
    * to 0c000h) will perform all tests in sequence and shove the results of
    * the tests into locations 00h.
    *
    * @Note: All oficial opcodes from nestest.nes passed!
    *                                                  -acoto87 January 30, 2017
    */
    // nes->cpu.pc = 0xC000;
    f32 d1 = 0, d2 = 0;

    if (argc > 1)
    {
        LoadFileIntoApp(win, argv[1]);
    }

    while (running)
    {
        /* Input */
        SDL_Event evt;
        nk_input_begin(ctx);
        while (SDL_PollEvent(&evt))
        {
            if (evt.type == SDL_QUIT)
            {
                quit = TRUE;
                break;
            }

            if (evt.type == SDL_DROPFILE)
            {
                LoadFileIntoApp(win, evt.drop.file);
                SDL_free(evt.drop.file);
                continue;
            }

            nk_sdl_handle_event(&evt);
        }
        nk_input_end(ctx);

        if (quit)
        {
            break;
        }

        u64 s = SDL_GetPerformanceCounter();

        RunEmulatorFrame(ctx, controller, dev, audioStream);

        u64 e = SDL_GetPerformanceCounter();
        d1 = GetSecondsElapsed(s, e);

        s = SDL_GetPerformanceCounter();

        /* GUI */
        nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE;
        SDL_GetWindowSize(win, &win_width, &win_height);
        UiLayout layout = ComputeUiLayout(win_width, win_height);

        DrawMainPanels(ctx, flags, win, &device, &layout, dt, d1, d2, &initialCounter);

        DrawInstructionsPanel(ctx, flags, &layout);

        if (debugMode)
        {
            if (nk_begin(ctx, "MEMORY", layout.memory, flags) && nes)
            {
                enum options { CPU_MEM, PPU_MEM, OAM_MEM, OAM2_MEM };
                const float ratio[] = { 80, 80, 80, 80 };

                if (app.ui.memoryOption < CPU_MEM || app.ui.memoryOption > OAM2_MEM)
                {
                    app.ui.memoryOption = CPU_MEM;
                }

                nk_layout_row(ctx, NK_STATIC, 25, 4, ratio);
                app.ui.memoryOption = nk_option_label(ctx, "CPU", app.ui.memoryOption == CPU_MEM) ? CPU_MEM : app.ui.memoryOption;
                app.ui.memoryOption = nk_option_label(ctx, "PPU", app.ui.memoryOption == PPU_MEM) ? PPU_MEM : app.ui.memoryOption;
                app.ui.memoryOption = nk_option_label(ctx, "OAM", app.ui.memoryOption == OAM_MEM) ? OAM_MEM : app.ui.memoryOption;
                app.ui.memoryOption = nk_option_label(ctx, "OAM2", app.ui.memoryOption == OAM2_MEM) ? OAM2_MEM : app.ui.memoryOption;

                u16 address = 0x0000;

                nk_label(ctx, "Address: ", NK_TEXT_LEFT);
                nk_edit_string(ctx, NK_EDIT_SIMPLE, app.ui.memoryAddressText, &app.ui.memoryAddressLen, 12, nk_filter_hex);

                if (app.ui.memoryAddressLen > 0)
                {
                    app.ui.memoryAddressText[app.ui.memoryAddressLen] = 0;
                    address = (u16)strtol(app.ui.memoryAddressText, NULL, 16);
                    if (address < 0x0000 || address > 0xFFFF)
                    {
                        address = 0x0000;
                    }
                }

                nk_layout_row_dynamic(ctx, 20, 1);

                if (app.ui.memoryOption == OAM_MEM)
                {
                    for (s32 i = 0; i < 16; ++i)
                    {
                        memset(debugBuffer, 0, sizeof(debugBuffer));

                        s32 col = sprintf(debugBuffer, "%04X: ", i * 16);

                        for (s32 j = 0; j < 16; ++j)
                        {
                            u8 v = ReadU8(&nes->oamMemory, i * 16 + j);
                            col += sprintf(debugBuffer + col, " %02X", v);
                        }

                        nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
                    }
                }
                else if (app.ui.memoryOption == OAM2_MEM)
                {
                    for (s32 i = 0; i < 2; ++i)
                    {
                        memset(debugBuffer, 0, sizeof(debugBuffer));

                        s32 col = sprintf(debugBuffer, "%04X: ", i * 16);

                        for (s32 j = 0; j < 16; ++j)
                        {
                            u8 v = ReadU8(&nes->oamMemory2, i * 16 + j);
                            col += sprintf(debugBuffer + col, " %02X", v);
                        }

                        nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
                    }
                }
                else
                {
                    for (s32 i = 0; i < 16; ++i)
                    {
                        memset(debugBuffer, 0, sizeof(debugBuffer));

                        s32 col = sprintf(debugBuffer, "%04X: ", address + i * 16);

                        for (s32 j = 0; j < 16; ++j)
                        {
                            u8 v = (app.ui.memoryOption == CPU_MEM)
                                ? ReadCPUU8(nes, address + i * 16 + j)
                                : ReadPPUU8(nes, address + i * 16 + j);

                            col += sprintf(debugBuffer + col, " %02X", v);
                        }

                        nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
                    }
                }
            }
            nk_end(ctx);
        }

        if (debugMode)
        {
            if (nk_begin(ctx, "VIDEO", layout.video, flags) && nes)
            {
                PPU *ppu = &nes->ppu;
                GUI *gui = &nes->gui;

                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                const struct nk_input *input = &ctx->input;

                struct nk_rect space;
                enum nk_widget_layout_states state;

                enum options { PATTERNS_PALETTES_OAM, NAMETABLES };
                const float ratio[] = { 200, 200 };

                if (app.ui.videoOption < PATTERNS_PALETTES_OAM || app.ui.videoOption > NAMETABLES)
                {
                    app.ui.videoOption = PATTERNS_PALETTES_OAM;
                }

                nk_layout_row(ctx, NK_STATIC, 25, 2, ratio);
                app.ui.videoOption = nk_option_label(ctx, "PATTERNS, PALETTES, OAM", app.ui.videoOption == PATTERNS_PALETTES_OAM) ? PATTERNS_PALETTES_OAM : app.ui.videoOption;
                app.ui.videoOption = nk_option_label(ctx, "NAMETABLES", app.ui.videoOption == NAMETABLES) ? NAMETABLES : app.ui.videoOption;

                if (app.ui.videoOption == PATTERNS_PALETTES_OAM)
                {
                    // PATTERNS
                    {
                        nk_layout_row_dynamic(ctx, 25, 1);
                        nk_label(ctx, "PATTERNS", NK_TEXT_LEFT);

                        nk_layout_row_static(ctx, 128, 128, 2);

                        for (s32 index = 0; index < 2; ++index)
                        {
                            u16 baseAddress = index * 0x1000;

                            for (s32 tileY = 0; tileY < 16; ++tileY)
                            {
                                for (s32 tileX = 0; tileX < 16; ++tileX)
                                {
                                    u16 patternIndex = tileY * 16 + tileX;

                                    for (s32 y = 0; y < 8; ++y)
                                    {
                                        u8 row1 = ReadPPUU8(nes, baseAddress + patternIndex * 16 + y);
                                        u8 row2 = ReadPPUU8(nes, baseAddress + patternIndex * 16 + 8 + y);

                                        for (s32 x = 0; x < 8; ++x)
                                        {
                                            u8 h = ((row2 >> (7 - x)) & 0x1);
                                            u8 l = ((row1 >> (7 - x)) & 0x1);
                                            u32 paletteIndex = (h << 0x1) | l;
                                            u32 colorIndex = ReadPPUU8(nes, 0x3F00 + paletteIndex);
                                            Color color = systemPalette[colorIndex % 64];

                                            s32 pixelX = (tileX * 8 + x);
                                            s32 pixelY = (tileY * 8 + y);
                                            s32 pixel = pixelY * 128 + pixelX;

                                            gui->patterns[index][pixel] = color;
                                        }
                                    }
                                }
                            }

                            state = nk_widget(&space, ctx);
                            if (state)
                            {
                                if (state != NK_WIDGET_ROM)
                                {
                                    // update_your_widget_by_user_input(...);
                                }

                                glBindTexture(GL_TEXTURE_2D, device.patterns[index].handle.id);
                                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, gui->patterns[index]);
                                glBindTexture(GL_TEXTURE_2D, 0);

                                nk_draw_image(canvas, space, &device.patterns[index], nk_rgb(255, 255, 255));

                                if (nk_input_is_mouse_hovering_rect(input, space))
                                {
                                    if (nk_tooltip_begin(ctx, 200))
                                    {
                                        f32 mouseX = input->mouse.pos.x - space.x;
                                        f32 mouseY = input->mouse.pos.y - space.y;
                                        s32 tileX = mouseX / 8;
                                        s32 tileY = mouseY / 8;

                                        u16 patternIndex = tileY * 16 + tileX;

                                        for (s32 y = 0; y < 8; ++y)
                                        {
                                            u8 row1 = ReadPPUU8(nes, baseAddress + patternIndex * 16 + y);
                                            u8 row2 = ReadPPUU8(nes, baseAddress + patternIndex * 16 + 8 + y);

                                            for (s32 x = 0; x < 8; ++x)
                                            {
                                                u8 h = ((row2 >> (7 - x)) & 0x1);
                                                u8 l = ((row1 >> (7 - x)) & 0x1);
                                                u32 paletteIndex = (h << 0x1) | l;
                                                u32 colorIndex = ReadPPUU8(nes, 0x3F00 + paletteIndex);
                                                Color color = systemPalette[colorIndex % 64];

                                                gui->patternHover[y * 8 + x] = color;
                                            }
                                        }

                                        nk_layout_row_static(ctx, 32, 32, 1);

                                        state = nk_widget(&space, ctx);
                                        if (state)
                                        {
                                            glBindTexture(GL_TEXTURE_2D, device.patternHover.handle.id);
                                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, gui->patternHover);
                                            glBindTexture(GL_TEXTURE_2D, 0);

                                            nk_draw_image(canvas, space, &device.patternHover, nk_rgb(255, 255, 255));
                                        }

                                        nk_layout_row_dynamic(ctx, 20, 1);
                                        nk_label(ctx, DebugText("Address: %04X", baseAddress + patternIndex * 16), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("Table:   %02X", index), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("Tile:    %02X", patternIndex), NK_TEXT_LEFT);

                                        nk_tooltip_end(ctx);
                                    }
                                }
                            }
                        }
                    }

                    // PALETTES
                    {
                        nk_layout_row_dynamic(ctx, 25, 1);
                        nk_label(ctx, "PALETTES", NK_TEXT_LEFT);

                        nk_layout_row_static(ctx, 20, 20, 16);

                        for (s32 index = 0; index < 2; ++index)
                        {
                            for (s32 i = 0; i < 16; ++i)
                            {
                                u16 address = 0x3F00 + (index * 0x10) + i;
                                u8 colorIndex = ReadPPUU8(nes, address);
                                Color color = systemPalette[colorIndex % 64];

                                state = nk_widget(&space, ctx);
                                if (state)
                                {
                                    if (state != NK_WIDGET_ROM)
                                    {
                                        // update_your_widget_by_user_input(...);
                                    }

                                    nk_fill_rect(canvas, space, 0, nk_rgb(color.r, color.g, color.b));

                                    if (nk_input_is_mouse_hovering_rect(input, space))
                                    {
                                        if (nk_tooltip_begin(ctx, 200))
                                        {
                                            nk_layout_row_static(ctx, 32, 32, 1);

                                            state = nk_widget(&space, ctx);
                                            if (state)
                                            {
                                                nk_fill_rect(canvas, space, 0, nk_rgb(color.r, color.g, color.b));
                                            }

                                            nk_layout_row_dynamic(ctx, 20, 1);
                                            nk_label(ctx, DebugText("Address: %04X", address), NK_TEXT_LEFT);
                                            nk_label(ctx, DebugText("Color:   %02X", colorIndex), NK_TEXT_LEFT);
                                            nk_label(ctx, DebugText("Offset:  %d", i % 4), NK_TEXT_LEFT);

                                            nk_tooltip_end(ctx);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // OAM
                    {
                        nk_layout_row_dynamic(ctx, 25, 1);
                        nk_label(ctx, "OAM", NK_TEXT_LEFT);

                        u8 spriteAddr = GetBitFlag(ppu->control, SPRITE_ADDR_FLAG);
                        u8 spriteSize = 8 * (GetBitFlag(ppu->control, SPRITE_SIZE_FLAG) + 1);
                        u16 baseAddress = 0x1000 * spriteAddr;

                        nk_layout_row_static(ctx, 16, 8, 16);

                        for (s32 index = 0; index < 64; ++index)
                        {
                            u8 spriteY = ReadU8(&nes->oamMemory, index * 4 + 0);
                            u8 spriteIdx = ReadU8(&nes->oamMemory, index * 4 + 1);
                            u8 spriteAttr = ReadU8(&nes->oamMemory, index * 4 + 2);
                            u8 spriteX = ReadU8(&nes->oamMemory, index * 4 + 3);

                            if (spriteSize == 16)
                            {
                                // if the sprite size is 8x16 the pattern table is given by bit 0 of spriteIdx
                                baseAddress = 0x1000 * (spriteIdx & 1);
                                spriteIdx &= 0xFE;
                            }

                            // the bits 0-1 indicate the high bits of the pixel color
                            u8 pixelHighBits = spriteAttr & 0x03;

                            // the bit 7 indicate that the sprite should flip vertically
                            b32 flipV = spriteAttr & 0x80;

                            // the bit 6 indicate that the sprite should flip horizontally
                            b32 flipH = spriteAttr & 0x40;

                            for (s32 y = 0; y < spriteSize; ++y)
                            {
                                u8 rowOffset = y;

                                if (flipV)
                                {
                                    rowOffset = (spriteSize - 1) - rowOffset;
                                }

                                u8 row1 = GetSpritePixelRow(nes, baseAddress, spriteIdx, rowOffset, 0);
                                u8 row2 = GetSpritePixelRow(nes, baseAddress, spriteIdx, rowOffset, 1);

                                for (s32 x = 0; x < 8; ++x)
                                {
                                    u8 colOffset = x;

                                    if (flipH)
                                    {
                                        colOffset = 7 - colOffset;
                                    }

                                    u32 paletteIndex = GetPixelColorBits(row1, row2, colOffset, pixelHighBits);
                                    u32 colorIndex = ReadPPUU8(nes, 0x3F10 + paletteIndex);
                                    Color color = systemPalette[colorIndex % 64];

                                    s32 pixelIndex = y * 8 + x;
                                    gui->sprites[index][pixelIndex] = color;
                                }
                            }

                            state = nk_widget(&space, ctx);
                            if (state)
                            {
                                if (state != NK_WIDGET_ROM)
                                {
                                    // update_your_widget_by_user_input(...);
                                }

                                glBindTexture(GL_TEXTURE_2D, device.oam[index].handle.id);
                                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 16, GL_RGBA, GL_UNSIGNED_BYTE, gui->sprites[index]);
                                glBindTexture(GL_TEXTURE_2D, 0);

                                nk_draw_image(canvas, space, &device.oam[index], nk_rgb(255, 255, 255));

                                if (nk_input_is_mouse_hovering_rect(input, space))
                                {
                                    if (nk_tooltip_begin(ctx, 200))
                                    {
                                        nk_layout_row_static(ctx, 64, 32, 1);

                                        state = nk_widget(&space, ctx);
                                        if (state)
                                        {
                                            nk_draw_image(canvas, space, &device.oam[index], nk_rgb(255, 255, 255));
                                        }

                                        nk_layout_row_dynamic(ctx, 20, 2);
                                        nk_label(ctx, DebugText("Number: %02X", index), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("Tile:   %02X", spriteIdx), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("X:      %02X", spriteX), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("Color:  %02X", pixelHighBits), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("Y:      %02X", spriteY), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("Flags:  %s%s", (flipV ? "V" : ""), (flipH ? "H" : "")), NK_TEXT_LEFT);

                                        nk_tooltip_end(ctx);
                                    }
                                }
                            }
                        }
                    }

                    // OAM 2
                    {
                        u8 spriteAddr = GetBitFlag(ppu->control, SPRITE_ADDR_FLAG);
                        u8 spriteSize = 8 * (GetBitFlag(ppu->control, SPRITE_SIZE_FLAG) + 1);
                        u16 baseAddress = 0x1000 * spriteAddr;

                        nk_layout_row_static(ctx, 16, 8, 8);

                        for (s32 index = 0; index < 8; ++index)
                        {
                            u8 spriteY = ReadU8(&nes->oamMemory2, index * 4 + 0);
                            u8 spriteIdx = ReadU8(&nes->oamMemory2, index * 4 + 1);
                            u8 spriteAttr = ReadU8(&nes->oamMemory2, index * 4 + 2);
                            u8 spriteX = ReadU8(&nes->oamMemory2, index * 4 + 3);

                            if (spriteSize == 16)
                            {
                                // if the sprite size is 8x16 the pattern table is given by bit 0 of spriteIdx
                                baseAddress = 0x1000 * (spriteIdx & 1);
                                spriteIdx &= 0xFE;
                            }

                            // the bits 0-1 indicate the high bits of the pixel color
                            u8 pixelHighBits = spriteAttr & 0x03;

                            // the bit 7 indicate that the sprite should flip vertically
                            b32 flipV = spriteAttr & 0x80;

                            // the bit 6 indicate that the sprite should flip horizontally
                            b32 flipH = spriteAttr & 0x40;

                            for (s32 y = 0; y < spriteSize; ++y)
                            {
                                u8 rowOffset = y;

                                // the bit 7 indicate that the sprite should flip vertically
                                if (flipV)
                                {
                                    rowOffset = (spriteSize - 1) - rowOffset;
                                }

                                u8 row1 = GetSpritePixelRow(nes, baseAddress, spriteIdx, rowOffset, 0);
                                u8 row2 = GetSpritePixelRow(nes, baseAddress, spriteIdx, rowOffset, 1);

                                for (s32 x = 0; x < 8; ++x)
                                {
                                    u8 colOffset = x;

                                    // the bit 6 indicate that the sprite should flip horizontally
                                    if (flipH)
                                    {
                                        colOffset = 7 - colOffset;
                                    }

                                    u32 paletteIndex = GetPixelColorBits(row1, row2, colOffset, pixelHighBits);
                                    u32 colorIndex = ReadPPUU8(nes, 0x3F10 + paletteIndex);
                                    Color color = systemPalette[colorIndex % 64];

                                    s32 pixelIndex = y * 8 + x;
                                    gui->sprites2[index][pixelIndex] = color;
                                }
                            }

                            state = nk_widget(&space, ctx);
                            if (state)
                            {
                                if (state != NK_WIDGET_ROM)
                                {
                                    // update_your_widget_by_user_input(...);
                                }

                                glBindTexture(GL_TEXTURE_2D, device.oam2[index].handle.id);
                                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 16, GL_RGBA, GL_UNSIGNED_BYTE, gui->sprites2[index]);
                                glBindTexture(GL_TEXTURE_2D, 0);

                                nk_draw_image(canvas, space, &device.oam2[index], nk_rgb(255, 255, 255));

                                if (nk_input_is_mouse_hovering_rect(input, space))
                                {
                                    if (nk_tooltip_begin(ctx, 200))
                                    {
                                        nk_layout_row_static(ctx, 64, 32, 1);

                                        state = nk_widget(&space, ctx);
                                        if (state)
                                        {
                                            nk_draw_image(canvas, space, &device.oam2[index], nk_rgb(255, 255, 255));
                                        }

                                        nk_layout_row_dynamic(ctx, 20, 2);
                                        nk_label(ctx, DebugText("Number: %02X", index), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("Tile:   %02X", spriteIdx), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("X:      %02X", spriteX), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("Color:  %02X", pixelHighBits), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("Y:      %02X", spriteY), NK_TEXT_LEFT);
                                        nk_label(ctx, DebugText("Flags:  %s%s", (flipV ? "V" : ""), (flipH ? "H" : "")), NK_TEXT_LEFT);

                                        nk_tooltip_end(ctx);
                                    }
                                }
                            }
                        }
                    }
                }
                else if (app.ui.videoOption == NAMETABLES)
                {
                    enum options { H2000, H2400, H2800, H2C00 };
                    const float ratio[] = { 80, 80, 80, 80, 80 };

                    if (app.ui.nametableOption < H2000 || app.ui.nametableOption > H2C00)
                    {
                        app.ui.nametableOption = H2000;
                    }

                    nk_layout_row(ctx, NK_STATIC, 25, 5, ratio);
                    app.ui.nametableOption = nk_option_label(ctx, "$2000", app.ui.nametableOption == H2000) ? H2000 : app.ui.nametableOption;
                    app.ui.nametableOption = nk_option_label(ctx, "$2400", app.ui.nametableOption == H2400) ? H2400 : app.ui.nametableOption;
                    app.ui.nametableOption = nk_option_label(ctx, "$2800", app.ui.nametableOption == H2800) ? H2800 : app.ui.nametableOption;
                    app.ui.nametableOption = nk_option_label(ctx, "$2C00", app.ui.nametableOption == H2C00) ? H2C00 : app.ui.nametableOption;

                    nk_checkbox_label(ctx, "PIX", &app.ui.showSeparatePixels);

                    u16 address = 0x2000 + app.ui.nametableOption * 0x400;

                    if (app.ui.showSeparatePixels)
                    {
                        nk_layout_row_static(ctx, 8, 8, 32);

                        u16 baseAddress = 0x1000 * GetBitFlag(ppu->control, BACKGROUND_ADDR_FLAG);

                        for (s32 tileY = 0; tileY < 30; ++tileY)
                        {
                            for (s32 tileX = 0; tileX < 32; ++tileX)
                            {
                                u16 tileIndex = tileY * 32 + tileX;
                                u8 patternIndex = ReadPPUU8(nes, address + tileIndex);

                                u16 attributeX = tileX / 4;
                                u16 attributeOffsetX = tileX % 4;

                                u16 attributeY = tileY / 4;
                                u16 attributeOffsetY = tileY % 4;

                                u16 attributeIndex = attributeY * 8 + attributeX;
                                u8 attributeByte = ReadPPUU8(nes, address + 0x3C0 + attributeIndex);

                                // lookupValue is a number between 0 and 15,
                                // for value 0x00, 0x01, 0x02, 0x03, we need to get the bits 00000011
                                // for value 0x04, 0x05, 0x06, 0x07, we need to get the bits 00001100
                                // for value 0x08, 0x09, 0x0A, 0x0B, we need to get the bits 00110000
                                // for value 0x0C, 0x0D, 0x0E, 0x0F, we need to get the bits 11000000
                                //
                                u8 lookupValue = attributeTableLookup[attributeOffsetY][attributeOffsetX];
                                u8 shiftValue = (lookupValue / 4) * 2;
                                u8 highColorBits = (attributeByte >> shiftValue) & 0x03;

                                for (s32 y = 0; y < 8; ++y)
                                {
                                    u8 row1 = ReadPPUU8(nes, baseAddress + patternIndex * 16 + y);
                                    u8 row2 = ReadPPUU8(nes, baseAddress + patternIndex * 16 + 8 + y);

                                    for (s32 x = 0; x < 8; ++x)
                                    {
                                        u8 h = ((row2 >> (7 - x)) & 0x1);
                                        u8 l = ((row1 >> (7 - x)) & 0x1);
                                        u8 v = (h << 0x1) | l;

                                        u32 paletteIndex = (highColorBits << 2) | v;
                                        u32 colorIndex = ReadPPUU8(nes, 0x3F00 + paletteIndex);

                                        // if the grayscale bit is set, then AND (&) with 0x30 to set
                                        // any color in the palette to the grey ones
                                        if (GetBitFlag(ppu->mask, COLOR_FLAG))
                                        {
                                            colorIndex &= 0x30;
                                        }

                                        Color color = systemPalette[colorIndex % 64];

                                        // check the bits 5, 6, 7 to color emphasis
                                        u8 colorMask = (ppu->mask & 0xE0) >> 5;
                                        if (colorMask != 0)
                                        {
                                            ColorEmphasis(&color, colorMask);
                                        }

                                        s32 pixel = y * 8 + x;
                                        gui->nametable2[tileX][tileY][pixel] = color;
                                    }
                                }

                                state = nk_widget(&space, ctx);
                                if (state)
                                {
                                    if (state != NK_WIDGET_ROM)
                                    {
                                        // update_your_widget_by_user_input(...);
                                    }

                                    glBindTexture(GL_TEXTURE_2D, device.nametable2[tileY * 32 + tileX].handle.id);
                                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, gui->nametable2[tileX][tileY]);
                                    glBindTexture(GL_TEXTURE_2D, 0);

                                    nk_draw_image(canvas, space, &device.nametable2[tileY * 32 + tileX], nk_rgb(255, 255, 255));
                                }
                            }
                        }
                    }
                    else
                    {
                        nk_layout_row_static(ctx, 240, 256, 1);

                        u16 backgroundBaseAddress = 0x1000 * GetBitFlag(ppu->control, BACKGROUND_ADDR_FLAG);

                        for (s32 tileY = 0; tileY < 30; ++tileY)
                        {
                            for (s32 tileX = 0; tileX < 32; ++tileX)
                            {
                                u16 tileIndex = tileY * 32 + tileX;
                                u8 patternIndex = ReadPPUU8(nes, address + tileIndex);

                                u16 attributeX = tileX / 4;
                                u16 attributeOffsetX = tileX % 4;

                                u16 attributeY = tileY / 4;
                                u16 attributeOffsetY = tileY % 4;

                                u16 attributeIndex = attributeY * 8 + attributeX;
                                u8 attributeByte = ReadPPUU8(nes, address + 0x3C0 + attributeIndex);

                                // lookupValue is a number between 0 and 15,
                                // for value 0x00, 0x01, 0x02, 0x03, we need to get the bits 00000011
                                // for value 0x04, 0x05, 0x06, 0x07, we need to get the bits 00001100
                                // for value 0x08, 0x09, 0x0A, 0x0B, we need to get the bits 00110000
                                // for value 0x0C, 0x0D, 0x0E, 0x0F, we need to get the bits 11000000
                                //
                                u8 lookupValue = attributeTableLookup[attributeOffsetY][attributeOffsetX];
                                u8 shiftValue = (lookupValue / 4) * 2;
                                u8 highColorBits = (attributeByte >> shiftValue) & 0x03;

                                for (s32 y = 0; y < 8; ++y)
                                {
                                    u8 row1 = ReadPPUU8(nes, backgroundBaseAddress + patternIndex * 16 + y);
                                    u8 row2 = ReadPPUU8(nes, backgroundBaseAddress + patternIndex * 16 + 8 + y);

                                    for (s32 x = 0; x < 8; ++x)
                                    {
                                        u8 h = ((row2 >> (7 - x)) & 0x1);
                                        u8 l = ((row1 >> (7 - x)) & 0x1);
                                        u8 v = (h << 0x1) | l;

                                        u32 paletteIndex = (highColorBits << 2) | v;
                                        u32 colorIndex = ReadPPUU8(nes, 0x3F00 + paletteIndex);

                                        // if the grayscale bit is set, then AND (&) with 0x30 to set
                                        // any color in the palette to the grey ones
                                        if (GetBitFlag(ppu->mask, COLOR_FLAG))
                                        {
                                            colorIndex &= 0x30;
                                        }

                                        Color color = systemPalette[colorIndex % 64];

                                        // check the bits 5, 6, 7 to color emphasis
                                        u8 colorMask = (ppu->mask & 0xE0) >> 5;
                                        if (colorMask != 0)
                                        {
                                            ColorEmphasis(&color, colorMask);
                                        }

                                        s32 pixelX = (tileX * 8 + x);
                                        s32 pixelY = (tileY * 8 + y);
                                        s32 pixel = pixelY * 256 + pixelX;
                                        gui->nametable[pixel] = color;
                                    }
                                }
                            }
                        }

                        state = nk_widget(&space, ctx);
                        if (state)
                        {
                            if (state != NK_WIDGET_ROM)
                            {
                                // update_your_widget_by_user_input(...);
                            }

                            glBindTexture(GL_TEXTURE_2D, device.nametable.handle.id);
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGBA, GL_UNSIGNED_BYTE, gui->nametable);
                            glBindTexture(GL_TEXTURE_2D, 0);

                            nk_draw_image(canvas, space, &device.nametable, nk_rgb(255, 255, 255));
                        }
                    }
                }
            }
            nk_end(ctx);
        }

        if (debugMode)
        {
            if (nk_begin(ctx, "AUDIO", layout.audio, flags | NK_WINDOW_SCALABLE) && nes)
            {
                APU *apu = &nes->apu;

                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

                struct nk_rect space;
                enum nk_widget_layout_states state;

                enum options { GENERAL, SQUARE1, SQUARE2, TRIANGLE, NOISE, DMC, BUFFER };
                const float ratio[] = { 100, 100, 100, 100, 100 };

                if (app.ui.audioOption < GENERAL || app.ui.audioOption > BUFFER)
                {
                    app.ui.audioOption = GENERAL;
                }

                nk_layout_row(ctx, NK_STATIC, 25, 5, ratio);
                app.ui.audioOption = nk_option_label(ctx, "GENERAL", app.ui.audioOption == GENERAL) ? GENERAL : app.ui.audioOption;
                app.ui.audioOption = nk_option_label(ctx, "SQUARE 1", app.ui.audioOption == SQUARE1) ? SQUARE1 : app.ui.audioOption;
                app.ui.audioOption = nk_option_label(ctx, "SQUARE 2", app.ui.audioOption == SQUARE2) ? SQUARE2 : app.ui.audioOption;
                app.ui.audioOption = nk_option_label(ctx, "TRIANGLE", app.ui.audioOption == TRIANGLE) ? TRIANGLE : app.ui.audioOption;
                app.ui.audioOption = nk_option_label(ctx, "NOISE", app.ui.audioOption == NOISE) ? NOISE : app.ui.audioOption;
                app.ui.audioOption = nk_option_label(ctx, "DMC", app.ui.audioOption == DMC) ? DMC : app.ui.audioOption;
                app.ui.audioOption = nk_option_label(ctx, "BUFFER", app.ui.audioOption == BUFFER) ? BUFFER : app.ui.audioOption;

                if (app.ui.audioOption == GENERAL)
                {
                    nk_layout_row_dynamic(ctx, 25, 3);

                    nk_label(ctx, DebugText("CYCLES:%lld", apu->cycles), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("FRAME MODE:%02X", apu->frameMode), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SAMPLE RATE:%04X", APU_SAMPLES_PER_SECOND), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("FRAME IRQ:%02X", !apu->inhibitIRQ), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("FRAME VALUE:%02X", apu->frameValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SAMPLE COUNTER:%02X", apu->sampleCounter), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("DMC IRQ:%02X", apu->dmcIRQ), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("FRAME COUNTER:%02X", apu->frameCounter), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("BUFFER INDEX:%04X", apu->bufferIndex), NK_TEXT_LEFT);

                    nk_checkbox_label(ctx, "SQUARE1 ENABLED", &app.ui.square1Enabled);
                    nk_checkbox_label(ctx, "SQUARE2 ENABLED", &app.ui.square2Enabled);
                    nk_checkbox_label(ctx, "TRIANGLE ENABLED", &app.ui.triangleEnabled);
                    nk_checkbox_label(ctx, "NOISE ENABLED", &app.ui.noiseEnabled);
                    nk_checkbox_label(ctx, "DMC ENABLED", &app.ui.dmcEnabled);

                    apu->pulse1.globalEnabled = app.ui.square1Enabled;
                    apu->pulse2.globalEnabled = app.ui.square2Enabled;
                    apu->triangle.globalEnabled = app.ui.triangleEnabled;
                    apu->noise.globalEnabled = app.ui.noiseEnabled;
                    apu->dmc.globalEnabled = app.ui.dmcEnabled;

                    const f32 rectHeight = 200.0f;
                    struct nk_color lineColor = nk_rgb(255, 0, 0);
                    const f32 lineThickness = 1.0f;

                    nk_layout_row_dynamic(ctx, rectHeight, 1);

                    state = nk_widget(&space, ctx);
                    if (state)
                    {
                        if (state != NK_WIDGET_ROM)
                        {
                            // update_your_widget_by_user_input(...);
                        }

                        s32 pointCount = apu->bufferIndex;
                        struct nk_vec2 *points = (struct nk_vec2*) Allocate(pointCount * sizeof(struct nk_vec2));

                        f32 horizontalSpacing = space.w / pointCount;

                        for (s32 i = 0; i < pointCount; i++)
                        {
                            f32 x = horizontalSpacing * i;
                            f32 y = rectHeight - apu->buffer[i] * rectHeight / APU_AMPLIFIER_VALUE;
                            *(points + i) = nk_vec2(space.x + x, space.y + y);
                        }

                        nk_stroke_rect(canvas, space, 0, 2, nk_rgb(0x41, 0x41, 0x41));
                        nk_stroke_polyline(canvas, (f32*)points, pointCount, lineThickness, lineColor);

                        Free(points);
                    }
                }
                else if (app.ui.audioOption == SQUARE1 || app.ui.audioOption == SQUARE2)
                {
                    APUPulse *pulse = app.ui.audioOption == SQUARE1 ? &apu->pulse1 : &apu->pulse2;

                    nk_layout_row_dynamic(ctx, 25, 4);

                    nk_label(ctx, DebugText("ENVELOPE ENABLED:%02X", pulse->envelopeEnabled), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SWEEP ENABLED:%02X", pulse->sweepEnabled), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("LEN ENABLED:%02X", pulse->lengthEnabled), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("ENABLED:%02X", pulse->enabled), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("ENVELOPE LOOP:%02X", pulse->envelopeLoop), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SWEEP NEGATE:%02X", pulse->sweepNegate), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("LEN VALUE:%02X", pulse->lengthValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("CHANNEL:%02X", pulse->channel), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("ENVELOPE PERIOD:%02X", pulse->envelopePeriod), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SWEEP SHIFT:%02X", pulse->sweepShift), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("TIMER PERIOD:%02X", pulse->timerPeriod), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("DUTY MODE:%02X", pulse->dutyMode), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("ENVELOPE VALUE:%02X", pulse->envelopeValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SWEEP PERIOD:%02X", pulse->sweepPeriod), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("TIMER VALUE:%02X", pulse->timerValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("DUTY VALUE:%02X", pulse->dutyValue), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("ENVELOPE VOLUME:%02X", pulse->envelopeVolume), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SWEEP VALUE:%02X", pulse->sweepValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("CONSTANT VOLUME:%02X", pulse->constantVolume), NK_TEXT_LEFT);

                    const f32 rectHeight = 200.0f;
                    struct nk_color lineColor = nk_rgb(255, 0, 0);
                    const f32 lineThickness = 1.0f;

                    nk_layout_row_dynamic(ctx, rectHeight, 1);

                    state = nk_widget(&space, ctx);
                    if (state)
                    {
                        if (state != NK_WIDGET_ROM)
                        {
                            // update_your_widget_by_user_input(...);
                        }

                        s32 pointCount = pulse->bufferIndex;
                        struct nk_vec2 *points = (struct nk_vec2*) Allocate(pointCount * sizeof(struct nk_vec2));

                        f32 horizontalSpacing = space.w / pointCount;

                        for (s32 i = 0; i < pointCount; i++)
                        {
                            f32 x = horizontalSpacing * i;
                            f32 y = rectHeight - pulse->buffer[i] * rectHeight / APU_AMPLIFIER_VALUE;
                            *(points + i) = nk_vec2(space.x + x, space.y + y);
                        }

                        nk_stroke_rect(canvas, space, 0, 2, nk_rgb(0x41, 0x41, 0x41));
                        nk_stroke_polyline(canvas, (f32*)points, pointCount, lineThickness, lineColor);

                        Free(points);
                    }
                }
                else if (app.ui.audioOption == TRIANGLE)
                {
                    APUTriangle *triangle = &apu->triangle;

                    nk_layout_row_dynamic(ctx, 25, 3);

                    nk_label(ctx, DebugText("COUNTER ENABLED:%02X", triangle->linearEnabled), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("LEN ENABLED:%02X", triangle->lengthEnabled), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("TIMER PERIOD:%02X", triangle->timerPeriod), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("COUNTER PERIOD:%02X", triangle->linearPeriod), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("LEN VALUE:%02X", triangle->lengthValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("TIMER VALUE:%02X", triangle->timerValue), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("COUNTER VALUE:%02X", triangle->linearValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("TABLE INDEX:%02X", triangle->timerValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("ENABLED:%02X", triangle->enabled), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("COUNTER RELOAD:%02X", triangle->linearReload), NK_TEXT_LEFT);

                    const f32 rectHeight = 200.0f;
                    struct nk_color lineColor = nk_rgb(255, 0, 0);
                    const f32 lineThickness = 1.0f;

                    nk_layout_row_dynamic(ctx, rectHeight, 1);

                    state = nk_widget(&space, ctx);
                    if (state)
                    {
                        if (state != NK_WIDGET_ROM)
                        {
                            // update_your_widget_by_user_input(...);
                        }

                        s32 pointCount = triangle->bufferIndex;
                        struct nk_vec2 *points = (struct nk_vec2*) Allocate(pointCount * sizeof(struct nk_vec2));

                        f32 horizontalSpacing = space.w / pointCount;

                        for (s32 i = 0; i < pointCount; i++)
                        {
                            f32 x = horizontalSpacing * i;
                            f32 y = rectHeight - triangle->buffer[i] * rectHeight / APU_AMPLIFIER_VALUE;
                            *(points + i) = nk_vec2(space.x + x, space.y + y);
                        }

                        nk_stroke_rect(canvas, space, 0, 2, nk_rgb(0x41, 0x41, 0x41));
                        nk_stroke_polyline(canvas, (f32*)points, pointCount, lineThickness, lineColor);

                        Free(points);
                    }
                }
                else if (app.ui.audioOption == NOISE)
                {
                    APUNoise *noise = &apu->noise;

                    nk_layout_row_dynamic(ctx, 25, 3);

                    nk_label(ctx, DebugText("ENVELOPE ENABLED:%02X", noise->envelopeEnabled), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("ENVELOPE VALUE:%02X", noise->envelopeValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("LEN ENABLED:%02X", noise->lengthEnabled), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("ENVELOPE LOOP:%02X", noise->envelopeLoop), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("TIMER PERIOD:%02X", noise->timerPeriod), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("LEN VALUE:%02X", noise->lengthValue), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("ENVELOPE PERIOD:%02X", noise->envelopePeriod), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("TIMER VALUE:%02X", noise->timerValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("ENABLED:%02X", noise->enabled), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("ENVELOPE VOLUME:%02X", noise->envelopeVolume), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("CONSTANT VOLUME:%02X", noise->constantVolume), NK_TEXT_LEFT);

                    const f32 rectHeight = 200.0f;
                    struct nk_color lineColor = nk_rgb(255, 0, 0);
                    const f32 lineThickness = 1.0f;

                    nk_layout_row_dynamic(ctx, rectHeight, 1);

                    state = nk_widget(&space, ctx);
                    if (state)
                    {
                        if (state != NK_WIDGET_ROM)
                        {
                            // update_your_widget_by_user_input(...);
                        }

                        s32 pointCount = noise->bufferIndex;
                        struct nk_vec2 *points = (struct nk_vec2*) Allocate(pointCount * sizeof(struct nk_vec2));

                        f32 horizontalSpacing = space.w / pointCount;

                        for (s32 i = 0; i < pointCount; i++)
                        {
                            f32 x = horizontalSpacing * i;
                            f32 y = rectHeight - noise->buffer[i] * rectHeight / APU_AMPLIFIER_VALUE;
                            *(points + i) = nk_vec2(space.x + x, space.y + y);
                        }

                        nk_stroke_rect(canvas, space, 0, 2, nk_rgb(0x41, 0x41, 0x41));
                        nk_stroke_polyline(canvas, (f32*)points, pointCount, lineThickness, lineColor);

                        Free(points);
                    }
                }
                else if (app.ui.audioOption == DMC)
                {
                    APUDMC *dmc = &apu->dmc;

                    nk_layout_row_dynamic(ctx, 25, 3);

                    nk_label(ctx, DebugText("SAMPLE ADDRESS:%02X", dmc->sampleAddress), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("CURRENT ADDRESS:%02X", dmc->currentAddress), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("TIMER PERIOD:%02X", dmc->timerPeriod), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("SAMPLE LENGTH:%02X", dmc->sampleLength), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("CURRENT LENGTH:%02X", dmc->currentLength), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("TIMER VALUE:%02X", dmc->timerValue), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("SHIFT REGISTER:%02X", dmc->shiftRegister), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("BIT COUNT:%02X", dmc->bitCount), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("VALUE:%02X", dmc->value), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("IRQ:%02X", dmc->irq), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("LOOP:%02X", dmc->loop), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("ENABLED:%02X", dmc->enabled), NK_TEXT_LEFT);

                    const f32 rectHeight = 200.0f;
                    struct nk_color lineColor = nk_rgb(255, 0, 0);
                    const f32 lineThickness = 1.0f;

                    nk_layout_row_dynamic(ctx, rectHeight, 1);

                    state = nk_widget(&space, ctx);
                    if (state)
                    {
                        if (state != NK_WIDGET_ROM)
                        {
                            // update_your_widget_by_user_input(...);
                        }

                        s32 pointCount = dmc->bufferIndex;
                        struct nk_vec2 *points = (struct nk_vec2*) Allocate(pointCount * sizeof(struct nk_vec2));

                        f32 horizontalSpacing = space.w / pointCount;

                        for (s32 i = 0; i < pointCount; i++)
                        {
                            f32 x = horizontalSpacing * i;
                            f32 y = rectHeight - dmc->buffer[i] * rectHeight / APU_AMPLIFIER_VALUE;
                            *(points + i) = nk_vec2(space.x + x, space.y + y);
                        }

                        nk_stroke_rect(canvas, space, 0, 2, nk_rgb(0x41, 0x41, 0x41));
                        nk_stroke_polyline(canvas, (f32*)points, pointCount, lineThickness, lineColor);

                        Free(points);
                    }
                }
                else if (app.ui.audioOption == BUFFER)
                {
                    nk_layout_row_dynamic(ctx, 20, 1);

                    for (s32 i = 0; i < APU_BUFFER_LENGTH / 16; ++i)
                    {
                        memset(debugBuffer, 0, sizeof(debugBuffer));

                        s32 col = 0;

                        for (s32 j = 0; j < 16; ++j)
                        {
                            f32 v = apu->buffer[i * 16 + j];

                            if (j > 0)
                            {
                                col += sprintf(debugBuffer + col, " ");
                            }

                            col += sprintf(debugBuffer + col, "%.2f", v);
                        }

                        nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
                    }
                }
            }
            nk_end(ctx);
        }

        /* Draw */
        {
            SDL_GetWindowSize(win, &win_width, &win_height);
            glViewport(0, 0, win_width, win_height);
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(0, 0, 0, 1);
            /* IMPORTANT: `nk_sdl_render` modifies some global OpenGL state
            * with blending, scissor, face culling, depth test and viewport and
            * defaults everything back into a default state.
            * Make sure to either a.) save and restore or b.) reset your own state after
            * rendering the UI. */
            nk_sdl_render(NK_ANTI_ALIASING_ON);
            SDL_GL_SwapWindow(win);
        }

        e = SDL_GetPerformanceCounter();
        d2 = GetSecondsElapsed(s, e);

        u64 endCounter = SDL_GetPerformanceCounter();
        f32 secondsElapsed = GetSecondsElapsed(startCounter, endCounter);

        while (secondsElapsed < 0.0167)
        {
            endCounter = SDL_GetPerformanceCounter();
            secondsElapsed = GetSecondsElapsed(startCounter, endCounter);
        }

        dt = GetSecondsElapsed(startCounter, endCounter);
        startCounter = endCounter;
    }

    if (dev)
    {
        SDL_CloseAudioDevice(dev);
    }

    if (audioStream)
    {
        SDL_FreeAudioStream(audioStream);
    }

    if (controller)
    {
        SDL_GameControllerClose(controller);
    }

    DeleteTextures(&device);
    nk_sdl_shutdown();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
