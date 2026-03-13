#ifndef UI_H
#define UI_H

#include "types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

/* App State */
typedef struct RuntimeState RuntimeState;
struct RuntimeState {
    s64 perfCountFrequency;
    struct NES* nes;
    char loadedFilePath[1024];
    char saveFilePath[1024];
};

typedef struct EmuControlState EmuControlState;
struct EmuControlState {
    b32 hitRun;
    b32 debugging;
    b32 stepping;
    u16 breakpoint;
};

typedef struct UiState UiState;
struct UiState {
    char debugBuffer[256];

    b32 coarseButtons[8];
    b32 oneCycleAtTime;
    b32 debugMode;

    b32 leftSidebarCollapsed;
    b32 rightSidebarCollapsed;
    s32 leftSidebarTab;
    s32 rightSidebarTab;

    b32 oneCycleToggle;
    b32 debugToggle;

    char instructionAddressText[12];
    char instructionBreakpointText[5];

    s32 debugToolTab;
    s32 memoryOption;
    char memoryAddressText[12];

    s32 videoOption;
    s32 nametableOption;
    b32 showSeparatePixels;

    s32 audioOption;
    b32 square1Enabled;
    b32 square2Enabled;
    b32 triangleEnabled;
    b32 noiseEnabled;
    b32 dmcEnabled;

    b32 hpFilter1Enabled;
    s32 hpFilter1Freq;
    b32 hpFilter2Enabled;
    s32 hpFilter2Freq;
    b32 lpFilterEnabled;
    s32 lpFilterFreq;
};

typedef struct AppState AppState;
struct AppState {
    RuntimeState runtime;
    EmuControlState control;
    UiState ui;
};

extern AppState app;

#define globalPerfCountFrequency (app.runtime.perfCountFrequency)
#define debugBuffer (app.ui.debugBuffer)
#define hitRun (app.control.hitRun)
#define debugging (app.control.debugging)
#define stepping (app.control.stepping)
#define breakpoint (app.control.breakpoint)
#define coarseButtons (app.ui.coarseButtons)
#define oneCycleAtTime (app.ui.oneCycleAtTime)
#define debugMode (app.ui.debugMode)
#define loadedFilePath (app.runtime.loadedFilePath)
#define saveFilePath (app.runtime.saveFilePath)

/* Textures */
#define NUM_TEXTURES 1 + 2 + 1 + 64 + 8 + 1 + 960

typedef struct Device Device;
struct Device {
    GLuint screen;
    GLuint patterns[2];
    GLuint patternHover;
    GLuint oam[64];
    GLuint oam2[8];
    GLuint nametable;
    GLuint nametable2[960];
};

void SetupImGuiStyle();
void DrawUI(SDL_Window* win, Device* device, f32 dt);

#endif
