#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

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
#include "ui.h"

#define nes (app.runtime.nes)

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

AppState app = {0};

global PFNGLGENERATEMIPMAPPROC GLGenerateMipmap;

internal void* LoadGLProc(const char* name)
{
    void* proc = SDL_GL_GetProcAddress(name);
    ASSERT(proc);
    return proc;
}

internal void LoadGLFunctions(void)
{
    GLGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)LoadGLProc("glGenerateMipmap");
}

internal GLuint InitTexture(GLsizei width, GLsizei height)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    if (GLGenerateMipmap) GLGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

internal void SetupDevice(Device* device)
{
    device->screen = InitTexture(256, 240);
    device->patterns[0] = InitTexture(128, 128);
    device->patterns[1] = InitTexture(128, 128);
    device->patternHover = InitTexture(8, 8);
    for (s32 i = 0; i < 64; ++i)
        device->oam[i] = InitTexture(8, 16);
    for (s32 i = 0; i < 8; ++i)
        device->oam2[i] = InitTexture(8, 16);
    device->nametable = InitTexture(256, 240);
    for (s32 i = 0; i < 960; ++i)
        device->nametable2[i] = InitTexture(8, 8);
}

internal inline f32 GetSecondsElapsed(u64 start, u64 end)
{
    f32 result = ((f32)(end - start) / (f32)globalPerfCountFrequency);
    return result;
}

internal void CopyString(char* dest, size_t destSize, const char* src)
{
    if (!destSize) return;
    if (!src) {
        dest[0] = 0;
        return;
    }
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = 0;
}

internal b32 EndsWith(const char* text, const char* suffix)
{
    size_t textLen = strlen(text);
    size_t suffixLen = strlen(suffix);
    if (textLen < suffixLen) return FALSE;
    return strcmp(text + textLen - suffixLen, suffix) == 0;
}

internal const char* GetFilenamePart(const char* path)
{
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    const char* name = path;
    if (slash && (!backslash || slash > backslash)) name = slash + 1;
    else if (backslash) name = backslash + 1;
    return name;
}

internal void UpdateWindowTitle(SDL_Window* win, const char* path)
{
    char windowTitle[256] = "Nes emulator";
    if (path && path[0]) snprintf(windowTitle, sizeof(windowTitle), "Nes emulator: %s", GetFilenamePart(path));
    SDL_SetWindowTitle(win, windowTitle);
}

internal void BuildSavePath(const char* sourcePath, char* dest, size_t destSize)
{
    const char* extension = strrchr(sourcePath, '.');
    size_t prefixLength = extension ? (size_t)(extension - sourcePath) : strlen(sourcePath);
    if (prefixLength >= destSize) prefixLength = destSize - 1;
    memcpy(dest, sourcePath, prefixLength);
    dest[prefixLength] = 0;
    strncat(dest, ".nsave", destSize - strlen(dest) - 1);
}

internal b32 LoadFileIntoApp(SDL_Window* win, const char* path)
{
    if (!path || !path[0]) return FALSE;
    if (EndsWith(path, ".nes")) {
        Cartridge cartridge = {};
        if (!LoadNesRom((char*)path, &cartridge)) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "The file couldn't be loaded!", win);
            return FALSE;
        }
        if (nes) Destroy(nes);
        nes = CreateNES(cartridge);
        if (!nes) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unsupported mapper",
                                     "This ROM uses a mapper that is not implemented yet.", win);
            return FALSE;
        }
        CopyString(loadedFilePath, sizeof(loadedFilePath), path);
        BuildSavePath(path, saveFilePath, sizeof(saveFilePath));
        UpdateWindowTitle(win, path);
        return TRUE;
    }
    if (EndsWith(path, ".nsave")) {
        NES* loaded = LoadNESSave((char*)path);
        if (!loaded) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "The file couldn't be loaded!", win);
            return FALSE;
        }
        if (nes) Destroy(nes);
        nes = loaded;
        CopyString(loadedFilePath, sizeof(loadedFilePath), path);
        CopyString(saveFilePath, sizeof(saveFilePath), path);
        if (nes->cartridge.path[0]) CopyString(loadedFilePath, sizeof(loadedFilePath), nes->cartridge.path);
        UpdateWindowTitle(win, loadedFilePath);
        return TRUE;
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Only .nes and .nsave files are supported.", win);
    return FALSE;
}

internal void QueueAudioBuffer(APU* apu, SDL_AudioDeviceID audioDeviceId, SDL_AudioStream* audioStream)
{
    if (!audioDeviceId) return;
    s32 bytesToQueue = apu->bufferIndex * APU_BYTES_PER_SAMPLE;

// Queue backpressure: even with a perfectly-timed sample accumulator the
// host audio clock and the emulation clock are never 100% identical.  If
// we push audio unconditionally, small timing jitter accumulates in SDL's
// internal buffer and creates ever-growing latency.
//
// Strategy: measure how many bytes are already queued and waiting.  If the
// queue already holds more than ~3 device-buffer periods of audio, skip
// this push entirely.  SDL will drain the existing queue on its own.
//
// Target: want.samples = 1024 frames, 3 device periods = 3072 samples.
// At 16-bit mono that is 3072 * 2 = 6144 bytes (~64 ms ceiling).
// This keeps latency bounded while absorbing normal frame-time jitter.
//
// Note: SDL_GetQueuedAudioSize returns bytes already queued but not yet
// consumed by the hardware. We compare in bytes to avoid an extra divide.
#define APU_MAX_QUEUED_BYTES (1024 * APU_BYTES_PER_SAMPLE * 3)

    u32 queuedBytes = SDL_GetQueuedAudioSize(audioDeviceId);
    if (queuedBytes > APU_MAX_QUEUED_BYTES) return; // let hardware catch up

    if (audioStream) {
        if (SDL_AudioStreamPut(audioStream, apu->buffer, bytesToQueue) == 0) {
            s32 convertedBytes = SDL_AudioStreamAvailable(audioStream);
            if (convertedBytes > 0) {
                u8* convertedBuffer = (u8*)SDL_malloc(convertedBytes);
                if (convertedBuffer) {
                    s32 readBytes = SDL_AudioStreamGet(audioStream, convertedBuffer, convertedBytes);
                    if (readBytes > 0) SDL_QueueAudio(audioDeviceId, convertedBuffer, readBytes);
                    SDL_free(convertedBuffer);
                }
            }
        }
    } else SDL_QueueAudio(audioDeviceId, apu->buffer, bytesToQueue);
}

internal void UpdateControllerInput(SDL_GameController* controller)
{
    const Uint8* keyboard = SDL_GetKeyboardState(NULL);

    SetButton(nes, 0, BUTTON_UP, keyboard[SDL_SCANCODE_UP] || coarseButtons[1]);
    SetButton(nes, 0, BUTTON_DOWN, keyboard[SDL_SCANCODE_DOWN] || coarseButtons[3]);
    SetButton(nes, 0, BUTTON_LEFT, keyboard[SDL_SCANCODE_LEFT] || coarseButtons[0]);
    SetButton(nes, 0, BUTTON_RIGHT, keyboard[SDL_SCANCODE_RIGHT] || coarseButtons[2]);
    SetButton(nes, 0, BUTTON_SELECT, keyboard[SDL_SCANCODE_SPACE] || coarseButtons[4]);
    SetButton(nes, 0, BUTTON_START, keyboard[SDL_SCANCODE_RETURN] || coarseButtons[5]);
    SetButton(nes, 0, BUTTON_B, keyboard[SDL_SCANCODE_S] || coarseButtons[6]);
    SetButton(nes, 0, BUTTON_A, keyboard[SDL_SCANCODE_A] || coarseButtons[7]);

    if (controller) {
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A)) SetButton(nes, 0, BUTTON_A, TRUE);
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X)) SetButton(nes, 0, BUTTON_B, TRUE);
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START)) SetButton(nes, 0, BUTTON_START, TRUE);
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK)) SetButton(nes, 0, BUTTON_SELECT, TRUE);
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP)) SetButton(nes, 0, BUTTON_UP, TRUE);
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            SetButton(nes, 0, BUTTON_DOWN, TRUE);
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            SetButton(nes, 0, BUTTON_LEFT, TRUE);
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            SetButton(nes, 0, BUTTON_RIGHT, TRUE);
    }
}

int main(int argc, char** argv)
{
    SDL_Window* win;
    SDL_GLContext glContext;
    int windowWidth, windowHeight;
    b32 quit = FALSE;
    f32 dt = 0;

    struct Device device;
    SDL_GameController* controller = NULL;

    debugging = TRUE;
    debugMode = TRUE;
    app.ui.debugToggle = TRUE;
    app.ui.square1Enabled = TRUE;
    app.ui.square2Enabled = TRUE;
    app.ui.triangleEnabled = TRUE;
    app.ui.noiseEnabled = TRUE;
    app.ui.dmcEnabled = TRUE;
    app.ui.hpFilter1Enabled = TRUE;
    app.ui.hpFilter1Freq = 90;
    app.ui.hpFilter2Enabled = TRUE;
    app.ui.hpFilter2Freq = 440;
    app.ui.lpFilterEnabled = TRUE;
    app.ui.lpFilterFreq = 14000;
    globalPerfCountFrequency = SDL_GetPerformanceFrequency();

    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "1");

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK |
             SDL_INIT_GAMECONTROLLER);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    win = SDL_CreateWindow("NES Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

    glContext = SDL_GL_CreateContext(win);
    SDL_GL_MakeCurrent(win, glContext);
    SDL_GL_SetSwapInterval(0); // v-sync

    LoadGLFunctions();
    SetupDevice(&device);

    igCreateContext(NULL);
    ImGuiIO* io = igGetIO_Nil();
    (void)io;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    SetupImGuiStyle();

    ImGui_ImplSDL2_InitForOpenGL(win, glContext);
    ImGui_ImplOpenGL3_Init("#version 130");

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = APU_SAMPLES_PER_SECOND;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = NULL;

    SDL_AudioDeviceID audioDeviceId = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (audioDeviceId == 0) {
        printf("Failed to open audio: %s\n", SDL_GetError());
    }

    SDL_AudioStream* audioStream =
        SDL_NewAudioStream(AUDIO_S16SYS, 1, APU_SAMPLES_PER_SECOND, have.format, have.channels, have.freq);
    if (!audioStream) {
        printf("Failed to create audio stream: %s\n", SDL_GetError());
    }

    SDL_PauseAudioDevice(audioDeviceId, 0);

    for (s32 i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) break;
        }
    }

    if (argc > 1) {
        LoadFileIntoApp(win, argv[1]);
    }

    u64 startCounter = SDL_GetPerformanceCounter();

    // Frame cycle accumulator — carries the sub-cycle fractional remainder
    // across frames so it is never permanently lost.
    //
    // The NES PPU runs 341 cycles × 262 scanlines = 89342 PPU ticks per frame.
    // The CPU runs at PPU/3, so the exact CPU cycles per frame is:
    //   89342 / 3 = 29780.666... cycles
    // Truncating to 29780 drops 0.666 cycles per frame = ~40 cycles/sec lost.
    // Although small, compounding over millions of frames matters for accuracy.
    //
    // Solution: accumulate the integer part (0.666... * 3 = 2 remainder over
    // every 3 frames). We use the same numerator/denominator approach as the
    // APU accumulator: add PPU_CYCLES_PER_FRAME every frame, take the integer
    // CPU part, subtract PPU_CYCLES_PER_FRAME from the running total.
    // In practice: base = 29780, every 3 frames one frame gets 29781 cycles.
    s32 cycleRemainder = 0; // accumulates sub-integer CPU cycles across frames

    while (!quit) {
        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            ImGui_ImplSDL2_ProcessEvent(&evt);

            if (evt.type == SDL_QUIT) quit = TRUE;

            if (evt.type == SDL_DROPFILE) {
                LoadFileIntoApp(win, evt.drop.file);
                SDL_free(evt.drop.file);
            }

            if (evt.type == SDL_WINDOWEVENT && evt.window.event == SDL_WINDOWEVENT_CLOSE &&
                evt.window.windowID == SDL_GetWindowID(win)) {
                quit = TRUE;
            }
        }

        if (nes) {
            UpdateControllerInput(controller);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        igNewFrame();

        if (nes) {
            // Exact NTSC CPU cycles per frame derived from PPU geometry:
            //   PPU cycles/frame = PPU_CYCLES_PER_SCANLINE * PPU_SCANLINES_PER_FRAME
            //                    = 341 * 262 = 89342
            //   CPU cycles/frame = 89342 / 3 = 29780.666...
            //
            // The old value was 0.0167 * CPU_FREQ = 29889 — 109 cycles/frame
            // too many, making the emulator run at ~59.88 fps instead of the
            // correct ~60.099 fps (NTSC).  Over-counted cycles also mean more
            // audio samples are generated per wall-clock second, compounding
            // the APU drift described in apu.h.
            //
            // cycleRemainder tracks the fractional cycles not yet assigned.
            // 89342 mod 3 = 2, so the remainder grows by 2 per frame.
            // When it accumulates to >= 3 we add one extra CPU cycle that frame
            // and subtract 3 from the remainder.  This keeps the long-run
            // average at exactly 29780.666... cycles/frame with no float math.
            cycleRemainder += (PPU_CYCLES_PER_SCANLINE * PPU_SCANLINES_PER_FRAME) % 3; // += 2
            s32 cycles = (PPU_CYCLES_PER_SCANLINE * PPU_SCANLINES_PER_FRAME) / 3;      // 29780
            if (cycleRemainder >= 3) {
                cycles++;
                cycleRemainder -= 3;
            }

            if (stepping || oneCycleAtTime) {
                cycles = 1;
            }

            nes->apu.bufferIndex = 0;
            nes->apu.pulse1.bufferIndex = 0;
            nes->apu.pulse2.bufferIndex = 0;
            nes->apu.triangle.bufferIndex = 0;
            nes->apu.noise.bufferIndex = 0;
            nes->apu.dmc.bufferIndex = 0;

            while (cycles > 0) {
                if (!debugging) {
                    if (!hitRun) {
                        if (nes->cpu.pc == breakpoint) {
                            debugging = TRUE;
                            stepping = FALSE;
                        }
                    } else {
                        hitRun = FALSE;
                    }
                }

                if (debugging && !stepping) break;

                CPUStep step = StepCPU(nes);
                cycles -= step.cycles;

                if (debugging) stepping = FALSE;
            }

            QueueAudioBuffer(&nes->apu, audioDeviceId, audioStream);
        }

        SDL_GetWindowSize(win, &windowWidth, &windowHeight);

        DrawUI(win, &device, dt);

        glViewport(0, 0, windowWidth, windowHeight);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
        SDL_GL_SwapWindow(win);

        u64 endCounter = SDL_GetPerformanceCounter();
        f32 secondsElapsed = GetSecondsElapsed(startCounter, endCounter);

// True NTSC frame duration: 1 / (CPU_FREQ / (341 * 262 / 3))
//   = (341 * 262) / (3 * CPU_FREQ)
//   = 89342 / 5369319
//   ≈ 0.016639 s  (16.639 ms, NOT 16.7 ms)
//
// Using 0.0167 s over-waits by ~0.061 ms per frame = ~3.7 ms/sec.
// Combined with the APU over-sampling this worsened audio drift.
// The exact expression avoids a magic number and stays in sync with
// any future changes to CPU_FREQ or PPU constants.
#define NES_FRAME_DURATION_S ((f32)(PPU_CYCLES_PER_SCANLINE * PPU_SCANLINES_PER_FRAME) / (3.0f * CPU_FREQ))

        while (secondsElapsed < NES_FRAME_DURATION_S) {
            endCounter = SDL_GetPerformanceCounter();
            secondsElapsed = GetSecondsElapsed(startCounter, endCounter);
        }

        dt = GetSecondsElapsed(startCounter, endCounter);
        startCounter = endCounter;
    }

    if (audioDeviceId) SDL_CloseAudioDevice(audioDeviceId);
    if (audioStream) SDL_FreeAudioStream(audioStream);
    if (controller) SDL_GameControllerClose(controller);
    if (nes) {
        Save(nes, saveFilePath);
        Destroy(nes);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
