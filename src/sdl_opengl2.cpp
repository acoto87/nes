#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <varargs.h>
#include <commdlg.h>

#include "utils.h"
#include "types.h"
#include "cartridge.h"
#include "nes.cpp"

#include <SDL2-2.0.5\include\SDL.h>
#undef main

#include <glew32\include\glew.h>
#include <SDL2-2.0.5\include\SDL_opengl.h>
#include <SDL2-2.0.5\include\SDL_syswm.h>
#include <SDL2-2.0.5\include\SDL_audio.h>

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

global s64 globalPerfCountFrequency;
global b32 running;
global b32 needs_refresh = TRUE;
global char debugBuffer[256];
global b32 hitRun;
global b32 debugging = TRUE;
global b32 stepping;
global u16 breakpoint;
global b32 coarseButtons[8];
global b32 oneCycleAtTime;
global b32 debugMode;

global NES *nes;

struct Device {
    struct nk_buffer cmds;
    GLuint vbo, vao, ebo;
    /* GLuint prog;
     GLuint vert_shdr;
     GLuint frag_shdr;
     GLint attrib_pos;
     GLint attrib_uv;
     GLint attrib_col;
     GLint uniform_tex;
     GLint uniform_proj;*/

    struct nk_image screen;
    struct nk_image patterns[2];
    struct nk_image patternHover;
    struct nk_image oam[64];
    struct nk_image oam2[8];
    struct nk_image nametable;
    struct nk_image nametable2[960];
};

#define ONE_MINUTE_OF_SOUND (48000 * 60 * 2 * 8)

// struct WavAudio
// {
//     /* RIFF identifier */
//     char riff[5] = "RIFF";
//     /* file length */
//     u32 length = 36 + ONE_MINUTE_OF_SOUND;
//     /* RIFF type */
//     char format[5] = "WAVE";

//     /* format chunk identifier */
//     char fmt[5] = "fmt ";
//     /* format chunk length */
//     u32 chunkLength = 16;
//     /* sample format (raw PCM) */
//     u16 sampleFormat = 1;
//     /* channel count */
//     u16 channelCount = 2;
//     /* sample rate */
//     u32 sampleRate = 48000;
//     /* byte rate (sample rate * block align) */
//     u32 byteRate = 48000 * 2 * 4;
//     /* block align (channel count * bytes per sample) */
//     u16 blockAlign = 2 * 4;
//     /* bits per sample */
//     u16 bitsPerSample = 32;

//     /* data chunk identifier */
//     char view[5] = "data";
//     /* data chunk length */
//     u32 dataLength = ONE_MINUTE_OF_SOUND; // one minute of sound
//     /* data */
//     u8 data[ONE_MINUTE_OF_SOUND];
// };

internal inline LARGE_INTEGER Win32GetWallClock(void)
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

internal inline f32 Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    f32 result = ((f32)(end.QuadPart - start.QuadPart) / (f32)globalPerfCountFrequency);
    return result;
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
    /*GLint status;

    static const GLchar *vertex_shader =
        NK_SHADER_VERSION
        "uniform mat4 ProjMtx;\n"
        "in vec2 Position;\n"
        "in vec2 TexCoord;\n"
        "in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main() {\n"
        "   Frag_UV = TexCoord;\n"
        "   Frag_Color = Color;\n"
        "   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
        "}\n";

    static const GLchar *fragment_shader =
        NK_SHADER_VERSION
        "precision mediump float;\n"
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main(){\n"
        "   Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
        "}\n";*/

    nk_buffer_init_default(&dev->cmds);
    /*dev->prog = glCreateProgram();
    dev->vert_shdr = glCreateShader(GL_VERTEX_SHADER);
    dev->frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);*/
    /*glShaderSource(dev->vert_shdr, 1, &vertex_shader, 0);
    glShaderSource(dev->frag_shdr, 1, &fragment_shader, 0);
    glCompileShader(dev->vert_shdr);
    glCompileShader(dev->frag_shdr);
    glGetShaderiv(dev->vert_shdr, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);
    glGetShaderiv(dev->frag_shdr, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);
    glAttachShader(dev->prog, dev->vert_shdr);
    glAttachShader(dev->prog, dev->frag_shdr);
    glLinkProgram(dev->prog);
    glGetProgramiv(dev->prog, GL_LINK_STATUS, &status);
    assert(status == GL_TRUE);*/

    /*dev->uniform_tex = glGetUniformLocation(dev->prog, "Texture");
    dev->uniform_proj = glGetUniformLocation(dev->prog, "ProjMtx");
    dev->attrib_pos = glGetAttribLocation(dev->prog, "Position");
    dev->attrib_uv = glGetAttribLocation(dev->prog, "TexCoord");
    dev->attrib_col = glGetAttribLocation(dev->prog, "Color");*/

    {
        /* buffer setup */
        GLsizei vs = sizeof(struct nk_sdl_vertex);
        size_t vp = offsetof(struct nk_sdl_vertex, position);
        size_t vt = offsetof(struct nk_sdl_vertex, uv);
        size_t vc = offsetof(struct nk_sdl_vertex, col);

        glGenBuffers(1, &dev->vbo);
        glGenBuffers(1, &dev->ebo);
        glGenVertexArrays(1, &dev->vao);

        glBindVertexArray(dev->vao);
        glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

        /*glEnableVertexAttribArray((GLuint)dev->attrib_pos);
        glEnableVertexAttribArray((GLuint)dev->attrib_uv);
        glEnableVertexAttribArray((GLuint)dev->attrib_col);

        glVertexAttribPointer((GLuint)dev->attrib_pos, 2, GL_FLOAT, GL_FALSE, vs, (void*)vp);
        glVertexAttribPointer((GLuint)dev->attrib_uv, 2, GL_FLOAT, GL_FALSE, vs, (void*)vt);
        glVertexAttribPointer((GLuint)dev->attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (void*)vc);*/
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

internal struct nk_image InitTexture(GLuint tex, GLsizei width, GLsizei height)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenerateMipmap(GL_TEXTURE_2D);
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
}

internal void SDLAudioCallback(void* userdata, u8* buffer, s32 len)
{
    memset(buffer, 0, len);

    if (nes)
    {
        APU *apu = &nes->apu;

        f32 *sampleOut = (f32*)buffer;
        for (s32 i = 0; i < len / APU_BYTES_PER_SAMPLE; i++)
        {
            f32 sampleValue = apu->buffer[i];
            sampleOut[2 * i + 0] = sampleValue;
            sampleOut[2 * i + 1] = sampleValue;
        }
    }

    /*local s32 samplesPerSecond = 48000;
    local s32 toneHz = 256;
    local s32 wavePeriod = samplesPerSecond / toneHz;
    local s32 bytesPerSample = sizeof(f32) * 2;
    local s32 toneVolume = 3000;
    local u32 wavePos = 0;

    #define PI 3.14159265359f

    f32 *sampleOut = (f32*)buffer;
    for (s32 i = 0; i < len / bytesPerSample; i++)
    {
        f32 t = 2.0f * PI * (f32)wavePos / (f32)wavePeriod;
        f32 sineValue = sinf(t);
        f32 sampleValue = sineValue * toneVolume;
        sampleOut[2 * i + 0] = sampleValue;
        sampleOut[2 * i + 1] = sampleValue;

        ++wavePos;
    }*/
}

int CALLBACK WinMain(
    HINSTANCE instance,
    HINSTANCE prevInstance,
    LPSTR     cmdLine,
    int       cmdShow)
{
    /* Platform */
    SDL_Window *win;
    SDL_SysWMinfo wmInfo;
    SDL_GLContext glContext;
    HWND winHwnd = NULL;
    int win_width, win_height;
    b32 running, quit = FALSE;
    f32 dt = 0;

    LARGE_INTEGER initialCounter;
    LARGE_INTEGER perfCountFrequencyResult;

    /* GUI */
    struct nk_context *ctx;
    struct Device device;
    SDL_GameController *controller = NULL;

    initialCounter = Win32GetWallClock();

    QueryPerformanceFrequency(&perfCountFrequencyResult);
    globalPerfCountFrequency = perfCountFrequencyResult.QuadPart;

    char windowTitle[256] = "Nes emulator";

    /* SDL setup */
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    win = SDL_CreateWindow(windowTitle,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    glContext = SDL_GL_CreateContext(win);
    SDL_GetWindowSize(win, &win_width, &win_height);

    glewExperimental = GL_TRUE;
    glewInit();

    // initialize info structure with SDL version info
    SDL_VERSION(&wmInfo.version);

    if (SDL_GetWindowWMInfo(win, &wmInfo))
    {
        winHwnd = wmInfo.info.win.window;
    }

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
        struct nk_font *future = nk_font_atlas_add_from_file(atlas, "D://Work/nes/sublime/fonts/consola.ttf", 14, 0);
        nk_sdl_font_stash_end();
        /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
        nk_style_set_font(ctx, &future->handle);
    }

    InitDevice(&device);
    InitTextures(&device);

    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;

    SDL_memset(&want, 0, sizeof(want));
    want.freq = APU_SAMPLES_PER_SECOND;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = NULL;// SDLAudioCallback;

    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (dev)
    {
        if (have.format != want.format) {
            SDL_Log("We didn't get Single32 audio format.");
        }
        SDL_PauseAudioDevice(dev, 0);
    }

    // DEBUG: cleanup
    shl_wave_file wave_file;
    shl_wave_init( &wave_file, 48000, "nes_audio.wav", 0, 0);

    LARGE_INTEGER startCounter = Win32GetWallClock();
    dt = Win32GetSecondsElapsed(initialCounter, startCounter);
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
            nk_sdl_handle_event(&evt);
        }
        nk_input_end(ctx);

        if (quit)
        {
            break;
        }

        LARGE_INTEGER s = Win32GetWallClock();

        if (nes)
        {
            CPU *cpu = &nes->cpu;
            PPU *ppu = &nes->ppu;
            APU *apu = &nes->apu;

            SetButton(nes, 0, BUTTON_UP, ctx->input.keyboard.keys[NK_KEY_UP].down || coarseButtons[1]);
            SetButton(nes, 0, BUTTON_DOWN, ctx->input.keyboard.keys[NK_KEY_DOWN].down || coarseButtons[3]);
            SetButton(nes, 0, BUTTON_LEFT, ctx->input.keyboard.keys[NK_KEY_LEFT].down || coarseButtons[0]);
            SetButton(nes, 0, BUTTON_RIGHT, ctx->input.keyboard.keys[NK_KEY_RIGHT].down || coarseButtons[2]);
            SetButton(nes, 0, BUTTON_SELECT, ctx->input.keyboard.keys[NK_KEY_SPACE].down || coarseButtons[4]);
            SetButton(nes, 0, BUTTON_START, ctx->input.keyboard.keys[NK_KEY_ENTER].down || coarseButtons[5]);
            SetButton(nes, 0, BUTTON_B, ctx->input.keyboard.keys[NK_KEY_S].down || coarseButtons[6]);
            SetButton(nes, 0, BUTTON_A, ctx->input.keyboard.keys[NK_KEY_A].down || coarseButtons[7]);

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

                /*for (s32 i = 0; i < 3 * step.cycles; i++)
                {
                    StepPPU(nes);
                }*/

                for (s32 i = 0; i < step.cycles; i++)
                {
                    StepAPU(nes);
                }

                cycles -= step.cycles;

                if (debugging)
                {
                    stepping = FALSE;
                }
            }

            // DEBUG: cleanup
            shl_wave_write( &wave_file, apu->buffer, apu->bufferIndex, 1 );

            SDL_QueueAudio(dev, apu->buffer, apu->bufferIndex * APU_BYTES_PER_SAMPLE);
        }

        LARGE_INTEGER e = Win32GetWallClock();
        d1 = Win32GetSecondsElapsed(s, e);

        s = Win32GetWallClock();

        /* GUI */
        nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE;

        if (nk_begin(ctx, "FPS INFO", nk_rect(10, 10, 290, 190), flags))
        {
            local s32 fps = 0;
            local s32 fpsCount = 0;
            local s32 oneCycle = 0;
            local s32 debug = 0;

            nk_layout_row_dynamic(ctx, 20, 2);

            LARGE_INTEGER fpsCounter = Win32GetWallClock();
            f32 fpsdt = Win32GetSecondsElapsed(initialCounter, fpsCounter);
            if (fpsdt > 1)
            {
                initialCounter = fpsCounter;
                fps = fpsCount;
                fpsCount = 0;
            }

            ++fpsCount;

            nk_label(ctx, DebugText("fps: %d", fps), NK_TEXT_LEFT);
            nk_label(ctx, DebugText("dt: %.4f", dt), NK_TEXT_LEFT);

            nk_checkbox_label(ctx, "debug mode", &debug);
            debugMode = debug;

            if (debugMode)
            {
                nk_checkbox_label(ctx, "1 cycle", &oneCycle);
                oneCycleAtTime = oneCycle;

                nk_label(ctx, DebugText("d1: %.4f", d1), NK_TEXT_LEFT);
                nk_label(ctx, DebugText("d2: %.4f", d2), NK_TEXT_LEFT);
            }

            nk_layout_row_dynamic(ctx, 20, 2);

            if (nk_button_label(ctx, "Open"))
            {
                debugging = TRUE;
                stepping = FALSE;

                OPENFILENAME openFileName;
                openFileName.lStructSize = sizeof(OPENFILENAME);
                openFileName.hInstance = NULL;
                openFileName.lpstrFilter = "Nes files (*.nes)\0*.nes\0Nes save files (*.nsave)\0*.nsave\0\0";
                openFileName.lpstrCustomFilter = NULL;
                openFileName.nMaxCustFilter = 0;
                openFileName.nFilterIndex = 1;
                openFileName.lpstrFile = (char*)Allocate(256);
                openFileName.nMaxFile = 256;
                openFileName.lpstrFileTitle = NULL;
                openFileName.nMaxFileTitle = 0;
                openFileName.lpstrInitialDir = NULL;
                openFileName.lpstrTitle = NULL;
                openFileName.Flags = 0;
                openFileName.nFileOffset = 0;
                openFileName.nFileExtension = 0;
                openFileName.lpstrDefExt = "nes";
                openFileName.lCustData = NULL;
                openFileName.lpfnHook = NULL;
                openFileName.lpTemplateName = NULL;

                if (winHwnd)
                {
                    openFileName.hwndOwner = winHwnd;
                }

                if (GetOpenFileName(&openFileName))
                {
                    char *rom = openFileName.lpstrFile;

                    s32 extlen = 0;
                    char ext[10] = "";

                    s32 len = strlen(rom);
                    for (s32 i = len - 1; i >= 0 && rom[i] != '.'; --i, ++extlen)
                    {
                    }

                    for (s32 i = len - 1, j = extlen - 1; i >= 0 && j >= 0 && rom[i] != '.'; --i, --j)
                    {
                        ext[j] = rom[i];
                    }

                    if (strcmp(ext, "nes") == 0)
                    {
                        Cartridge cartridge = {};
                        if (LoadNesRom(rom, &cartridge))
                        {
                            if (nes)
                            {
                                Destroy(nes);
                            }

                            nes = CreateNES(cartridge);

                            memset(windowTitle, 0, 256);
                            strcpy(windowTitle, "Nes emulator: ");
                            strcat(windowTitle + 13, rom);
                            SDL_SetWindowTitle(win, windowTitle);
                        }
                        else
                        {
                            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "The file couldn't be loaded!", win);
                        }
                    }
                    else if (strcmp(ext, "nsave") == 0)
                    {
                        if (nes)
                        {
                            Destroy(nes);
                        }

                        nes = LoadNesSave(rom);

                        if (nes)
                        {
                            memset(windowTitle, 0, 256);
                            strcpy(windowTitle, "Nes emulator: ");
                            strcat(windowTitle + 13, rom);
                            SDL_SetWindowTitle(win, windowTitle);
                        }
                        else
                        {
                            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "The file couldn't be loaded!", win);
                        }
                    }
                    else
                    {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "The file couldn't be loaded!", win);
                    }

                }
            }

            if (nk_button_label(ctx, "Save"))
            {
                if (nes)
                {
                    debugging = TRUE;
                    stepping = FALSE;

                    OPENFILENAME saveFileName;
                    saveFileName.lStructSize = sizeof(OPENFILENAME);
                    saveFileName.hInstance = NULL;
                    saveFileName.lpstrFilter = "Nes save files (*.nsave)\0*.nsave\0\0";
                    saveFileName.lpstrCustomFilter = NULL;
                    saveFileName.nMaxCustFilter = 0;
                    saveFileName.nFilterIndex = 1;
                    saveFileName.lpstrFile = (char*)Allocate(256);
                    saveFileName.nMaxFile = 256;
                    saveFileName.lpstrFileTitle = NULL;
                    saveFileName.nMaxFileTitle = 0;
                    saveFileName.lpstrInitialDir = NULL;
                    saveFileName.lpstrTitle = NULL;
                    saveFileName.Flags = 0;
                    saveFileName.nFileOffset = 0;
                    saveFileName.nFileExtension = 0;
                    saveFileName.lpstrDefExt = "nsave";
                    saveFileName.lCustData = NULL;
                    saveFileName.lpfnHook = NULL;
                    saveFileName.lpTemplateName = NULL;

                    if (winHwnd)
                    {
                        saveFileName.hwndOwner = winHwnd;
                    }

                    if (GetSaveFileName(&saveFileName))
                    {
                        char *file = saveFileName.lpstrFile;
                        Save(nes, file);
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
            if (nk_begin(ctx, "CPU INFO", nk_rect(310, 10, 250, 170), flags) && nes)
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
            if (nk_begin(ctx, "CONTROLLER 0", nk_rect(310, 190, 250, 120), flags) && nes)
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
                nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);    // Up
                nk_checkbox_label(ctx, "", &buttons[1]);

                nk_layout_row_static(ctx, 20, 20, 9);
                nk_checkbox_label(ctx, "", &buttons[0]);    // Left
                nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
                nk_checkbox_label(ctx, "", &buttons[2]);    // Right
                nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
                nk_checkbox_label(ctx, "", &buttons[4]);    // Select
                nk_checkbox_label(ctx, "", &buttons[5]);    // Start
                nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
                nk_checkbox_label(ctx, "", &buttons[6]);    // B
                nk_checkbox_label(ctx, "", &buttons[7]);    // A

                nk_layout_row_static(ctx, 20, 20, 2);
                nk_label(ctx, "", NK_TEXT_ALIGN_MIDDLE);
                nk_checkbox_label(ctx, "", &buttons[3]);

                // for (s32 i = 0; i < 8; ++i) {
                //     coarseButtons[i] = buttons[i];
                // }
            }
            nk_end(ctx);
        }

        if (debugMode)
        {
            if (nk_begin(ctx, "PPU INFO", nk_rect(570, 10, 310, 300), flags) && nes)
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

        struct nk_rect screenRect = debugMode 
            ? nk_rect(890, 10, 300, 300)
            : nk_rect(310, 10, 880, 780);

        if (nk_begin(ctx, "SCREEN", screenRect, flags))
        {
            ctx->current->bounds = screenRect;

            if (nes)
            {
                CPU *cpu = &nes->cpu;
                PPU *ppu = &nes->ppu;
                GUI *gui = &nes->gui;

                s32 width = 256, height = 240;

                if (!debugMode)
                {
                    width *= 3;
                    height *= 3;
                }

                nk_layout_row_static(ctx, height, width, 1);

                struct nk_command_buffer *canvas;
                struct nk_input *input = &ctx->input;
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

                    glBindTexture(GL_TEXTURE_2D, device.screen.handle.id);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGBA, GL_UNSIGNED_BYTE, gui->pixels);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    nk_draw_image(canvas, space, &device.screen, nk_rgb(255, 255, 255));
                }
            }
        }
        nk_end(ctx);

        if (debugMode)
        {
            if (nk_begin(ctx, "INSTRUCTIONS", nk_rect(10, 210, 290, 580), flags) && nes)
            {
                CPU *cpu = &nes->cpu;
                PPU *ppu = &nes->ppu;

                // REVISAR TAMBIEN ESTA SECCION DEL CODIGO PARA CUANDO SE PONGA UNA DIRECCION EN LA SECCION DE INSTRUCCTIONS
                // SE VAYA A LA INSTRUCCION MAS CERCANA, Y NO COJA LA DIRECCION LITERAL, YA QUE PUEDE QUE EN ESA DIRECCION NO
                // HAYA NINGUNA INSTRUCCION O SEA UN PARAMETRO

                local const float ratio[] = { 100, 100 };
                local char text[12], text2[5];
                local s32 len, len2;

                nk_layout_row(ctx, NK_STATIC, 25, 3, ratio);
                nk_label(ctx, "  Address: ", NK_TEXT_LEFT);
                nk_edit_string(ctx, NK_EDIT_SIMPLE, text, &len, 12, nk_filter_hex);

                nk_layout_row(ctx, NK_STATIC, 25, 3, ratio);
                nk_label(ctx, "  Breakpoint: ", NK_TEXT_LEFT);
                nk_edit_string(ctx, NK_EDIT_SIMPLE, text2, &len2, 5, nk_filter_hex);

                nk_layout_row_dynamic(ctx, 20, 1);

                u16 pc = cpu->pc;

                if (len > 0)
                {
                    text[len] = 0;
                    pc = (u16)strtol(text, NULL, 16);
                    if (pc < 0x8000 || pc > 0xFFFF)
                    {
                        pc = cpu->pc;
                    }
                }

                if (len2 > 0)
                {
                    text2[len2] = 0;
                    breakpoint = (u16)strtol(text2, NULL, 16);
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
                        {
                            col += sprintf(debugBuffer + col, " %02X", ReadCPUU8(nes, pc + 1));
                            break;
                        }
                        case 3:
                        {
                            col += sprintf(debugBuffer + col, " %02X %02X", ReadCPUU8(nes, pc + 1), ReadCPUU8(nes, pc + 2));
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }

                    memset(debugBuffer + col, ' ', 18 - col);
                    col = 18;

                    col += sprintf(debugBuffer + col, "%s", GetInstructionStr(instruction->instruction));


                    switch (instruction->addressingMode)
                    {
                        // Immediate #$00
                        case AM_IMM:
                        {
                            u8 data = ReadCPUU8(nes, pc + 1);
                            col += sprintf(debugBuffer + col, " #$%02X", data);
                            break;
                        }

                        // Absolute $0000
                        case AM_ABS:
                        {
                            u8 lo = ReadCPUU8(nes, pc + 1);
                            u8 hi = ReadCPUU8(nes, pc + 2);
                            u16 address = (hi << 8) | lo;

                            col += sprintf(debugBuffer + col, " $%04X", address);
                            break;
                        }

                        // Absolute Indexed $0000, X
                        case AM_ABX:
                        {
                            u8 lo = ReadCPUU8(nes, pc + 1);
                            u8 hi = ReadCPUU8(nes, pc + 2);
                            u16 address = (hi << 8) | lo;

                            col += sprintf(debugBuffer + col, " $%04X, X", address);
                            break;
                        }

                        // Absolute Indexed $0000, Y
                        case AM_ABY:
                        {
                            u8 lo = ReadCPUU8(nes, pc + 1);
                            u8 hi = ReadCPUU8(nes, pc + 2);
                            u16 address = (hi << 8) | lo;

                            col += sprintf(debugBuffer + col, " $%04X, Y", address);
                            break;
                        }

                        // Zero-Page-Absolute $00
                        case AM_ZPA:
                        {
                            u8 address = ReadCPUU8(nes, pc + 1);
                            col += sprintf(debugBuffer + col, " $%02X", address);
                            break;
                        }

                        // Zero-Page-Indexed $00, X
                        case AM_ZPX:
                        {
                            u8 address = ReadCPUU8(nes, pc + 1);
                            col += sprintf(debugBuffer + col, " $%02X, X", address);
                            break;
                        }

                        // Zero-Page-Indexed $00, Y
                        case AM_ZPY:
                        {
                            u8 address = ReadCPUU8(nes, pc + 1);
                            col += sprintf(debugBuffer + col, " $%02X, Y", address);
                            break;
                        }

                        // Indirect ($0000)
                        case AM_IND:
                        {
                            u8 lo = ReadCPUU8(nes, pc + 1);
                            u8 hi = ReadCPUU8(nes, pc + 2);
                            u16 address = (hi << 8) | lo;

                            col += sprintf(debugBuffer + col, " ($%04X)", address);
                            break;
                        }

                        // Pre-Indexed-Indirect ($00, X)
                        case AM_IZX:
                        {
                            u8 address = ReadCPUU8(nes, pc + 1);
                            col += sprintf(debugBuffer + col, " ($%02X, X)", address);
                            break;
                        }

                        // Post-Indexed-Indirect ($00), Y
                        case AM_IZY:
                        {
                            u8 address = ReadCPUU8(nes, pc + 1);
                            col += sprintf(debugBuffer + col, " ($%02X), Y", address);
                            break;
                        }

                        // Implied
                        case AM_IMP:
                            break;

                            // Accumulator
                        case AM_ACC:
                            break;

                            // Relative $0000
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

        if (debugMode)
        {
            if (nk_begin(ctx, "MEMORY", nk_rect(310, 320, 425, 470), flags) && nes)
            {
                CPU *cpu = &nes->cpu;
                PPU *ppu = &nes->ppu;

                enum options { CPU_MEM, PPU_MEM, OAM_MEM, OAM2_MEM };
                local s32 option = CPU_MEM;

                local const float ratio[] = { 80, 80, 80, 80 };
                local char text[12];
                local s32 len;

                nk_layout_row(ctx, NK_STATIC, 25, 4, ratio);
                option = nk_option_label(ctx, "CPU", option == CPU_MEM) ? CPU_MEM : option;
                option = nk_option_label(ctx, "PPU", option == PPU_MEM) ? PPU_MEM : option;
                option = nk_option_label(ctx, "OAM", option == OAM_MEM) ? OAM_MEM : option;
                option = nk_option_label(ctx, "OAM2", option == OAM2_MEM) ? OAM2_MEM : option;

                u16 address = 0x0000;

                nk_label(ctx, "Address: ", NK_TEXT_LEFT);
                nk_edit_string(ctx, NK_EDIT_SIMPLE, text, &len, 12, nk_filter_hex);

                if (len > 0)
                {
                    text[len] = 0;
                    address = (u16)strtol(text, NULL, 16);
                    if (address < 0x0000 || address > 0xFFFF)
                    {
                        address = 0x0000;
                    }
                }

                nk_layout_row_dynamic(ctx, 20, 1);

                if (option == OAM_MEM)
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
                else if (option == OAM2_MEM)
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
                            u8 v = (option == CPU_MEM)
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
            if (nk_begin(ctx, "VIDEO", nk_rect(745, 320, 445, 470), flags) && nes)
            {
                CPU *cpu = &nes->cpu;
                PPU *ppu = &nes->ppu;
                GUI *gui = &nes->gui;

                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                const struct nk_input *input = &ctx->input;

                struct nk_rect space;
                enum nk_widget_layout_states state;

                enum options { PATTERNS_PALETTES_OAM, NAMETABLES };
                static s32 option = PATTERNS_PALETTES_OAM;

                local const float ratio[] = { 200, 200 };

                nk_layout_row(ctx, NK_STATIC, 25, 2, ratio);
                option = nk_option_label(ctx, "PATTERNS, PALETTES, OAM", option == PATTERNS_PALETTES_OAM) ? PATTERNS_PALETTES_OAM : option;
                option = nk_option_label(ctx, "NAMETABLES", option == NAMETABLES) ? NAMETABLES : option;

                if (option == PATTERNS_PALETTES_OAM)
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
                                    if (nk_tooltip_begin(ctx, 200, NK_TOOLTIP_POS_BOTTOM, 0))
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
                                        if (nk_tooltip_begin(ctx, 200, NK_TOOLTIP_POS_UP, 150))
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
                                    if (nk_tooltip_begin(ctx, 200, NK_TOOLTIP_POS_UP, 150))
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
                                    if (nk_tooltip_begin(ctx, 200, NK_TOOLTIP_POS_UP, 150))
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
                else if (option == NAMETABLES)
                {
                    enum options { H2000, H2400, H2800, H2C00 };
                    local s32 option = H2000;

                    local const float ratio[] = { 80, 80, 80, 80, 80 };

                    nk_layout_row(ctx, NK_STATIC, 25, 5, ratio);
                    option = nk_option_label(ctx, "$2000", option == H2000) ? H2000 : option;
                    option = nk_option_label(ctx, "$2400", option == H2400) ? H2400 : option;
                    option = nk_option_label(ctx, "$2800", option == H2800) ? H2800 : option;
                    option = nk_option_label(ctx, "$2C00", option == H2C00) ? H2C00 : option;

                    local s32 showSepPixels;
                    nk_checkbox_label(ctx, "PIX", &showSepPixels);

                    u16 address = 0x2000 + option * 0x400;

                    if (showSepPixels)
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
            if (nk_begin(ctx, "AUDIO", nk_rect(445, 220, 650, 430), flags | NK_WINDOW_SCALABLE) && nes)
            {
                CPU *cpu = &nes->cpu;
                APU *apu = &nes->apu;

                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                const struct nk_input *input = &ctx->input;

                struct nk_rect space;
                enum nk_widget_layout_states state;

                enum options { GENERAL, SQUARE1, SQUARE2, BUFFER };
                static s32 option = GENERAL;

                local const float ratio[] = { 100, 100, 100, 100 };

                nk_layout_row(ctx, NK_STATIC, 25, 4, ratio);
                option = nk_option_label(ctx, "GENERAL", option == GENERAL) ? GENERAL : option;
                option = nk_option_label(ctx, "SQUARE 1", option == SQUARE1) ? SQUARE1 : option;
                option = nk_option_label(ctx, "SQUARE 2", option == SQUARE2) ? SQUARE2 : option;
                option = nk_option_label(ctx, "BUFFER", option == BUFFER) ? BUFFER : option;

                if (option == GENERAL)
                {
                    local s32 square1Enabled = TRUE;
                    local s32 square2Enabled = TRUE;

                    nk_layout_row_dynamic(ctx, 25, 3);

                    nk_label(ctx, DebugText("CYCLES:%lld", apu->cycles), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("FRAME MODE:%02X", apu->frameMode), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SAMPLE RATE:%04X", apu->sampleRate), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("FRAME IRQ:%02X", apu->frameIRQ), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("FRAME VALUE:%02X", apu->frameValue), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SAMPLE COUNTER:%02X", apu->sampleCounter), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("DMC IRQ:%02X", apu->dmcIRQ), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("FRAME COUNTER:%02X", apu->frameCounter), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("BUFFER INDEX:%04X", apu->bufferIndex), NK_TEXT_LEFT);

                    nk_checkbox_label(ctx, "SQUARE1 ENABLED", &square1Enabled);
                    nk_checkbox_label(ctx, "SQUARE2 ENABLED", &square2Enabled);

                    apu->pulse1.globalEnabled = square1Enabled;
                    apu->pulse2.globalEnabled = square2Enabled;
                }
                else if (option == SQUARE1 || option == SQUARE2)
                {
                    APU::Pulse *pulse = option == SQUARE1 ? &apu->pulse1 : &apu->pulse2;

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

                    local f32 rectHeight = 200.0f;
                    local nk_color lineColor = nk_rgb(255, 0, 0);
                    local f32 lineThickness = 1.0f;

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
                            f32 y = rectHeight - apu->buffer[i] * rectHeight / (APU_AMPLIFIER_VALUE / 2);
                            *(points + i) = nk_vec2(space.x + x, space.y + y);
                        }

                        nk_stroke_rect(canvas, space, 0, 2, nk_rgb(0x41, 0x41, 0x41));
                        nk_stroke_polyline(canvas, (f32*)points, pointCount, lineThickness, lineColor);

                        Free(points);
                    }
                }
                else if (option == BUFFER)
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

        e = Win32GetWallClock();
        d2 = Win32GetSecondsElapsed(s, e);

        LARGE_INTEGER endCounter = Win32GetWallClock();
        f32 secondsElapsed = Win32GetSecondsElapsed(startCounter, endCounter);

        while (secondsElapsed < 0.0167)
        {
            endCounter = Win32GetWallClock();
            secondsElapsed = Win32GetSecondsElapsed(startCounter, endCounter);
        }

        dt = Win32GetSecondsElapsed(startCounter, endCounter);
        startCounter = endCounter;
    }

    // DEBUG: cleanup
    shl_wave_flush ( &wave_file, SHL_TRUE );

    if (dev)
    {
        SDL_CloseAudioDevice(dev);
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
