#ifdef SDL_OPENGL2

#include "types.h"
#include "cartridge.h"
#include "nes.h"
#include "cpu.h"
#include "ppu.h"

#include <SDL2/SDL.h>
#undef main

#include "glew.h"
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_syswm.h>

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

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

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

global NES *nes;


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

internal inline char* DebugText(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    memset(debugBuffer, 0, sizeof(debugBuffer));
    vsprintf(debugBuffer, fmt, ap);
    return debugBuffer;
}

struct device {
    struct nk_buffer cmds;
    struct nk_draw_null_texture null;
    GLuint vbo, vao, ebo;
    GLuint prog;
    GLuint vert_shdr;
    GLuint frag_shdr;
    GLint attrib_pos;
    GLint attrib_uv;
    GLint attrib_col;
    GLint uniform_tex;
    GLint uniform_proj;
    GLuint screen_tex;
};

internal void init_device(struct device *dev)
{
    GLint status;

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
        "}\n";

    nk_buffer_init_default(&dev->cmds);
    dev->prog = glCreateProgram();
    dev->vert_shdr = glCreateShader(GL_VERTEX_SHADER);
    dev->frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(dev->vert_shdr, 1, &vertex_shader, 0);
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
    assert(status == GL_TRUE);

    dev->uniform_tex = glGetUniformLocation(dev->prog, "Texture");
    dev->uniform_proj = glGetUniformLocation(dev->prog, "ProjMtx");
    dev->attrib_pos = glGetAttribLocation(dev->prog, "Position");
    dev->attrib_uv = glGetAttribLocation(dev->prog, "TexCoord");
    dev->attrib_col = glGetAttribLocation(dev->prog, "Color");

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

        glEnableVertexAttribArray((GLuint)dev->attrib_pos);
        glEnableVertexAttribArray((GLuint)dev->attrib_uv);
        glEnableVertexAttribArray((GLuint)dev->attrib_col);

        glVertexAttribPointer((GLuint)dev->attrib_pos, 2, GL_FLOAT, GL_FALSE, vs, (void*)vp);
        glVertexAttribPointer((GLuint)dev->attrib_uv, 2, GL_FLOAT, GL_FALSE, vs, (void*)vt);
        glVertexAttribPointer((GLuint)dev->attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (void*)vc);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

internal void init_textures(struct device *dev)
{
    glGenTextures(1, &dev->screen_tex);
    glBindTexture(GL_TEXTURE_2D, dev->screen_tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
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
    HWND winHwnd;
    struct nk_color background;
    int win_width, win_height;
    b32 running = TRUE, quit = FALSE;
    f32 dt = 0;

    LARGE_INTEGER initialCounter;
    LARGE_INTEGER perfCountFrequencyResult;

    /* GUI */
    struct nk_context *ctx;
    struct device device;

    initialCounter = Win32GetWallClock();

    QueryPerformanceFrequency(&perfCountFrequencyResult);
    globalPerfCountFrequency = perfCountFrequencyResult.QuadPart;

    char windowTitle[256] = "Nes emulator";

    /* SDL setup */
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_Init(SDL_INIT_VIDEO);
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

    /* GUI */
    ctx = nk_sdl_init(win);
    init_device(&device);
    {
        struct nk_font_atlas *atlas;
        nk_sdl_font_stash_begin(&atlas);
        struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../fonts/consola.ttf", 14, 0);
        nk_sdl_font_stash_end();
        /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
        nk_style_set_font(ctx, &future->handle);
    }

    glEnable(GL_TEXTURE_2D);
    init_textures(&device);

    background = nk_rgb(0, 0, 0);

    LARGE_INTEGER startCounter = Win32GetWallClock();
    dt = Win32GetSecondsElapsed(initialCounter, startCounter);
    running = true;

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
            }
            nk_sdl_handle_event(&evt);
        }
        nk_input_end(ctx);

        if (quit)
        {
            break;
        }

        if (nes)
        {
            CPU *cpu = &nes->cpu;
            PPU *ppu = &nes->ppu;

            // 
            // ADD THE BINDINGS FOR SPACE, A AND S AGAIN
            //

            SetButton(nes, 0, BUTTON_UP, ctx->input.keyboard.keys[NK_KEY_UP].down || coarseButtons[1]);
            SetButton(nes, 0, BUTTON_DOWN, ctx->input.keyboard.keys[NK_KEY_DOWN].down || coarseButtons[3]);
            SetButton(nes, 0, BUTTON_LEFT, ctx->input.keyboard.keys[NK_KEY_LEFT].down || coarseButtons[0]);
            SetButton(nes, 0, BUTTON_RIGHT, ctx->input.keyboard.keys[NK_KEY_RIGHT].down || coarseButtons[2]);
            SetButton(nes, 0, BUTTON_SELECT, ctx->input.keyboard.keys[NK_KEY_SPACE].down || coarseButtons[4]);
            SetButton(nes, 0, BUTTON_START, ctx->input.keyboard.keys[NK_KEY_ENTER].down || coarseButtons[5]);
            SetButton(nes, 0, BUTTON_B, ctx->input.keyboard.keys[NK_KEY_S].down || coarseButtons[6]);
            SetButton(nes, 0, BUTTON_A, ctx->input.keyboard.keys[NK_KEY_A].down || coarseButtons[7]);

            // Frame
            // I'm targeting 60fps, so run the amount of cpu cycles for 0.0167 seconds
            //
            // This could be calculated with the frequency of the PPU, and run based on 
            // the amount of cycles that we want the PPU to run in 0.0167 seconds. For now
            // i'm using the CPU frequency.
            s32 cycles = 0.0167 * CPU_FREQ;

            // If we are stepping in the debugger, run only 1 cycle
            if (stepping || oneCycleAtTime)
            {
                cycles = 1;
            }

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

                for (s32 i = 0; i < 3 * step.cycles; i++)
                {
                    StepPPU(nes);
                }

                /*for (s32 i = 0; i < step.cycles; i++)
                {
                StepAPU(nes);
                }*/

                cycles -= step.cycles;

                if (debugging)
                {
                    stepping = FALSE;
                }
            }
        }

        /* GUI */
        nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE;

        if (nk_begin(ctx, "FPS INFO", nk_rect(10, 10, 290, 170), flags))
        {
            local s32 fps = 0;
            local s32 fpsCount = 0;
            local s32 oneCycle = 0;

            nk_layout_row_dynamic(ctx, 20, 2);
            nk_label(ctx, DebugText("dt: %.4f", dt), NK_TEXT_LEFT);

            nk_checkbox_label(ctx, "1 cycle", &oneCycle);
            oneCycleAtTime = oneCycle;

            LARGE_INTEGER fpsCounter = Win32GetWallClock();
            f32 fpsdt = Win32GetSecondsElapsed(initialCounter, fpsCounter);
            if (fpsdt > 1)
            {
                initialCounter = fpsCounter;
                fps = fpsCount;
                fpsCount = 0;
            }

            nk_label(ctx, DebugText("fps: %d", fps), NK_TEXT_LEFT);
            ++fpsCount;

            nk_layout_row_dynamic(ctx, 20, 2);
            if (nk_button_label(ctx, "Run"))
            {
                hitRun = TRUE;
                debugging = FALSE;
                stepping = FALSE;
            }

            if (nk_button_label(ctx, "Reset"))
            {
                ResetNES(nes);

                debugging = TRUE;
            }

            if (nk_button_label(ctx, "Pause"))
            {
                debugging = TRUE;
                stepping = FALSE;
            }

            if (nk_button_label(ctx, "Step"))
            {
                debugging = TRUE;
                stepping = TRUE;
            }

            if (nk_button_label(ctx, "Open"))
            {
                debugging = TRUE;
                stepping = FALSE;

                OPENFILENAME openFileName;
                openFileName.lStructSize = sizeof(OPENFILENAME);
                openFileName.hInstance = NULL;
                openFileName.lpstrFilter = "Nes files (*.nes)\0*.nes\0\0";
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
            }
        }
        nk_end(ctx);

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

        if (nk_begin(ctx, "CONTROLLER 0", nk_rect(310, 190, 250, 120), flags) && nes)
        {
            local s32 buttons[8];

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

            for (s32 i = 0; i < 8; ++i) {
                coarseButtons[i] = buttons[i];
            }
        }
        nk_end(ctx);

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

        if (nk_begin(ctx, "SCREEN", nk_rect(890, 10, 300, 300), flags) && nes)
        {
            GUI *gui = &nes->gui;

            // and upload the texture data every time
            glBindTexture(GL_TEXTURE_2D, device.screen_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 240, 0, GL_RGBA, GL_UNSIGNED_BYTE, gui->pixels);
            glGenerateMipmap(GL_TEXTURE_2D);

            nk_layout_row_static(ctx, 240, 256, 1);

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

                struct nk_image image = nk_image_id((int)device.screen_tex);
                nk_draw_image(canvas, space, &image, nk_rgb(255, 255, 255));
            }
        }
        nk_end(ctx);

        if (nk_begin(ctx, "INSTRUCTIONS", nk_rect(10, 190, 290, 600), flags) && nes)
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
                        col += sprintf(debugBuffer + col, " %02X", ReadU8(&nes->cpuMemory, pc + 1));
                        break;
                    }
                    case 3:
                    {
                        col += sprintf(debugBuffer + col, " %02X %02X", ReadU8(&nes->cpuMemory, pc + 1), ReadU8(&nes->cpuMemory, pc + 2));
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
                        u8 data = ReadU8(&nes->cpuMemory, pc + 1);
                        col += sprintf(debugBuffer + col, " #$%02X", data);
                        break;
                    }

                    // Absolute $0000
                    case AM_ABS:
                    {
                        u8 lo = ReadU8(&nes->cpuMemory, pc + 1);
                        u8 hi = ReadU8(&nes->cpuMemory, pc + 2);
                        u16 address = (hi << 8) | lo;

                        col += sprintf(debugBuffer + col, " $%04X", address);
                        break;
                    }

                    // Absolute Indexed $0000, X
                    case AM_ABX:
                    {
                        u8 lo = ReadU8(&nes->cpuMemory, pc + 1);
                        u8 hi = ReadU8(&nes->cpuMemory, pc + 2);
                        u16 address = (hi << 8) | lo;

                        col += sprintf(debugBuffer + col, " $%04X, X", address);
                        break;
                    }

                    // Absolute Indexed $0000, Y
                    case AM_ABY:
                    {
                        u8 lo = ReadU8(&nes->cpuMemory, pc + 1);
                        u8 hi = ReadU8(&nes->cpuMemory, pc + 2);
                        u16 address = (hi << 8) | lo;

                        col += sprintf(debugBuffer + col, " $%04X, Y", address);
                        break;
                    }

                    // Zero-Page-Absolute $00
                    case AM_ZPA:
                    {
                        u8 address = ReadU8(&nes->cpuMemory, pc + 1);
                        col += sprintf(debugBuffer + col, " $%02X", address);
                        break;
                    }

                    // Zero-Page-Indexed $00, X
                    case AM_ZPX:
                    {
                        u8 address = ReadU8(&nes->cpuMemory, pc + 1);
                        col += sprintf(debugBuffer + col, " $%02X, X", address);
                        break;
                    }

                    // Zero-Page-Indexed $00, Y
                    case AM_ZPY:
                    {
                        u8 address = ReadU8(&nes->cpuMemory, pc + 1);
                        col += sprintf(debugBuffer + col, " $%02X, Y", address);
                        break;
                    }

                    // Indirect ($0000)
                    case AM_IND:
                    {
                        u8 lo = ReadU8(&nes->cpuMemory, pc + 1);
                        u8 hi = ReadU8(&nes->cpuMemory, pc + 2);
                        u16 address = (hi << 8) | lo;

                        col += sprintf(debugBuffer + col, " ($%04X)", address);
                        break;
                    }

                    // Pre-Indexed-Indirect ($00, X)
                    case AM_IZX:
                    {
                        u8 address = ReadU8(&nes->cpuMemory, pc + 1);
                        col += sprintf(debugBuffer + col, " ($%02X, X)", address);
                        break;
                    }

                    // Post-Indexed-Indirect ($00), Y
                    case AM_IZY:
                    {
                        u8 address = ReadU8(&nes->cpuMemory, pc + 1);
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
                        s8 address = (s8)ReadU8(&nes->cpuMemory, pc + 1);
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
                            ? ReadU8(&nes->cpuMemory, address + i * 16 + j)
                            : ReadU8(&nes->ppuMemory, address + i * 16 + j);

                        col += sprintf(debugBuffer + col, " %02X", v);
                    }

                    nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
                }
            }
        }
        nk_end(ctx);

        if (nk_begin(ctx, "VIDEO", nk_rect(745, 320, 445, 470), flags) && nes)
        {
            CPU *cpu = &nes->cpu;
            PPU *ppu = &nes->ppu;

            enum options { PATTERNS_PALETTES_OAM, NAMETABLES };
            static s32 option = PATTERNS_PALETTES_OAM;

            local const float ratio[] = { 200, 200 };

            nk_layout_row(ctx, NK_STATIC, 25, 2, ratio);
            option = nk_option_label(ctx, "PATTERNS, PALETTES, OAM", option == PATTERNS_PALETTES_OAM) ? PATTERNS_PALETTES_OAM : option;
            option = nk_option_label(ctx, "NAMETABLES", option == NAMETABLES) ? NAMETABLES : option;

            if (option == PATTERNS_PALETTES_OAM)
            {
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "PATTERNS", NK_TEXT_LEFT);

                local u32 patterns[2][128 * 128];

                nk_layout_row_static(ctx, 128, 128, 2);

                struct nk_command_buffer *canvas;
                struct nk_input *input = &ctx->input;
                canvas = nk_window_get_canvas(ctx);

                struct nk_rect space;
                enum nk_widget_layout_states state;

                for (s32 patternTable = 0; patternTable < 2; ++patternTable)
                {
                    u16 patternTableAddress = patternTable * 0x1000;

                    for (s32 tileY = 0; tileY < 16; ++tileY)
                    {
                        for (s32 tileX = 0; tileX < 16; ++tileX)
                        {
                            u16 patternIndex = tileY * 16 + tileX;

                            for (s32 y = 0; y < 8; ++y)
                            {
                                u8 row1 = ReadPPUU8(nes, patternTableAddress + patternIndex * 16 + y);
                                u8 row2 = ReadPPUU8(nes, patternTableAddress + patternIndex * 16 + 8 + y);

                                for (s32 x = 0; x < 8; ++x)
                                {
                                    u8 h = ((row2 >> (7 - x)) & 0x1);
                                    u8 l = ((row1 >> (7 - x)) & 0x1);
                                    u8 v = (h << 0x1) | l;

                                    s32 pixelX = (tileX * 8 + x);
                                    s32 pixelY = (tileY * 8 + y);
                                    s32 pixel = pixelY * 128 + pixelX;

                                    u32 paletteIndex = v;
                                    u32 colorIndex = ReadPPUU8(nes, 0x3F00 + paletteIndex);
                                    Color color = systemPalette[colorIndex % 64];

                                    patterns[patternTable][pixel] = color.bgra;
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

                        struct nk_image image;
                        image.w = 128;
                        image.h = 128;
                        image.handle.ptr = patterns[patternTable];
                        image.region[0] = space.x;
                        image.region[1] = space.y;
                        image.region[2] = space.w;
                        image.region[3] = space.h;

                        nk_draw_image(canvas, space, &image, nk_rgb(255, 0, 0));
                    }
                }

                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "PALETTES", NK_TEXT_LEFT);

                nk_layout_row_static(ctx, 20, 20, 16);

                for (s32 paletteIndex = 0; paletteIndex < 2; ++paletteIndex)
                {
                    for (s32 i = 0; i < 16; ++i)
                    {
                        u8 colorIndex = ReadPPUU8(nes, 0x3F00 + (paletteIndex * 0x10) + i);
                        Color color = systemPalette[colorIndex % 64];

                        state = nk_widget(&space, ctx);
                        if (state)
                        {
                            if (state != NK_WIDGET_ROM)
                            {
                                // update_your_widget_by_user_input(...);
                            }

                            nk_fill_rect(canvas, nk_rect(space.x, space.y, space.w, space.h), 0, nk_rgb(color.r, color.g, color.b));
                        }
                    }
                }

                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "OAM", NK_TEXT_LEFT);

                local u32 sprites[64][8 * 8];

                nk_layout_row_static(ctx, 8, 8, 16);

                u16 spriteBaseAddress = 0x1000 * GetBitFlag(ppu->control, SPRITE_ADDR_FLAG);

                for (s32 index = 0; index < 64; ++index)
                {
                    u8 spriteY = ReadU8(&nes->oamMemory, index * 4 + 0);
                    u8 spriteIdx = ReadU8(&nes->oamMemory, index * 4 + 1);
                    u8 spriteAttr = ReadU8(&nes->oamMemory, index * 4 + 2);
                    u8 spriteX = ReadU8(&nes->oamMemory, index * 4 + 3);

                    u8 highColorBits = spriteAttr & 0x03;

                    for (s32 y = 0; y < 8; ++y)
                    {
                        u8 rowOffset = y;

                        // the bit 7 indicate that the sprite should flip vertically
                        if (spriteAttr & 0x80)
                        {
                            // @TODO: this doesn't care about 8x16 sprites
                            rowOffset = 7 - rowOffset;
                        }

                        u8 row1 = ReadPPUU8(nes, spriteBaseAddress + spriteIdx * 16 + rowOffset);
                        u8 row2 = ReadPPUU8(nes, spriteBaseAddress + spriteIdx * 16 + 8 + rowOffset);

                        for (s32 x = 0; x < 8; ++x)
                        {
                            u8 colOffset = x;

                            // the bit 6 indicate that the sprite should flip horizontally
                            if (spriteAttr & 0x40)
                            {
                                // @TODO: this doesn't care about 8x16 sprites
                                colOffset = 7 - colOffset;
                            }

                            u8 h = ((row2 >> (7 - colOffset)) & 0x1);
                            u8 l = ((row1 >> (7 - colOffset)) & 0x1);
                            u8 v = (h << 0x1) | l;

                            u32 paletteIndex = (highColorBits << 2) | v;
                            u32 colorIndex = ReadPPUU8(nes, 0x3F00 + paletteIndex);
                            Color color = systemPalette[colorIndex % 64];

                            s32 pixelIndex = y * 8 + x;
                            sprites[index][pixelIndex] = color.bgra;
                        }
                    }

                    state = nk_widget(&space, ctx);
                    if (state)
                    {
                        if (state != NK_WIDGET_ROM)
                        {
                            // update_your_widget_by_user_input(...);
                        }

                        struct nk_image image;
                        image.w = 8;
                        image.h = 8;
                        image.handle.ptr = sprites[index];
                        image.region[0] = space.x;
                        image.region[1] = space.y;
                        image.region[2] = space.w;
                        image.region[3] = space.h;

                        nk_draw_image(canvas, space, &image, nk_rgb(255, 0, 0));
                    }
                }

                local u32 sprites2[8][8 * 8];

                nk_layout_row_static(ctx, 8, 8, 8);

                for (s32 index = 0; index < 8; ++index)
                {
                    u8 spriteY = ReadU8(&nes->oamMemory2, index * 4 + 0);
                    u8 spriteIdx = ReadU8(&nes->oamMemory2, index * 4 + 1);
                    u8 spriteAttr = ReadU8(&nes->oamMemory2, index * 4 + 2);
                    u8 spriteX = ReadU8(&nes->oamMemory2, index * 4 + 3);

                    u8 highColorBits = spriteAttr & 0x03;

                    for (s32 y = 0; y < 8; ++y)
                    {
                        u8 rowOffset = y;

                        // the bit 7 indicate that the sprite should flip vertically
                        if (spriteAttr & 0x80)
                        {
                            // @TODO: this doesn't care about 8x16 sprites
                            rowOffset = 7 - rowOffset;
                        }

                        u8 row1 = ReadPPUU8(nes, spriteBaseAddress + spriteIdx * 16 + y);
                        u8 row2 = ReadPPUU8(nes, spriteBaseAddress + spriteIdx * 16 + 8 + y);

                        for (s32 x = 0; x < 8; ++x)
                        {
                            u8 colOffset = x;

                            // the bit 6 indicate that the sprite should flip horizontally
                            if (spriteAttr & 0x40)
                            {
                                // @TODO: this doesn't care about 8x16 sprites
                                colOffset = 7 - colOffset;
                            }

                            u8 h = ((row2 >> (7 - colOffset)) & 0x1);
                            u8 l = ((row1 >> (7 - colOffset)) & 0x1);
                            u8 v = (h << 0x1) | l;

                            u32 paletteIndex = (highColorBits << 2) | v;
                            u32 colorIndex = ReadPPUU8(nes, 0x3F10 + paletteIndex);
                            Color color = systemPalette[colorIndex % 64];

                            s32 pixelIndex = y * 8 + x;
                            sprites2[index][pixelIndex] = color.bgra;
                        }
                    }

                    state = nk_widget(&space, ctx);
                    if (state)
                    {
                        if (state != NK_WIDGET_ROM)
                        {
                            // update_your_widget_by_user_input(...);
                        }

                        struct nk_image image;
                        image.w = 8;
                        image.h = 8;
                        image.handle.ptr = sprites2[index];
                        image.region[0] = space.x;
                        image.region[1] = space.y;
                        image.region[2] = space.w;
                        image.region[3] = space.h;

                        nk_draw_image(canvas, space, &image, nk_rgb(255, 0, 0));
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

                struct nk_command_buffer *canvas;
                struct nk_input *input = &ctx->input;
                canvas = nk_window_get_canvas(ctx);

                struct nk_rect space;
                enum nk_widget_layout_states state;

                u16 address = 0x2000 + option * 0x400;

                if (showSepPixels)
                {
                    local u32 nametable[32][30][64];

                    nk_layout_row_static(ctx, 8, 8, 32);

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
                                    ColorEmphasis(&color, colorMask);

                                    s32 pixel = y * 8 + x;
                                    nametable[tileX][tileY][pixel] = color.bgra;
                                }
                            }

                            state = nk_widget(&space, ctx);
                            if (state)
                            {
                                if (state != NK_WIDGET_ROM)
                                {
                                    // update_your_widget_by_user_input(...);
                                }

                                struct nk_image image;
                                image.w = 8;
                                image.h = 8;
                                image.handle.ptr = nametable[tileX][tileY];
                                image.region[0] = space.x;
                                image.region[1] = space.y;
                                image.region[2] = space.w;
                                image.region[3] = space.h;

                                nk_draw_image(canvas, space, &image, nk_rgb(255, 0, 0));
                            }
                        }
                    }
                }
                else
                {
                    local u32 nametable[256 * 240];

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
                                    ColorEmphasis(&color, colorMask);

                                    s32 pixelX = (tileX * 8 + x);
                                    s32 pixelY = (tileY * 8 + y);
                                    s32 pixel = pixelY * 256 + pixelX;
                                    nametable[pixel] = color.bgra;
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

                        struct nk_image image;
                        image.w = 256;
                        image.h = 240;
                        image.handle.ptr = nametable;
                        image.region[0] = space.x;
                        image.region[1] = space.y;
                        image.region[2] = space.w;
                        image.region[3] = space.h;

                        nk_draw_image(canvas, space, &image, nk_rgb(255, 0, 0));
                    }
                }
            }
        }
        nk_end(ctx);

        /* Draw */
        {
            float bg[4];
            nk_color_fv(bg, background);
            SDL_GetWindowSize(win, &win_width, &win_height);
            glViewport(0, 0, win_width, win_height);
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(bg[0], bg[1], bg[2], bg[3]);
            /* IMPORTANT: `nk_sdl_render` modifies some global OpenGL state
            * with blending, scissor, face culling, depth test and viewport and
            * defaults everything back into a default state.
            * Make sure to either a.) save and restore or b.) reset your own state after
            * rendering the UI. */
            nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
            SDL_GL_SwapWindow(win);
        }
    }

    nk_sdl_shutdown();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

#endif // SDL_OPENGL2
