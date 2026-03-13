#ifndef UI_H
#define UI_H

#include "types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

/* App State */
typedef struct RuntimeState {
    s64 perfCountFrequency;
    struct NES* nes;
    char loadedFilePath[1024];
    char saveFilePath[1024];
} RuntimeState;

typedef struct EmuControlState {
    bool hitRun;
    bool debugging;
    bool stepping;
    u16 breakpoint;
} EmuControlState;

typedef struct UiState {
    char debugBuffer[256];

    bool coarseButtons[8];
    bool oneCycleAtTime;
    bool debugMode;

    bool leftSidebarCollapsed;
    bool rightSidebarCollapsed;
    s32 leftSidebarTab;
    s32 rightSidebarTab;

    bool oneCycleToggle;
    bool debugToggle;

    char instructionAddressText[12];
    char instructionBreakpointText[5];

    s32 debugToolTab;
    s32 memoryOption;
    char memoryAddressText[12];

    s32 videoOption;
    s32 nametableOption;
    bool showSeparatePixels;

    s32 audioOption;
    bool square1Enabled;
    bool square2Enabled;
    bool triangleEnabled;
    bool noiseEnabled;
    bool dmcEnabled;

    bool hpFilter1Enabled;
    s32 hpFilter1Freq;
    bool hpFilter2Enabled;
    s32 hpFilter2Freq;
    bool lpFilterEnabled;
    s32 lpFilterFreq;
} UiState;

typedef struct AppState {
    RuntimeState runtime;
    EmuControlState control;
    UiState ui;
} AppState;

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

typedef struct Device {
    GLuint screen;
    GLuint patterns[2];
    GLuint patternHover;
    GLuint oam[64];
    GLuint oam2[8];
    GLuint nametable;
    GLuint nametable2[960];
} Device;

void SetupImGuiStyle();
void DrawUI(SDL_Window* win, Device* device, f32 dt);

#endif
