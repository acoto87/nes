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

#define CLAY_IMPLEMENTATION
#include "../external/clay/clay.h"

/* Clay integration helpers */
global const struct nk_user_font *g_clayNkFont = NULL;

static Clay_Dimensions ClayMeasureText(Clay_StringSlice text, Clay_TextElementConfig *cfg, void *userData)
{
    (void)cfg;
    const struct nk_user_font *font = (const struct nk_user_font *)userData;
    if (!font || text.length <= 0)
        return (Clay_Dimensions){ 0, 0 };
    float w = font->width(font->userdata, font->height, text.chars, text.length);
    return (Clay_Dimensions){ .width = w, .height = font->height };
}

static void ClayHandleError(Clay_ErrorData errorData)
{
    (void)errorData;
    /* In a real app you'd log errorData.errorText.chars here */
}

global Clay_Context *g_clayCtx = NULL;

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
    
    b32 leftSidebarCollapsed;
    b32 rightSidebarCollapsed;
    s32 leftSidebarTab;
    s32 rightSidebarTab;

    s32 fps;
    s32 fpsCount;
    s32 oneCycleToggle;
    s32 debugToggle;

    char instructionAddressText[12];
    s32 instructionAddressLen;
    char instructionBreakpointText[5];
    s32 instructionBreakpointLen;

    s32 debugToolTab;
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
        .leftSidebarTab = 0,
        .rightSidebarTab = 0,
        .videoOption = 0,
        .debugToolTab = 0,
        .debugToggle = FALSE,
        .debugMode = FALSE,
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

typedef struct Device Device;

global PFNGLGENBUFFERSPROC GLGenBuffers;
global PFNGLDELETEBUFFERSPROC GLDeleteBuffers;
global PFNGLBINDBUFFERPROC GLBindBuffer;
global PFNGLGENVERTEXARRAYSPROC GLGenVertexArrays;
global PFNGLDELETEVERTEXARRAYSPROC GLDeleteVertexArrays;
global PFNGLBINDVERTEXARRAYPROC GLBindVertexArray;
global PFNGLGENERATEMIPMAPPROC GLGenerateMipmap;

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

/* ── Clay custom element IDs ─────────────────────────────────────────────── */
#define CLAY_CUSTOM_TOP_BAR       1
#define CLAY_CUSTOM_LEFT_SIDEBAR  2
#define CLAY_CUSTOM_SCREEN        3
#define CLAY_CUSTOM_TOOLS_TABS    4
#define CLAY_CUSTOM_TOOLS_VIEW    5
#define CLAY_CUSTOM_RIGHT_SIDEBAR 6
#define CLAY_CUSTOM_STATUS_BAR    7

/* ── Palette ─────────────────────────────────────────────────────────────── */
#define COL_BG         nk_rgba(0x0D,0x0D,0x0D,255)
#define COL_PANEL      nk_rgba(0x11,0x11,0x11,255)
#define COL_PANEL_ALT  nk_rgba(0x14,0x14,0x14,255)
#define COL_BORDER     nk_rgba(0x22,0x22,0x22,255)
#define COL_BORDER_HI  nk_rgba(0x2A,0x2A,0x2A,255)
#define COL_TEXT       nk_rgba(0xC8,0xC8,0xC8,255)
#define COL_TEXT_DIM   nk_rgba(0x66,0x66,0x66,255)
#define COL_TEXT_HI    nk_rgba(0xEF,0xEF,0xEF,255)
#define COL_ACCENT     nk_rgba(0x33,0xFF,0x66,255)
#define COL_ACCENT_DK  nk_rgba(0x1A,0x8C,0x3A,255)
#define COL_ACCENT_DIM nk_rgba(0x0D,0x3D,0x1F,255)
#define COL_BTN        nk_rgba(0x1A,0x1A,0x1A,255)

/* Clay color helper */
static Clay_Color ClayRGBA(u8 r, u8 g, u8 b, u8 a) { Clay_Color c; c.r=(float)r; c.g=(float)g; c.b=(float)b; c.a=(float)a; return c; }
#define CLAY_COL_BG          ClayRGBA(0x0D,0x0D,0x0D,255)
#define CLAY_COL_PANEL       ClayRGBA(0x11,0x11,0x11,255)
#define CLAY_COL_PANEL_ALT   ClayRGBA(0x14,0x14,0x14,255)
#define CLAY_COL_BORDER      ClayRGBA(0x22,0x22,0x22,255)
#define CLAY_COL_BORDER_HI   ClayRGBA(0x2A,0x2A,0x2A,255)
#define CLAY_COL_TEXT        ClayRGBA(0xC8,0xC8,0xC8,255)
#define CLAY_COL_TEXT_DIM    ClayRGBA(0x66,0x66,0x66,255)
#define CLAY_COL_ACCENT      ClayRGBA(0x33,0xFF,0x66,255)
#define CLAY_COL_ACCENT_DIM  ClayRGBA(0x0D,0x3D,0x1F,255)
#define CLAY_COL_BTN         ClayRGBA(0x1A,0x1A,0x1A,255)
#define CLAY_COL_STATUS_BG   ClayRGBA(0x33,0xFF,0x66,255)

/* ── Forward declarations for interactive content drawers ─────────────────── */
internal void DrawInstructionsContent(struct nk_context *ctx, Device *device);
internal void DrawMemoryContent(struct nk_context *ctx);
internal void DrawVideoContent(struct nk_context *ctx, struct Device *device, const struct nk_input *input);
internal void DrawAudioContent(struct nk_context *ctx);

/* ── Panel header helper ─────────────────────────────────────────────────── */
internal void DrawPanelHeader(struct nk_context *ctx, const char *label)
{
    struct nk_color accent = COL_ACCENT;
    struct nk_color dim    = COL_BORDER_HI;
    nk_layout_row_template_begin(ctx, 18);
    nk_layout_row_template_push_static(ctx, 160);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_end(ctx);

    nk_label_colored(ctx, label, NK_TEXT_LEFT, accent);
    nk_label_colored(ctx, "──────────────────────────────────────────", NK_TEXT_LEFT, dim);
}

/* ── BuildClayLayout: declare the full UI tree, return render commands ─────── */
internal Clay_RenderCommandArray BuildClayLayout(s32 winWidth, s32 winHeight, f32 dt)
{
    float w = (float)winWidth;
    float h = (float)winHeight;

    Clay_SetLayoutDimensions((Clay_Dimensions){ w, h });

    float topBarH    = 36.0f;
    float statusBarH = 28.0f;
    float gap        = 6.0f;
    float colLeftW   = app.ui.leftSidebarCollapsed  ? 40.0f  : 200.0f;
    float colRightW  = app.ui.rightSidebarCollapsed ? 40.0f  : 260.0f;

    (void)dt;

    Clay_BeginLayout();

    /* Root: full window, top-to-bottom */
    CLAY(CLAY_ID("Root"), {
        .layout = {
            .sizing          = { CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h) },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = CLAY_COL_BG,
    }) {

        /* ── Top bar ── */
        CLAY(CLAY_ID("TopBar"), {
            .layout = {
                .sizing  = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(topBarH) },
            },
            .backgroundColor = CLAY_COL_PANEL,
            .border = { .color = CLAY_COL_BORDER, .width = { .bottom = 1 } },
            .custom = { .customData = (void*)(intptr_t)CLAY_CUSTOM_TOP_BAR },
        }) {}

        if (debugMode)
        {
            /* ── Workspace row ── */
            CLAY(CLAY_ID("Workspace"), {
                .layout = {
                    .sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .padding         = { (uint16_t)gap, (uint16_t)gap, (uint16_t)gap, (uint16_t)gap },
                    .childGap        = (uint16_t)gap,
                },
                .backgroundColor = CLAY_COL_BG,
            }) {

                /* Left sidebar */
                CLAY(CLAY_ID("LeftSidebar"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_FIXED(colLeftW), CLAY_SIZING_GROW(0) },
                    },
                    .backgroundColor = CLAY_COL_PANEL,
                    .border = { .color = CLAY_COL_BORDER, .width = { .left=1,.right=1,.top=1,.bottom=1 } },
                    .custom = { .customData = (void*)(intptr_t)CLAY_CUSTOM_LEFT_SIDEBAR },
                }) {}

                /* Center column */
                CLAY(CLAY_ID("CenterCol"), {
                    .layout = {
                        .sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap        = (uint16_t)gap,
                    },
                }) {
                    float availH  = h - topBarH - gap * 2.0f;
                    float screenH = availH * 0.55f;
                    if (screenH < 240.0f) screenH = 240.0f;
                    float tabsH   = 36.0f;
                    float toolsH  = availH - screenH - tabsH - gap * 2.0f;
                    if (toolsH < 60.0f) toolsH = 60.0f;

                    CLAY(CLAY_ID("Screen"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(screenH) },
                        },
                        .backgroundColor = { 0, 0, 0, 255 },
                        .border = { .color = CLAY_COL_BORDER, .width = { .left=1,.right=1,.top=1,.bottom=1 } },
                        .custom = { .customData = (void*)(intptr_t)CLAY_CUSTOM_SCREEN },
                    }) {}

                    CLAY(CLAY_ID("ToolsTabs"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(tabsH) },
                        },
                        .backgroundColor = CLAY_COL_PANEL,
                        .border = { .color = CLAY_COL_BORDER, .width = { .left=1,.right=1,.top=1,.bottom=1 } },
                        .custom = { .customData = (void*)(intptr_t)CLAY_CUSTOM_TOOLS_TABS },
                    }) {}

                    CLAY(CLAY_ID("ToolsView"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(toolsH) },
                        },
                        .backgroundColor = CLAY_COL_PANEL_ALT,
                        .border = { .color = CLAY_COL_BORDER, .width = { .left=1,.right=1,.top=1,.bottom=1 } },
                        .custom = { .customData = (void*)(intptr_t)CLAY_CUSTOM_TOOLS_VIEW },
                    }) {}
                }

                /* Right sidebar */
                CLAY(CLAY_ID("RightSidebar"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_FIXED(colRightW), CLAY_SIZING_GROW(0) },
                    },
                    .backgroundColor = CLAY_COL_PANEL,
                    .border = { .color = CLAY_COL_BORDER, .width = { .left=1,.right=1,.top=1,.bottom=1 } },
                    .custom = { .customData = (void*)(intptr_t)CLAY_CUSTOM_RIGHT_SIDEBAR },
                }) {}
            }

            /* Status bar */
            CLAY(CLAY_ID("StatusBar"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(statusBarH) },
                },
                .backgroundColor = CLAY_COL_STATUS_BG,
                .custom = { .customData = (void*)(intptr_t)CLAY_CUSTOM_STATUS_BAR },
            }) {}
        }
        else
        {
            /* Non-debug: NES screen fills workspace */
            CLAY(CLAY_ID("Screen"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                },
                .backgroundColor = { 0, 0, 0, 255 },
                .custom = { .customData = (void*)(intptr_t)CLAY_CUSTOM_SCREEN },
            }) {}

            /* Status bar */
            CLAY(CLAY_ID("StatusBar"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(statusBarH) },
                },
                .backgroundColor = CLAY_COL_STATUS_BG,
                .custom = { .customData = (void*)(intptr_t)CLAY_CUSTOM_STATUS_BAR },
            }) {}
        }
    }

    return Clay_EndLayout();
}

/* ── Clay → Nuklear renderer ─────────────────────────────────────────────── */
/*
 * All rendering and interaction happens inside ONE fullscreen Nuklear window
 * "CLAY_UI". This avoids NK_WINDOW_ROM issues that arise when overlapping
 * windows are opened (Nuklear marks all but the topmost as read-only).
 *
 * Canvas draws (backgrounds/borders/text) are done first via the window canvas.
 * Interactive zones are then rendered using nk_layout_space_push + nk_group_*.
 * The SCREEN zone draws the NES image directly on the parent canvas using
 * absolute coords obtained from nk_widget().
 */

/* Helper: draw zone content inside a group already begun by the caller */
internal void DrawZoneContent_TopBar(
    struct nk_context *ctx,
    Device *device,
    SDL_Window *win,
    f32 dt, f32 d1, f32 d2,
    u64 *initialCounter)
{
    (void)device; (void)d1; (void)d2;

    /* FPS counter */
    {
        u64 fpsCounter = SDL_GetPerformanceCounter();
        f32 fpsdt = GetSecondsElapsed(*initialCounter, fpsCounter);
        if (fpsdt > 1.0f) { *initialCounter = fpsCounter; app.ui.fps = app.ui.fpsCount; app.ui.fpsCount = 0; }
        ++app.ui.fpsCount;
    }

    nk_layout_row_template_begin(ctx, 22);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 50);
    nk_layout_row_template_push_static(ctx, 70);
    nk_layout_row_template_push_static(ctx, 50);
    nk_layout_row_template_push_static(ctx, 50);
    nk_layout_row_template_push_static(ctx, 70);
    if (debugMode) { nk_layout_row_template_push_static(ctx, 90); nk_layout_row_template_push_static(ctx, 75); }
    nk_layout_row_template_push_static(ctx, 70);
    nk_layout_row_template_end(ctx);

    {
        const char *romName = "No ROM";
        if (nes && loadedFilePath[0]) { const char *s = SDL_strrchr(loadedFilePath, '\\'); if (!s) s = SDL_strrchr(loadedFilePath, '/'); romName = s ? s + 1 : loadedFilePath; }
        nk_label_colored(ctx, romName, NK_TEXT_LEFT, COL_TEXT_DIM);
    }

    if (nk_button_label(ctx, "Open")) { debugging = TRUE; stepping = FALSE; SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Open ROM", "Drag & drop a .nes or .nsave file onto the window.", win); }
    if (nk_button_label(ctx, debugging ? "Run" : "Pause")) {
        if (!nes) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Run", "Load a ROM first.", win);
        } else if (debugging) {
            hitRun = TRUE; debugging = FALSE; stepping = FALSE;
        } else {
            debugging = TRUE; stepping = FALSE;
        }
    }
    if (nk_button_label(ctx, "Reset")) { if (nes) { ResetNES(nes); debugging = TRUE; } }
    if (nk_button_label(ctx, "Save"))  { if (nes) { debugging = TRUE; stepping = FALSE; if (saveFilePath[0]) Save(nes, saveFilePath); else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Save", "Load a ROM first.", win); } }

    nk_label_colored(ctx, DebugText("FPS %d", app.ui.fps), NK_TEXT_CENTERED, COL_TEXT_DIM);
    if (debugMode) { nk_label_colored(ctx, DebugText("dt %.4f", dt), NK_TEXT_CENTERED, COL_TEXT_DIM); nk_checkbox_label(ctx, "1cyc", &app.ui.oneCycleToggle); oneCycleAtTime = app.ui.oneCycleToggle; }
    nk_checkbox_label(ctx, "Debug", &app.ui.debugToggle);
    debugMode = app.ui.debugToggle;
}

internal void DrawZoneContent_LeftSidebar(struct nk_context *ctx, f32 d1, f32 d2)
{
    if (app.ui.leftSidebarCollapsed)
    {
        nk_layout_row_dynamic(ctx, 28, 1);
        if (nk_button_label(ctx, ">")) app.ui.leftSidebarCollapsed = FALSE;
        nk_layout_row_dynamic(ctx, 22, 1);
        if (nk_option_label(ctx, "S", app.ui.leftSidebarTab == 0)) app.ui.leftSidebarTab = 0;
        if (nk_option_label(ctx, "B", app.ui.leftSidebarTab == 1)) app.ui.leftSidebarTab = 1;
        return;
    }
    DrawPanelHeader(ctx, "SYSTEM");
    nk_layout_row_template_begin(ctx, 22);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 24);
    nk_layout_row_template_end(ctx);
    if (nk_option_label(ctx, "SYSTEM", app.ui.leftSidebarTab == 0)) app.ui.leftSidebarTab = 0;
    if (nk_option_label(ctx, "BREAK",  app.ui.leftSidebarTab == 1)) app.ui.leftSidebarTab = 1;
    if (nk_button_label(ctx, "<")) app.ui.leftSidebarCollapsed = TRUE;
    if (!nes) return;

    if (app.ui.leftSidebarTab == 0)
    {
        if (nk_tree_push(ctx, NK_TREE_TAB, "Controls", NK_MAXIMIZED))
        {
            nk_layout_row_dynamic(ctx, 20, 2);
            if (nk_button_label(ctx, debugging ? "Run" : "Pause")) {
                if (nes) { if (debugging) { hitRun = TRUE; debugging = FALSE; stepping = FALSE; } else { debugging = TRUE; stepping = FALSE; } }
            }
            if (nk_button_label(ctx, "Step")) { if (nes) { debugging = TRUE; stepping = TRUE; } }
            nk_label(ctx, DebugText("d1: %.4f", d1), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("d2: %.4f", d2), NK_TEXT_LEFT);
            nk_tree_pop(ctx);
        }
        if (nk_tree_push(ctx, NK_TREE_TAB, "CPU", NK_MAXIMIZED))
        {
            CPU *cpu = &nes->cpu;
            nk_layout_row_dynamic(ctx, 18, 2);
            nk_label_colored(ctx, DebugText("PC:%04X", cpu->pc), NK_TEXT_LEFT, COL_ACCENT);
            nk_label_colored(ctx, DebugText("SP:%02X",  cpu->sp), NK_TEXT_LEFT, COL_ACCENT);
            nk_label_colored(ctx, DebugText("A: %02X",  cpu->a),  NK_TEXT_LEFT, COL_ACCENT);
            nk_label_colored(ctx, DebugText("X: %02X",  cpu->x),  NK_TEXT_LEFT, COL_ACCENT);
            nk_label_colored(ctx, DebugText("Y: %02X",  cpu->y),  NK_TEXT_LEFT, COL_ACCENT);
            nk_label_colored(ctx, DebugText("P: %02X",  cpu->p),  NK_TEXT_LEFT, COL_ACCENT);
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label(ctx, "N V - B D I Z C", NK_TEXT_LEFT);
            nk_label(ctx, DebugText("%d %d %d %d %d %d %d %d",
                GetBitFlag(cpu->p, NEGATIVE_FLAG) ? 1:0, GetBitFlag(cpu->p, OVERFLOW_FLAG) ? 1:0,
                GetBitFlag(cpu->p, EMPTY_FLAG)    ? 1:0, GetBitFlag(cpu->p, BREAK_FLAG)    ? 1:0,
                GetBitFlag(cpu->p, DECIMAL_FLAG)  ? 1:0, GetBitFlag(cpu->p, INTERRUPT_FLAG)? 1:0,
                GetBitFlag(cpu->p, ZERO_FLAG)     ? 1:0, GetBitFlag(cpu->p, CARRY_FLAG)    ? 1:0), NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label(ctx, DebugText("CYCLES: %d", cpu->cycles), NK_TEXT_LEFT);
            nk_tree_pop(ctx);
        }
        if (nk_tree_push(ctx, NK_TREE_TAB, "PPU", NK_MINIMIZED))
        {
            PPU *ppu = &nes->ppu;
            nk_layout_row_dynamic(ctx, 18, 2);
            nk_label(ctx, DebugText("CTRL:%02X",  ppu->control),    NK_TEXT_LEFT);
            nk_label(ctx, DebugText("SL:%3d",     ppu->scanline),   NK_TEXT_LEFT);
            nk_label(ctx, DebugText("MASK:%02X",  ppu->mask),       NK_TEXT_LEFT);
            nk_label(ctx, DebugText("CYC:%3d",    ppu->cycle),      NK_TEXT_LEFT);
            nk_label(ctx, DebugText("STAT:%02X",  ppu->status),     NK_TEXT_LEFT);
            nk_label(ctx, DebugText("FRM:%lld",   ppu->frameCount), NK_TEXT_LEFT);
            nk_tree_pop(ctx);
        }
        if (nk_tree_push(ctx, NK_TREE_TAB, "Controller", NK_MINIMIZED))
        {
            s32 buttons[8] = { GetButton(nes,0,BUTTON_LEFT), GetButton(nes,0,BUTTON_UP), GetButton(nes,0,BUTTON_RIGHT), GetButton(nes,0,BUTTON_DOWN), GetButton(nes,0,BUTTON_SELECT), GetButton(nes,0,BUTTON_START), GetButton(nes,0,BUTTON_B), GetButton(nes,0,BUTTON_A) };
            nk_layout_row_static(ctx, 18, 18, 2); nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE); nk_checkbox_label(ctx, "", &buttons[1]);
            nk_layout_row_static(ctx, 18, 18, 9);
            nk_checkbox_label(ctx, "", &buttons[0]); nk_checkbox_label(ctx, "", &buttons[3]); nk_checkbox_label(ctx, "", &buttons[2]);
            nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
            nk_checkbox_label(ctx, "Sel", &buttons[4]); nk_checkbox_label(ctx, "St", &buttons[5]);
            nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
            nk_checkbox_label(ctx, "B", &buttons[6]); nk_checkbox_label(ctx, "A", &buttons[7]);
            nk_tree_pop(ctx);
        }
    }
    else
    {
        if (nk_tree_push(ctx, NK_TREE_TAB, "Breakpoint", NK_MAXIMIZED))
        {
            CPU *cpu = &nes->cpu;
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label(ctx, DebugText("PC: %04X", cpu->pc), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("BP: %04X", breakpoint), NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_edit_string(ctx, NK_EDIT_SIMPLE, app.ui.instructionBreakpointText, &app.ui.instructionBreakpointLen, 5, nk_filter_hex);
            if (app.ui.instructionBreakpointLen > 0) { app.ui.instructionBreakpointText[app.ui.instructionBreakpointLen] = 0; breakpoint = (u16)strtol(app.ui.instructionBreakpointText, NULL, 16); }
            nk_layout_row_dynamic(ctx, 24, 2);
            if (nk_button_label(ctx, "Run"))  { if (nes) { hitRun = TRUE; debugging = FALSE; stepping = FALSE; } }
            if (nk_button_label(ctx, "Step")) { if (nes) { debugging = TRUE; stepping = TRUE; } }
            nk_tree_pop(ctx);
        }
    }
}

internal void DrawZoneContent_ToolsTabs(struct nk_context *ctx)
{
    if (!nes) return;
    nk_layout_row_template_begin(ctx, 22);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 60);
    nk_layout_row_template_push_static(ctx, 70);
    nk_layout_row_template_push_static(ctx, 80);
    nk_layout_row_template_end(ctx);
    if (nk_option_label(ctx, "INSTRUCTIONS", app.ui.debugToolTab == 0)) app.ui.debugToolTab = 0;
    if (nk_option_label(ctx, "MEMORY",       app.ui.debugToolTab == 1)) app.ui.debugToolTab = 1;
    if (nk_button_label(ctx, "Step"))       { if (nes) { debugging = TRUE; stepping = TRUE; } }
    if (nk_button_label(ctx, "Step Over"))  { if (nes) { debugging = TRUE; stepping = TRUE; } }
    if (nk_button_label(ctx, "Run to Here")){ if (nes) { hitRun = TRUE; debugging = FALSE; stepping = FALSE; } }
}

internal void DrawZoneContent_RightSidebar(struct nk_context *ctx, Device *device)
{
    if (app.ui.rightSidebarCollapsed)
    {
        nk_layout_row_dynamic(ctx, 28, 1);
        if (nk_button_label(ctx, "<")) app.ui.rightSidebarCollapsed = FALSE;
        nk_layout_row_dynamic(ctx, 22, 1);
        if (nk_option_label(ctx, "V", app.ui.rightSidebarTab == 0)) app.ui.rightSidebarTab = 0;
        if (nk_option_label(ctx, "A", app.ui.rightSidebarTab == 1)) app.ui.rightSidebarTab = 1;
        return;
    }
    if (!nes) return;
    DrawPanelHeader(ctx, "VIDEO / AUDIO");
    nk_layout_row_template_begin(ctx, 22);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 24);
    nk_layout_row_template_end(ctx);
    if (nk_option_label(ctx, "VIDEO", app.ui.rightSidebarTab == 0)) app.ui.rightSidebarTab = 0;
    if (nk_option_label(ctx, "AUDIO", app.ui.rightSidebarTab == 1)) app.ui.rightSidebarTab = 1;
    if (nk_button_label(ctx, ">")) app.ui.rightSidebarCollapsed = TRUE;
    if (app.ui.rightSidebarTab == 0) DrawVideoContent(ctx, device, NULL);
    else                             DrawAudioContent(ctx);
}

internal void DrawZoneContent_StatusBar(struct nk_context *ctx)
{
    nk_layout_row_template_begin(ctx, 18);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 80);
    nk_layout_row_template_push_static(ctx, 160);
    nk_layout_row_template_push_static(ctx, 80);
    nk_layout_row_template_end(ctx);
    struct nk_color dark = nk_rgba(0x0D, 0x0D, 0x0D, 255);
    nk_label_colored(ctx,
        nes ? DebugText("  %s  PC $%04X  Mapper %d  NTSC", debugging ? "PAUSED" : "RUNNING", nes->cpu.pc, nes->cartridge.mapper) : "  No ROM loaded",
        NK_TEXT_LEFT, dark);
    nk_label_colored(ctx, "Drag & drop", NK_TEXT_CENTERED, dark);
    nk_label_colored(ctx, "WASD + A/S keys", NK_TEXT_CENTERED, dark);
    nk_label_colored(ctx, "v0.1.0", NK_TEXT_RIGHT, dark);
}

/* ── Walk Clay render commands and dispatch ─────────────────────────────── */
internal void Clay_NkRender(
    struct nk_context *ctx,
    Clay_RenderCommandArray cmds,
    s32 winW, s32 winH,
    Device *device,
    SDL_Window *win,
    f32 dt, f32 d1, f32 d2,
    u64 *initialCounter)
{
    struct nk_rect fullscreen = nk_rect(0, 0, (float)winW, (float)winH);
    /* One fullscreen window — no scrollbar, no title, background so it stays
       behind any Nuklear popups/tooltips.  All zones live inside this single
       window to avoid the NK_WINDOW_ROM read-only-mode bug that occurs when
       multiple overlapping windows are stacked. */
    nk_flags uiFlags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND;

    nk_window_set_bounds(ctx, "CLAY_UI", fullscreen);
    if (!nk_begin(ctx, "CLAY_UI", fullscreen, uiFlags))
    {
        nk_end(ctx);
        return;
    }

    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

    /* ── Pass 1: draw Clay background rectangles / borders / text ── */
    for (int i = 0; i < cmds.length; ++i)
    {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
        struct nk_rect r = nk_rect(cmd->boundingBox.x, cmd->boundingBox.y,
                                   cmd->boundingBox.width, cmd->boundingBox.height);
        switch (cmd->commandType)
        {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
        {
            Clay_Color c = cmd->renderData.rectangle.backgroundColor;
            if (c.a < 1.0f) break;
            nk_fill_rect(canvas, r, cmd->renderData.rectangle.cornerRadius.topLeft,
                nk_rgba((nk_byte)c.r,(nk_byte)c.g,(nk_byte)c.b,(nk_byte)c.a));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
        {
            Clay_BorderRenderData bc = cmd->renderData.border;
            Clay_Color c = bc.color;
            if (c.a < 1.0f) break;
            struct nk_color nc = nk_rgba((nk_byte)c.r,(nk_byte)c.g,(nk_byte)c.b,(nk_byte)c.a);
            if (bc.width.top    > 0) nk_fill_rect(canvas, nk_rect(r.x, r.y,                           r.w,                  (float)bc.width.top),    0, nc);
            if (bc.width.bottom > 0) nk_fill_rect(canvas, nk_rect(r.x, r.y+r.h-bc.width.bottom,       r.w,                  (float)bc.width.bottom), 0, nc);
            if (bc.width.left   > 0) nk_fill_rect(canvas, nk_rect(r.x, r.y,                           (float)bc.width.left, r.h),                    0, nc);
            if (bc.width.right  > 0) nk_fill_rect(canvas, nk_rect(r.x+r.w-bc.width.right, r.y,        (float)bc.width.right,r.h),                    0, nc);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_TEXT:
        {
            Clay_Color c = cmd->renderData.text.textColor;
            if (g_clayNkFont && cmd->renderData.text.stringContents.length > 0)
                nk_draw_text(canvas, r,
                    cmd->renderData.text.stringContents.chars,
                    cmd->renderData.text.stringContents.length,
                    g_clayNkFont, nk_rgba(0,0,0,0),
                    nk_rgba((nk_byte)c.r,(nk_byte)c.g,(nk_byte)c.b,(nk_byte)c.a));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: nk_push_scissor(canvas, r);          break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:   nk_push_scissor(canvas, fullscreen); break;
        default: break;
        }
    }

    /* ── Pass 2: render interactive zones using layout_space + groups ── */
    /* Count CUSTOM commands so we can size the layout_space correctly */
    int customCount = 0;
    for (int i = 0; i < cmds.length; ++i)
        if (Clay_RenderCommandArray_Get(&cmds, i)->commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM)
            ++customCount;

    if (customCount > 0)
    {
        nk_layout_space_begin(ctx, NK_STATIC, (float)winH, customCount);

        for (int i = 0; i < cmds.length; ++i)
        {
            Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
            if (cmd->commandType != CLAY_RENDER_COMMAND_TYPE_CUSTOM) continue;

            int zoneId = (int)(intptr_t)cmd->renderData.custom.customData;
            /* bounds relative to the window (which starts at 0,0) */
            struct nk_rect b = nk_rect(
                cmd->boundingBox.x, cmd->boundingBox.y,
                cmd->boundingBox.width, cmd->boundingBox.height);

            nk_layout_space_push(ctx, b);

            switch (zoneId)
            {
            /* ── SCREEN: draw NES image directly on canvas, no group needed ── */
            case CLAY_CUSTOM_SCREEN:
            {
                if (nes)
                {
                    struct nk_rect space;
                    enum nk_widget_layout_states state = nk_widget(&space, ctx);
                    if (state)
                    {
                        GUI *gui = &nes->gui;
                        f32 aspect = 256.0f / 240.0f;
                        f32 drawW = space.w, drawH = space.w / aspect;
                        if (drawH > space.h) { drawH = space.h; drawW = drawH * aspect; }
                        f32 ox = space.x + (space.w - drawW) * 0.5f;
                        f32 oy = space.y + (space.h - drawH) * 0.5f;
                        struct nk_rect dst = nk_rect(ox, oy, drawW, drawH);

                        glBindTexture(GL_TEXTURE_2D, device->screen.handle.id);
                        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGBA, GL_UNSIGNED_BYTE, gui->pixels);
                        glBindTexture(GL_TEXTURE_2D, 0);
                        nk_draw_image(canvas, dst, &device->screen, nk_rgb(255,255,255));
                    }
                }
                else
                {
                    /* consume the space so layout stays consistent */
                    nk_spacing(ctx, 1);
                }
                break;
            }

            /* ── TOP BAR ── */
            case CLAY_CUSTOM_TOP_BAR:
            {
                nk_flags gf = NK_WINDOW_NO_SCROLLBAR;
                if (nk_group_begin(ctx, "grp_topbar", gf))
                {
                    DrawZoneContent_TopBar(ctx, device, win, dt, d1, d2, initialCounter);
                    nk_group_end(ctx);
                }
                break;
            }

            /* ── LEFT SIDEBAR ── */
            case CLAY_CUSTOM_LEFT_SIDEBAR:
            {
                if (nk_group_begin(ctx, "grp_leftsidebar", 0))
                {
                    DrawZoneContent_LeftSidebar(ctx, d1, d2);
                    nk_group_end(ctx);
                }
                break;
            }

            /* ── TOOLS TABS ── */
            case CLAY_CUSTOM_TOOLS_TABS:
            {
                if (nk_group_begin(ctx, "grp_toolstabs", NK_WINDOW_NO_SCROLLBAR))
                {
                    DrawZoneContent_ToolsTabs(ctx);
                    nk_group_end(ctx);
                }
                break;
            }

            /* ── TOOLS VIEW ── */
            case CLAY_CUSTOM_TOOLS_VIEW:
            {
                const char *gname = (app.ui.debugToolTab == 0) ? "grp_instructions" : "grp_memory";
                if (nk_group_begin(ctx, gname, 0))
                {
                    if (nes)
                    {
                        if (app.ui.debugToolTab == 0) DrawInstructionsContent(ctx, device);
                        else                          DrawMemoryContent(ctx);
                    }
                    else
                    {
                        nk_layout_row_dynamic(ctx, 24, 1);
                        nk_label_colored(ctx, "Load a ROM to inspect instructions and memory.", NK_TEXT_LEFT, COL_TEXT_DIM);
                    }
                    nk_group_end(ctx);
                }
                break;
            }

            /* ── RIGHT SIDEBAR ── */
            case CLAY_CUSTOM_RIGHT_SIDEBAR:
            {
                if (nk_group_begin(ctx, "grp_rightsidebar", 0))
                {
                    DrawZoneContent_RightSidebar(ctx, device);
                    nk_group_end(ctx);
                }
                break;
            }

            /* ── STATUS BAR ── */
            case CLAY_CUSTOM_STATUS_BAR:
            {
                {
                    struct nk_style_item oldBg = ctx->style.window.fixed_background;
                    ctx->style.window.fixed_background = nk_style_item_color(nk_rgba(0, 0, 0, 0));
                    if (nk_group_begin(ctx, "grp_statusbar", NK_WINDOW_NO_SCROLLBAR))
                    {
                        DrawZoneContent_StatusBar(ctx);
                        nk_group_end(ctx);
                    }
                    ctx->style.window.fixed_background = oldBg;
                }
                break;
            }

            default:
                nk_spacing(ctx, 1);
                break;
            }
        }

        nk_layout_space_end(ctx);
    }

    nk_end(ctx);
}



/* ── DrawInstructionsContent: disassembly list ─────────────────────────── */
internal void DrawInstructionsContent(struct nk_context *ctx, Device *device)
{
    (void)device;
    if (!nes) return;

    CPU *cpu = &nes->cpu;
    const float ratio[] = { 80, 120 };

    DrawPanelHeader(ctx, "INSTRUCTIONS");

    nk_layout_row(ctx, NK_STATIC, 22, 2, ratio);
    nk_label(ctx, "Address:", NK_TEXT_LEFT);
    nk_edit_string(ctx, NK_EDIT_SIMPLE,
        app.ui.instructionAddressText, &app.ui.instructionAddressLen,
        12, nk_filter_hex);

    nk_layout_row(ctx, NK_STATIC, 22, 2, ratio);
    nk_label(ctx, "Breakpoint:", NK_TEXT_LEFT);
    nk_edit_string(ctx, NK_EDIT_SIMPLE,
        app.ui.instructionBreakpointText, &app.ui.instructionBreakpointLen,
        5, nk_filter_hex);

    nk_layout_row_dynamic(ctx, 18, 1);

    u16 pc = cpu->pc;
    if (app.ui.instructionAddressLen > 0)
    {
        app.ui.instructionAddressText[app.ui.instructionAddressLen] = 0;
        u16 parsed = (u16)strtol(app.ui.instructionAddressText, NULL, 16);
        if (parsed >= 0x8000) pc = parsed;
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
        b32 currentInstr  = (pc == cpu->pc);
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
            case 2: col += sprintf(debugBuffer + col, " %02X", ReadCPUU8(nes, pc + 1)); break;
            case 3: col += sprintf(debugBuffer + col, " %02X %02X", ReadCPUU8(nes, pc + 1), ReadCPUU8(nes, pc + 2)); break;
            default: break;
        }

        memset(debugBuffer + col, ' ', 18 - col);
        col = 18;
        col += sprintf(debugBuffer + col, "%s", GetInstructionStr(instruction->instruction));

        switch (instruction->addressingMode)
        {
            case AM_IMM: col += sprintf(debugBuffer + col, " #$%02X", ReadCPUU8(nes, pc + 1)); break;
            case AM_ABS: { u8 lo = ReadCPUU8(nes, pc+1); u8 hi = ReadCPUU8(nes, pc+2); col += sprintf(debugBuffer + col, " $%04X", (hi<<8)|lo); break; }
            case AM_ABX: { u8 lo = ReadCPUU8(nes, pc+1); u8 hi = ReadCPUU8(nes, pc+2); col += sprintf(debugBuffer + col, " $%04X,X", (hi<<8)|lo); break; }
            case AM_ABY: { u8 lo = ReadCPUU8(nes, pc+1); u8 hi = ReadCPUU8(nes, pc+2); col += sprintf(debugBuffer + col, " $%04X,Y", (hi<<8)|lo); break; }
            case AM_ZPA: col += sprintf(debugBuffer + col, " $%02X", ReadCPUU8(nes, pc+1)); break;
            case AM_ZPX: col += sprintf(debugBuffer + col, " $%02X,X", ReadCPUU8(nes, pc+1)); break;
            case AM_ZPY: col += sprintf(debugBuffer + col, " $%02X,Y", ReadCPUU8(nes, pc+1)); break;
            case AM_IND: { u8 lo = ReadCPUU8(nes, pc+1); u8 hi = ReadCPUU8(nes, pc+2); col += sprintf(debugBuffer + col, " ($%04X)", (hi<<8)|lo); break; }
            case AM_IZX: col += sprintf(debugBuffer + col, " ($%02X,X)", ReadCPUU8(nes, pc+1)); break;
            case AM_IZY: col += sprintf(debugBuffer + col, " ($%02X),Y", ReadCPUU8(nes, pc+1)); break;
            case AM_REL: { s8 rel = (s8)ReadCPUU8(nes, pc+1); col += sprintf(debugBuffer + col, " $%04X", (u16)(pc + instruction->bytesCount + rel)); break; }
            default: break;
        }

        if (instruction->instruction == CPU_RTS)
            col += sprintf(debugBuffer + col, " ----");

        if (currentInstr)
            nk_label_colored(ctx, debugBuffer, NK_TEXT_LEFT, COL_ACCENT);
        else if (breakpointHit)
            nk_label_colored(ctx, debugBuffer, NK_TEXT_LEFT, nk_rgba(0xFF,0x44,0x44,255));
        else
            nk_label(ctx, debugBuffer, NK_TEXT_LEFT);

        pc += instruction->bytesCount;
    }
}

/* ── DrawMemoryContent: raw memory dump ─────────────────────────────────── */
internal void DrawMemoryContent(struct nk_context *ctx)
{
    if (!nes) return;

    DrawPanelHeader(ctx, "MEMORY");

    enum mem_options { CPU_MEM, PPU_MEM, OAM_MEM, OAM2_MEM };
    const float ratio[] = { 80, 80, 80, 80 };

    if (app.ui.memoryOption < CPU_MEM || app.ui.memoryOption > OAM2_MEM)
        app.ui.memoryOption = CPU_MEM;

    nk_layout_row(ctx, NK_STATIC, 22, 4, ratio);
    app.ui.memoryOption = nk_option_label(ctx, "CPU",  app.ui.memoryOption == CPU_MEM)  ? CPU_MEM  : app.ui.memoryOption;
    app.ui.memoryOption = nk_option_label(ctx, "PPU",  app.ui.memoryOption == PPU_MEM)  ? PPU_MEM  : app.ui.memoryOption;
    app.ui.memoryOption = nk_option_label(ctx, "OAM",  app.ui.memoryOption == OAM_MEM)  ? OAM_MEM  : app.ui.memoryOption;
    app.ui.memoryOption = nk_option_label(ctx, "OAM2", app.ui.memoryOption == OAM2_MEM) ? OAM2_MEM : app.ui.memoryOption;

    const float ar[] = { 80, 120 };
    nk_layout_row(ctx, NK_STATIC, 22, 2, ar);
    nk_label(ctx, "Address:", NK_TEXT_LEFT);
    nk_edit_string(ctx, NK_EDIT_SIMPLE, app.ui.memoryAddressText, &app.ui.memoryAddressLen, 12, nk_filter_hex);

    u16 address = 0x0000;
    if (app.ui.memoryAddressLen > 0)
    {
        app.ui.memoryAddressText[app.ui.memoryAddressLen] = 0;
        address = (u16)strtol(app.ui.memoryAddressText, NULL, 16);
    }

    nk_layout_row_dynamic(ctx, 18, 1);

    if (app.ui.memoryOption == OAM_MEM)
    {
        for (s32 i = 0; i < 16; ++i)
        {
            memset(debugBuffer, 0, sizeof(debugBuffer));
            s32 col = sprintf(debugBuffer, "%04X:", i * 16);
            for (s32 j = 0; j < 16; ++j) col += sprintf(debugBuffer + col, " %02X", ReadU8(&nes->oamMemory, i*16+j));
            nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
        }
    }
    else if (app.ui.memoryOption == OAM2_MEM)
    {
        for (s32 i = 0; i < 2; ++i)
        {
            memset(debugBuffer, 0, sizeof(debugBuffer));
            s32 col = sprintf(debugBuffer, "%04X:", i * 16);
            for (s32 j = 0; j < 16; ++j) col += sprintf(debugBuffer + col, " %02X", ReadU8(&nes->oamMemory2, i*16+j));
            nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
        }
    }
    else
    {
        for (s32 i = 0; i < 16; ++i)
        {
            memset(debugBuffer, 0, sizeof(debugBuffer));
            s32 col = sprintf(debugBuffer, "%04X:", address + i * 16);
            for (s32 j = 0; j < 16; ++j)
            {
                u8 v = (app.ui.memoryOption == CPU_MEM)
                    ? ReadCPUU8(nes, address + i*16 + j)
                    : ReadPPUU8(nes, address + i*16 + j);
                col += sprintf(debugBuffer + col, " %02X", v);
            }
            nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
        }
    }
}

/* ── DrawOAM (shared helper) ─────────────────────────────────────────────── */
internal void DrawOAM(struct nk_context *ctx, struct nk_command_buffer *canvas, const struct nk_input *input, struct nk_image *deviceOam, Color (*guiSprites)[128], Memory *oamMem, s32 spriteCount, s32 columns)
{
    PPU *ppu = &nes->ppu;
    u8 spriteAddr = GetBitFlag(ppu->control, SPRITE_ADDR_FLAG);
    u8 spriteSize = 8 * (GetBitFlag(ppu->control, SPRITE_SIZE_FLAG) + 1);
    u16 baseAddress = 0x1000 * spriteAddr;

    nk_layout_row_static(ctx, 16, 8, columns);

    for (s32 index = 0; index < spriteCount; ++index)
    {
        u8 spriteY    = ReadU8(oamMem, index * 4 + 0);
        u8 spriteIdx  = ReadU8(oamMem, index * 4 + 1);
        u8 spriteAttr = ReadU8(oamMem, index * 4 + 2);
        u8 spriteX    = ReadU8(oamMem, index * 4 + 3);

        if (spriteSize == 16)
        {
            baseAddress = 0x1000 * (spriteIdx & 1);
            spriteIdx &= 0xFE;
        }

        u8 pixelHighBits = spriteAttr & 0x03;
        b32 flipV = spriteAttr & 0x80;
        b32 flipH = spriteAttr & 0x40;

        for (s32 y = 0; y < spriteSize; ++y)
        {
            u8 rowOffset = y;
            if (flipV) rowOffset = (spriteSize - 1) - rowOffset;
            u8 row1 = GetSpritePixelRow(nes, baseAddress, spriteIdx, rowOffset, 0);
            u8 row2 = GetSpritePixelRow(nes, baseAddress, spriteIdx, rowOffset, 1);
            for (s32 x = 0; x < 8; ++x)
            {
                u8 colOffset = x;
                if (flipH) colOffset = 7 - colOffset;
                u32 paletteIndex = GetPixelColorBits(row1, row2, colOffset, pixelHighBits);
                u32 colorIndex   = ReadPPUU8(nes, 0x3F10 + paletteIndex);
                guiSprites[index][y * 8 + x] = systemPalette[colorIndex % 64];
            }
        }

        struct nk_rect space;
        enum nk_widget_layout_states state = nk_widget(&space, ctx);
        if (state)
        {
            glBindTexture(GL_TEXTURE_2D, deviceOam[index].handle.id);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 16, GL_RGBA, GL_UNSIGNED_BYTE, guiSprites[index]);
            glBindTexture(GL_TEXTURE_2D, 0);
            nk_draw_image(canvas, space, &deviceOam[index], nk_rgb(255, 255, 255));

            if (input && nk_input_is_mouse_hovering_rect(input, space))
            {
                if (nk_tooltip_begin(ctx, 200))
                {
                    nk_layout_row_static(ctx, 64, 32, 1);
                    state = nk_widget(&space, ctx);
                    if (state) nk_draw_image(canvas, space, &deviceOam[index], nk_rgb(255,255,255));
                    nk_layout_row_dynamic(ctx, 18, 2);
                    nk_label(ctx, DebugText("Num: %02X", index),         NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("Tile: %02X", spriteIdx),    NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("X: %02X", spriteX),         NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("Y: %02X", spriteY),         NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("Pal: %02X", pixelHighBits), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("Fl: %s%s", flipV?"V":"", flipH?"H":""), NK_TEXT_LEFT);
                    nk_tooltip_end(ctx);
                }
            }
        }
    }
}

/* ── DrawAudioWaveform (shared helper) ───────────────────────────────────── */
internal void DrawAudioWaveform(struct nk_context *ctx, struct nk_command_buffer *canvas, s16 *buffer, s32 pointCount)
{
    const f32 rectHeight  = 120.0f;
    struct nk_color lineColor = nk_rgb(0x33, 0xFF, 0x66);
    const f32 lineThickness   = 1.0f;
    struct nk_rect space;

    nk_layout_row_dynamic(ctx, rectHeight, 1);
    enum nk_widget_layout_states state = nk_widget(&space, ctx);
    if (state && pointCount > 0)
    {
        struct nk_vec2 *points = (struct nk_vec2 *)Allocate(pointCount * sizeof(struct nk_vec2));
        f32 hspacing = space.w / pointCount;
        for (s32 i = 0; i < pointCount; i++)
        {
            f32 x = hspacing * i;
            f32 y = rectHeight - buffer[i] * rectHeight / APU_AMPLIFIER_VALUE;
            *(points + i) = nk_vec2(space.x + x, space.y + y);
        }
        nk_stroke_rect(canvas, space, 0, 1, COL_BORDER_HI);
        nk_stroke_polyline(canvas, (f32*)points, pointCount, lineThickness, lineColor);
        Free(points);
    }
}

/* ── DrawVideoContent: patterns/palettes/OAM/nametables ─────────────────── */
internal void DrawVideoContent(struct nk_context *ctx, struct Device *device, const struct nk_input *input)
{
    if (!nes) return;

    PPU *ppu = &nes->ppu;
    GUI *gui = &nes->gui;
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    struct nk_rect space;
    enum nk_widget_layout_states state;

    enum video_options { PATTERNS_PALETTES_OAM, NAMETABLES };
    const float ratio[] = { 200, 200 };

    if (app.ui.videoOption < PATTERNS_PALETTES_OAM || app.ui.videoOption > NAMETABLES)
        app.ui.videoOption = PATTERNS_PALETTES_OAM;

    nk_layout_row(ctx, NK_STATIC, 22, 2, ratio);
    app.ui.videoOption = nk_option_label(ctx, "PATTERNS/PALETTES/OAM", app.ui.videoOption == PATTERNS_PALETTES_OAM) ? PATTERNS_PALETTES_OAM : app.ui.videoOption;
    app.ui.videoOption = nk_option_label(ctx, "NAMETABLES",             app.ui.videoOption == NAMETABLES)            ? NAMETABLES            : app.ui.videoOption;

    if (app.ui.videoOption == PATTERNS_PALETTES_OAM)
    {
        /* PATTERNS */
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label_colored(ctx, "PATTERNS", NK_TEXT_LEFT, COL_TEXT_DIM);
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
                            u8 h = ((row2 >> (7-x)) & 0x1);
                            u8 l = ((row1 >> (7-x)) & 0x1);
                            u32 pi = (h << 1) | l;
                            u32 ci = ReadPPUU8(nes, 0x3F00 + pi);
                            gui->patterns[index][(tileY*8+y)*128 + (tileX*8+x)] = systemPalette[ci % 64];
                        }
                    }
                }
            }
            state = nk_widget(&space, ctx);
            if (state)
            {
                glBindTexture(GL_TEXTURE_2D, device->patterns[index].handle.id);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, gui->patterns[index]);
                glBindTexture(GL_TEXTURE_2D, 0);
                nk_draw_image(canvas, space, &device->patterns[index], nk_rgb(255,255,255));

                if (input && nk_input_is_mouse_hovering_rect(input, space))
                {
                    if (nk_tooltip_begin(ctx, 200))
                    {
                        f32 mx = input->mouse.pos.x - space.x;
                        f32 my = input->mouse.pos.y - space.y;
                        u16 pi = (s32)(my/8)*16 + (s32)(mx/8);
                        for (s32 y = 0; y < 8; ++y)
                        {
                            u8 r1 = ReadPPUU8(nes, baseAddress + pi*16 + y);
                            u8 r2 = ReadPPUU8(nes, baseAddress + pi*16 + 8 + y);
                            for (s32 x = 0; x < 8; ++x)
                            {
                                u8 h = ((r2>>(7-x))&1); u8 l = ((r1>>(7-x))&1);
                                gui->patternHover[y*8+x] = systemPalette[ReadPPUU8(nes,0x3F00+((h<<1)|l))%64];
                            }
                        }
                        nk_layout_row_static(ctx, 32, 32, 1);
                        state = nk_widget(&space, ctx);
                        if (state)
                        {
                            glBindTexture(GL_TEXTURE_2D, device->patternHover.handle.id);
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, gui->patternHover);
                            glBindTexture(GL_TEXTURE_2D, 0);
                            nk_draw_image(canvas, space, &device->patternHover, nk_rgb(255,255,255));
                        }
                        nk_layout_row_dynamic(ctx, 18, 1);
                        nk_label(ctx, DebugText("Addr: %04X", baseAddress + pi*16), NK_TEXT_LEFT);
                        nk_label(ctx, DebugText("Table: %d  Tile: %02X", index, pi), NK_TEXT_LEFT);
                        nk_tooltip_end(ctx);
                    }
                }
            }
        }

        /* PALETTES */
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label_colored(ctx, "PALETTES", NK_TEXT_LEFT, COL_TEXT_DIM);
        nk_layout_row_static(ctx, 18, 18, 16);
        for (s32 index = 0; index < 2; ++index)
        {
            for (s32 i = 0; i < 16; ++i)
            {
                u16 addr = 0x3F00 + (index * 0x10) + i;
                u8 ci = ReadPPUU8(nes, addr);
                Color color = systemPalette[ci % 64];
                state = nk_widget(&space, ctx);
                if (state)
                {
                    nk_fill_rect(canvas, space, 0, nk_rgb(color.r, color.g, color.b));
                    if (input && nk_input_is_mouse_hovering_rect(input, space))
                    {
                        if (nk_tooltip_begin(ctx, 160))
                        {
                            nk_layout_row_static(ctx, 24, 24, 1);
                            state = nk_widget(&space, ctx);
                            if (state) nk_fill_rect(canvas, space, 0, nk_rgb(color.r, color.g, color.b));
                            nk_layout_row_dynamic(ctx, 18, 1);
                            nk_label(ctx, DebugText("%04X  idx=%02X  off=%d", addr, ci, i%4), NK_TEXT_LEFT);
                            nk_tooltip_end(ctx);
                        }
                    }
                }
            }
        }

        /* OAM */
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label_colored(ctx, "OAM", NK_TEXT_LEFT, COL_TEXT_DIM);
        DrawOAM(ctx, canvas, input, device->oam,  (Color(*)[128])gui->sprites,  &nes->oamMemory,  64, 16);
        DrawOAM(ctx, canvas, input, device->oam2, (Color(*)[128])gui->sprites2, &nes->oamMemory2,  8,  8);
    }
    else /* NAMETABLES */
    {
        enum nt_options { H2000, H2400, H2800, H2C00 };
        const float ntr[] = { 70, 70, 70, 70, 50 };

        if (app.ui.nametableOption < H2000 || app.ui.nametableOption > H2C00)
            app.ui.nametableOption = H2000;

        nk_layout_row(ctx, NK_STATIC, 22, 5, ntr);
        app.ui.nametableOption = nk_option_label(ctx, "$2000", app.ui.nametableOption == H2000) ? H2000 : app.ui.nametableOption;
        app.ui.nametableOption = nk_option_label(ctx, "$2400", app.ui.nametableOption == H2400) ? H2400 : app.ui.nametableOption;
        app.ui.nametableOption = nk_option_label(ctx, "$2800", app.ui.nametableOption == H2800) ? H2800 : app.ui.nametableOption;
        app.ui.nametableOption = nk_option_label(ctx, "$2C00", app.ui.nametableOption == H2C00) ? H2C00 : app.ui.nametableOption;
        nk_checkbox_label(ctx, "PIX", &app.ui.showSeparatePixels);

        u16 address = 0x2000 + app.ui.nametableOption * 0x400;

        if (app.ui.showSeparatePixels)
        {
            nk_layout_row_static(ctx, 8, 8, 32);
            u16 bgBase = 0x1000 * GetBitFlag(ppu->control, BACKGROUND_ADDR_FLAG);
            for (s32 tileY = 0; tileY < 30; ++tileY)
            {
                for (s32 tileX = 0; tileX < 32; ++tileX)
                {
                    u16 tileIndex    = tileY * 32 + tileX;
                    u8  patternIndex = ReadPPUU8(nes, address + tileIndex);
                    u16 attrX = tileX/4, attrOffX = tileX%4;
                    u16 attrY = tileY/4, attrOffY = tileY%4;
                    u8  attrByte = ReadPPUU8(nes, address + 0x3C0 + attrY*8 + attrX);
                    u8  lv = attributeTableLookup[attrOffY][attrOffX];
                    u8  hcb = (attrByte >> ((lv/4)*2)) & 0x03;
                    for (s32 y = 0; y < 8; ++y)
                    {
                        u8 r1 = ReadPPUU8(nes, bgBase + patternIndex*16 + y);
                        u8 r2 = ReadPPUU8(nes, bgBase + patternIndex*16 + 8 + y);
                        for (s32 x = 0; x < 8; ++x)
                        {
                            u8 h = ((r2>>(7-x))&1); u8 l = ((r1>>(7-x))&1);
                            u8 v = (h<<1)|l;
                            u32 ci = ReadPPUU8(nes, 0x3F00 + ((hcb<<2)|v));
                            if (GetBitFlag(ppu->mask, COLOR_FLAG)) ci &= 0x30;
                            Color color = systemPalette[ci % 64];
                            u8 cm = (ppu->mask & 0xE0) >> 5;
                            if (cm) ColorEmphasis(&color, cm);
                            gui->nametable2[tileX][tileY][y*8+x] = color;
                        }
                    }
                    state = nk_widget(&space, ctx);
                    if (state)
                    {
                        glBindTexture(GL_TEXTURE_2D, device->nametable2[tileY*32+tileX].handle.id);
                        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,8,8,GL_RGBA,GL_UNSIGNED_BYTE, gui->nametable2[tileX][tileY]);
                        glBindTexture(GL_TEXTURE_2D, 0);
                        nk_draw_image(canvas, space, &device->nametable2[tileY*32+tileX], nk_rgb(255,255,255));
                    }
                }
            }
        }
        else
        {
            nk_layout_row_static(ctx, 240, 256, 1);
            u16 bgBase = 0x1000 * GetBitFlag(ppu->control, BACKGROUND_ADDR_FLAG);
            for (s32 tileY = 0; tileY < 30; ++tileY)
            {
                for (s32 tileX = 0; tileX < 32; ++tileX)
                {
                    u16 tileIndex    = tileY * 32 + tileX;
                    u8  patternIndex = ReadPPUU8(nes, address + tileIndex);
                    u16 attrX = tileX/4, attrOffX = tileX%4;
                    u16 attrY = tileY/4, attrOffY = tileY%4;
                    u8  attrByte = ReadPPUU8(nes, address + 0x3C0 + attrY*8 + attrX);
                    u8  lv = attributeTableLookup[attrOffY][attrOffX];
                    u8  hcb = (attrByte >> ((lv/4)*2)) & 0x03;
                    for (s32 y = 0; y < 8; ++y)
                    {
                        u8 r1 = ReadPPUU8(nes, bgBase + patternIndex*16 + y);
                        u8 r2 = ReadPPUU8(nes, bgBase + patternIndex*16 + 8 + y);
                        for (s32 x = 0; x < 8; ++x)
                        {
                            u8 h = ((r2>>(7-x))&1); u8 l = ((r1>>(7-x))&1);
                            u8 v = (h<<1)|l;
                            u32 ci = ReadPPUU8(nes, 0x3F00 + ((hcb<<2)|v));
                            if (GetBitFlag(ppu->mask, COLOR_FLAG)) ci &= 0x30;
                            Color color = systemPalette[ci % 64];
                            u8 cm = (ppu->mask & 0xE0) >> 5;
                            if (cm) ColorEmphasis(&color, cm);
                            gui->nametable[(tileY*8+y)*256 + (tileX*8+x)] = color;
                        }
                    }
                }
            }
            state = nk_widget(&space, ctx);
            if (state)
            {
                glBindTexture(GL_TEXTURE_2D, device->nametable.handle.id);
                glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,240,GL_RGBA,GL_UNSIGNED_BYTE, gui->nametable);
                glBindTexture(GL_TEXTURE_2D, 0);
                nk_draw_image(canvas, space, &device->nametable, nk_rgb(255,255,255));
            }
        }
    }
}

/* ── DrawAudioContent ────────────────────────────────────────────────────── */
internal void DrawAudioContent(struct nk_context *ctx)
{
    if (!nes) return;

    APU *apu = &nes->apu;
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

    enum audio_options { GENERAL, SQUARE1, SQUARE2, TRIANGLE_OPT, NOISE_OPT, DMC_OPT, BUFFER_OPT };
    const float ratio[] = { 80, 80, 80, 80, 80 };

    if (app.ui.audioOption < GENERAL || app.ui.audioOption > BUFFER_OPT)
        app.ui.audioOption = GENERAL;

    nk_layout_row(ctx, NK_STATIC, 22, 5, ratio);
    app.ui.audioOption = nk_option_label(ctx, "GENERAL",  app.ui.audioOption == GENERAL)       ? GENERAL       : app.ui.audioOption;
    app.ui.audioOption = nk_option_label(ctx, "SQ1",      app.ui.audioOption == SQUARE1)        ? SQUARE1       : app.ui.audioOption;
    app.ui.audioOption = nk_option_label(ctx, "SQ2",      app.ui.audioOption == SQUARE2)        ? SQUARE2       : app.ui.audioOption;
    app.ui.audioOption = nk_option_label(ctx, "TRI",      app.ui.audioOption == TRIANGLE_OPT)   ? TRIANGLE_OPT  : app.ui.audioOption;
    app.ui.audioOption = nk_option_label(ctx, "NOISE",    app.ui.audioOption == NOISE_OPT)      ? NOISE_OPT     : app.ui.audioOption;
    const float ratio2[] = { 60, 60 };
    nk_layout_row(ctx, NK_STATIC, 22, 2, ratio2);
    app.ui.audioOption = nk_option_label(ctx, "DMC",      app.ui.audioOption == DMC_OPT)        ? DMC_OPT       : app.ui.audioOption;
    app.ui.audioOption = nk_option_label(ctx, "BUF",      app.ui.audioOption == BUFFER_OPT)     ? BUFFER_OPT    : app.ui.audioOption;

    if (app.ui.audioOption == GENERAL)
    {
        nk_layout_row_dynamic(ctx, 18, 3);
        nk_label(ctx, DebugText("CYC:%lld",  apu->cycles),           NK_TEXT_LEFT);
        nk_label(ctx, DebugText("MODE:%02X", apu->frameMode),        NK_TEXT_LEFT);
        nk_label(ctx, DebugText("RATE:%d",   APU_SAMPLES_PER_SECOND),NK_TEXT_LEFT);
        nk_label(ctx, DebugText("IRQ:%02X",  !apu->inhibitIRQ),      NK_TEXT_LEFT);
        nk_label(ctx, DebugText("FVAL:%02X", apu->frameValue),       NK_TEXT_LEFT);
        nk_label(ctx, DebugText("SCNT:%02X", apu->sampleCounter),    NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 18, 2);
        nk_checkbox_label(ctx, "SQ1",  &app.ui.square1Enabled);
        nk_checkbox_label(ctx, "SQ2",  &app.ui.square2Enabled);
        nk_checkbox_label(ctx, "TRI",  &app.ui.triangleEnabled);
        nk_checkbox_label(ctx, "NOISE",&app.ui.noiseEnabled);
        nk_checkbox_label(ctx, "DMC",  &app.ui.dmcEnabled);
        apu->pulse1.globalEnabled    = app.ui.square1Enabled;
        apu->pulse2.globalEnabled    = app.ui.square2Enabled;
        apu->triangle.globalEnabled  = app.ui.triangleEnabled;
        apu->noise.globalEnabled     = app.ui.noiseEnabled;
        apu->dmc.globalEnabled       = app.ui.dmcEnabled;
        DrawAudioWaveform(ctx, canvas, apu->buffer, apu->bufferIndex);
    }
    else if (app.ui.audioOption == SQUARE1 || app.ui.audioOption == SQUARE2)
    {
        APUPulse *pulse = (app.ui.audioOption == SQUARE1) ? &apu->pulse1 : &apu->pulse2;
        nk_layout_row_dynamic(ctx, 18, 3);
        nk_label(ctx, DebugText("ENV_EN:%02X", pulse->envelopeEnabled), NK_TEXT_LEFT);
        nk_label(ctx, DebugText("SWP_EN:%02X", pulse->sweepEnabled),    NK_TEXT_LEFT);
        nk_label(ctx, DebugText("LEN_EN:%02X", pulse->lengthEnabled),   NK_TEXT_LEFT);
        nk_label(ctx, DebugText("DUTY:%02X",   pulse->dutyMode),        NK_TEXT_LEFT);
        nk_label(ctx, DebugText("PERIOD:%02X", pulse->timerPeriod),     NK_TEXT_LEFT);
        nk_label(ctx, DebugText("LEN:%02X",    pulse->lengthValue),     NK_TEXT_LEFT);
        DrawAudioWaveform(ctx, canvas, pulse->buffer, pulse->bufferIndex);
    }
    else if (app.ui.audioOption == TRIANGLE_OPT)
    {
        APUTriangle *tri = &apu->triangle;
        nk_layout_row_dynamic(ctx, 18, 3);
        nk_label(ctx, DebugText("LIN_EN:%02X", tri->linearEnabled), NK_TEXT_LEFT);
        nk_label(ctx, DebugText("LEN_EN:%02X", tri->lengthEnabled), NK_TEXT_LEFT);
        nk_label(ctx, DebugText("PERIOD:%02X", tri->timerPeriod),   NK_TEXT_LEFT);
        DrawAudioWaveform(ctx, canvas, tri->buffer, tri->bufferIndex);
    }
    else if (app.ui.audioOption == NOISE_OPT)
    {
        APUNoise *noise = &apu->noise;
        nk_layout_row_dynamic(ctx, 18, 3);
        nk_label(ctx, DebugText("ENV_EN:%02X", noise->envelopeEnabled), NK_TEXT_LEFT);
        nk_label(ctx, DebugText("PERIOD:%02X", noise->timerPeriod),     NK_TEXT_LEFT);
        nk_label(ctx, DebugText("LEN:%02X",    noise->lengthValue),     NK_TEXT_LEFT);
        DrawAudioWaveform(ctx, canvas, noise->buffer, noise->bufferIndex);
    }
    else if (app.ui.audioOption == DMC_OPT)
    {
        APUDMC *dmc = &apu->dmc;
        nk_layout_row_dynamic(ctx, 18, 3);
        nk_label(ctx, DebugText("ADDR:%02X",   dmc->sampleAddress),  NK_TEXT_LEFT);
        nk_label(ctx, DebugText("LEN:%02X",    dmc->sampleLength),   NK_TEXT_LEFT);
        nk_label(ctx, DebugText("PERIOD:%02X", dmc->timerPeriod),    NK_TEXT_LEFT);
        DrawAudioWaveform(ctx, canvas, dmc->buffer, dmc->bufferIndex);
    }
    else if (app.ui.audioOption == BUFFER_OPT)
    {
        nk_layout_row_dynamic(ctx, 18, 1);
        for (s32 i = 0; i < APU_BUFFER_LENGTH / 16; ++i)
        {
            memset(debugBuffer, 0, sizeof(debugBuffer));
            s32 col = 0;
            for (s32 j = 0; j < 16; ++j)
            {
                if (j > 0) col += sprintf(debugBuffer + col, " ");
                col += sprintf(debugBuffer + col, "%.2f", (f32)apu->buffer[i*16+j]);
            }
            nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
        }
    }
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
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );
    SDL_SetWindowMinimumSize(win, 1024, 768);
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
        struct nk_font *future = nk_font_atlas_add_from_file(atlas, "fonts/consola.ttf", 15, 0);
        nk_sdl_font_stash_end();
        /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
        if (future)
        {
            nk_style_set_font(ctx, &future->handle);
            g_clayNkFont = &future->handle;
        }
    }

    /* ── Clay layout engine init ── */
    {
        uint32_t clayMemSize = Clay_MinMemorySize();
        void *clayMem = malloc(clayMemSize);
        Clay_Arena clayArena = Clay_CreateArenaWithCapacityAndMemory(clayMemSize, clayMem);
        g_clayCtx = Clay_Initialize(
            clayArena,
            (Clay_Dimensions){ (float)win_width, (float)win_height },
            (Clay_ErrorHandler){ ClayHandleError, NULL });
        Clay_SetMeasureTextFunction(ClayMeasureText, (void *)g_clayNkFont);
    }

    /* ── Retro terminal theme ── */
    {
        struct nk_color bg       = nk_rgba(0x0D, 0x0D, 0x0D, 255); /* #0D0D0D */
        struct nk_color panel    = nk_rgba(0x14, 0x14, 0x14, 255); /* #141414 */
        struct nk_color header   = nk_rgba(0x1A, 0x1A, 0x1A, 255); /* #1A1A1A */
        struct nk_color border   = nk_rgba(0x2A, 0x2A, 0x2A, 255); /* #2A2A2A */
        struct nk_color text     = nk_rgba(0xC8, 0xC8, 0xC8, 255); /* #C8C8C8 */
        struct nk_color accent   = nk_rgba(0x33, 0xFF, 0x66, 255); /* #33FF66 */
        struct nk_color accentDk = nk_rgba(0x1A, 0x8C, 0x3A, 255); /* #1A8C3A */
        struct nk_color btnNorm  = nk_rgba(0x1E, 0x1E, 0x1E, 255); /* #1E1E1E */
        struct nk_color btnHover = nk_rgba(0x2A, 0x2A, 0x2A, 255); /* #2A2A2A */
        struct nk_color editBg   = nk_rgba(0x0A, 0x0A, 0x0A, 255); /* #0A0A0A */
        struct nk_color darkText = nk_rgba(0x0D, 0x0D, 0x0D, 255);
        struct nk_color zero     = nk_rgba(0, 0, 0, 0);

        struct nk_style *s = &ctx->style;

        /* Window */
        s->window.fixed_background = nk_style_item_color(panel);
        s->window.background       = panel;
        s->window.border_color     = border;
        s->window.border           = 1.0f;
        s->window.padding          = nk_vec2(6, 6);
        s->window.spacing          = nk_vec2(4, 4);

        /* Window header (Nuklear chrome title bar) */
        s->window.header.normal    = nk_style_item_color(header);
        s->window.header.hover     = nk_style_item_color(header);
        s->window.header.active    = nk_style_item_color(header);
        s->window.header.label_normal = accent;
        s->window.header.label_hover  = accent;
        s->window.header.label_active = accent;

        /* Text */
        s->text.color = text;

        /* Buttons */
        s->button.normal         = nk_style_item_color(btnNorm);
        s->button.hover          = nk_style_item_color(btnHover);
        s->button.active         = nk_style_item_color(accent);
        s->button.border_color   = border;
        s->button.text_background= btnNorm;
        s->button.text_normal    = text;
        s->button.text_hover     = accent;
        s->button.text_active    = darkText;
        s->button.border         = 1.0f;
        s->button.rounding       = 0.0f;
        s->button.padding        = nk_vec2(4, 2);

        /* Checkbox */
        s->checkbox.normal       = nk_style_item_color(btnNorm);
        s->checkbox.hover        = nk_style_item_color(btnHover);
        s->checkbox.active       = nk_style_item_color(accent);
        s->checkbox.border_color = border;
        s->checkbox.text_normal  = text;
        s->checkbox.text_hover   = accent;
        s->checkbox.text_active  = text;
        s->checkbox.cursor_normal= nk_style_item_color(accent);
        s->checkbox.cursor_hover = nk_style_item_color(accent);
        s->checkbox.border       = 1.0f;

        /* Option (radio) */
        s->option.normal         = nk_style_item_color(btnNorm);
        s->option.hover          = nk_style_item_color(btnHover);
        s->option.active         = nk_style_item_color(accent);
        s->option.border_color   = border;
        s->option.text_normal    = text;
        s->option.text_hover     = accent;
        s->option.text_active    = text;
        s->option.cursor_normal  = nk_style_item_color(accent);
        s->option.cursor_hover   = nk_style_item_color(accent);
        s->option.border         = 1.0f;

        /* Edit fields */
        s->edit.normal           = nk_style_item_color(editBg);
        s->edit.hover            = nk_style_item_color(editBg);
        s->edit.active           = nk_style_item_color(editBg);
        s->edit.border_color     = border;
        s->edit.cursor_normal    = accent;
        s->edit.cursor_hover     = accent;
        s->edit.cursor_text_normal = darkText;
        s->edit.cursor_text_hover  = darkText;
        s->edit.text_normal      = text;
        s->edit.text_hover       = text;
        s->edit.text_active      = text;
        s->edit.selected_normal  = accentDk;
        s->edit.selected_hover   = accentDk;
        s->edit.selected_text_normal = text;
        s->edit.selected_text_hover  = text;
        s->edit.border           = 1.0f;
        s->edit.padding          = nk_vec2(4, 2);

        /* Scrollbar */
        s->scrollv.normal        = nk_style_item_color(btnNorm);
        s->scrollv.hover         = nk_style_item_color(btnHover);
        s->scrollv.active        = nk_style_item_color(btnHover);
        s->scrollv.cursor_normal = nk_style_item_color(accentDk);
        s->scrollv.cursor_hover  = nk_style_item_color(accent);
        s->scrollv.cursor_active = nk_style_item_color(accent);
        s->scrollv.border_color  = border;
        s->scrollv.cursor_border_color = border;
        s->scrollv.border        = 1.0f;
        s->scrollv.rounding      = 0.0f;

        s->scrollh = s->scrollv;

        /* Selectable (used in lists) */
        s->selectable.normal         = nk_style_item_color(panel);
        s->selectable.hover          = nk_style_item_color(btnHover);
        s->selectable.pressed        = nk_style_item_color(accent);
        s->selectable.normal_active  = nk_style_item_color(accentDk);
        s->selectable.hover_active   = nk_style_item_color(accent);
        s->selectable.pressed_active = nk_style_item_color(accent);
        s->selectable.text_normal    = text;
        s->selectable.text_hover     = accent;
        s->selectable.text_pressed   = darkText;
        s->selectable.text_normal_active  = text;
        s->selectable.text_hover_active   = darkText;
        s->selectable.text_pressed_active = darkText;

        /* Progress bar */
        s->progress.normal       = nk_style_item_color(btnNorm);
        s->progress.hover        = nk_style_item_color(btnNorm);
        s->progress.active       = nk_style_item_color(btnNorm);
        s->progress.cursor_normal= nk_style_item_color(accent);
        s->progress.cursor_hover = nk_style_item_color(accent);
        s->progress.cursor_active= nk_style_item_color(accent);
        s->progress.border_color = border;
        s->progress.border       = 1.0f;
        s->progress.rounding     = 0.0f;

        (void)bg; (void)zero;
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

            if (evt.type == SDL_KEYDOWN)
            {
                if (evt.key.keysym.sym == SDLK_F11 || 
                   (evt.key.keysym.sym == SDLK_RETURN && (evt.key.keysym.mod & KMOD_ALT)))
                {
                    Uint32 flags = SDL_GetWindowFlags(win);
                    if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
                        SDL_SetWindowFullscreen(win, 0);
                    else
                        SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    continue;
                }
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
        SDL_GetWindowSize(win, &win_width, &win_height);

        /* Forward mouse state to Clay before layout pass */
        {
            int mx, my;
            Uint32 mbtn = SDL_GetMouseState(&mx, &my);
            bool mouseDown = (mbtn & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
            Clay_SetPointerState(
                (Clay_Vector2){ (float)mx, (float)my },
                mouseDown);
            Clay_UpdateScrollContainers(false,
                (Clay_Vector2){ 0, 0 },
                dt);
        }

        Clay_SetLayoutDimensions((Clay_Dimensions){ (float)win_width, (float)win_height });
        Clay_RenderCommandArray clayCmds = BuildClayLayout(win_width, win_height, dt);
        Clay_NkRender(ctx, clayCmds, win_width, win_height, &device, win, dt, d1, d2, &initialCounter);


        

        

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
