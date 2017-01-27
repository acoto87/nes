#include "types.h"
#include "cartridge.h"
#include "nes.h"
#include "cpu.h"
#include "ppu.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_GDI_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_gdi.h"

global u32 windowWidth = 1200;
global u32 windowHeight = 800;
global s64 globalPerfCountFrequency;
global b32 running;
global b32 needs_refresh = TRUE;
global char debugBuffer[256];
global b32 hitRun;
global b32 debugging = TRUE;
global b32 stepping;
global u16 breakpoint;

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

LRESULT CALLBACK Win32MainWindowCallback(
    HWND   window,
    UINT   msg,
    WPARAM wParam,
    LPARAM lParam)
{
    LRESULT result = 0;

    switch (msg)
    {
        case WM_SIZE:
        {
            windowWidth = LOWORD(lParam);
            windowHeight = HIWORD(lParam);
            break;
        }

        case WM_DESTROY:
        {
            running = false;
            return 0;
        }

        case WM_CLOSE:
        {
            running = false;
            return 0;
        }
    }

    //if (!nk_gdip_handle_event(window, msg, wParam, lParam))
    if (!nk_gdi_handle_event(window, msg, wParam, lParam))
    {
        result = DefWindowProc(window, msg, wParam, lParam);
    }

    return result;
}

int CALLBACK WinMain(
    HINSTANCE instance,
    HINSTANCE prevInstance,
    LPSTR     cmdLine,
    int       cmdShow)
{
    LARGE_INTEGER initialCounter = Win32GetWallClock();

    LARGE_INTEGER perfCountFrequencyResult;
    QueryPerformanceFrequency(&perfCountFrequencyResult);
    globalPerfCountFrequency = perfCountFrequencyResult.QuadPart;

    f32 dt = 0;

    Cartridge cartridge = {};
    if (!LoadNesRom("nestest.nes", &cartridge))
    {
        return 0;
    }

    nes = CreateNES(cartridge);
    if (!nes)
    {
        return 0;
    }

    WNDCLASS windowClass = {};
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = Win32MainWindowCallback;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(0, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    windowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    windowClass.lpszClassName = "NESWindowClass";

    if (RegisterClassA(&windowClass))
    {
        //u32 windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
        u32 windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

        RECT windowRect = { 0, 0, windowWidth, windowHeight };
        AdjustWindowRectEx(
            &windowRect,
            windowStyle,
            NULL,
            NULL);

        HWND window = CreateWindowEx(
            NULL,
            windowClass.lpszClassName,
            "Nes emulator",
            windowStyle,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            NULL,
            NULL,
            instance,
            NULL);

        if (window)
        {
            HDC dc = GetDC(window);

            GdiFont *font = nk_gdifont_create("Consolas", 14);
            nk_context *ctx = nk_gdi_init(font, dc, windowWidth, windowHeight);

            /*nk_context *ctx = nk_gdip_init(window, windowWidth, windowHeight);
            GdipFont *font = nk_gdipfont_create("Consolas", 14);
            nk_gdip_set_font(font);*/

            LARGE_INTEGER startCounter = Win32GetWallClock();

            dt = Win32GetSecondsElapsed(initialCounter, startCounter);
            running = true;

            while (running)
            {
                MSG msg;

                nk_input_begin(ctx);

                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                    {
                        running = FALSE;
                    }

                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                nk_input_end(ctx);

                // Frame

                if (!debugging)
                {
                    if (!hitRun)
                    {
                        if (nes->cpu.pc == breakpoint)
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

                if (!debugging || stepping)
                {
                    //while (cycles > 0)
                    {
                        CPUStep step = StepCPU(nes);

                        for (u32 i = 0; i < 3 * step.cycles; i++)
                        {
                            StepPPU(nes);
                        }

                        for (u32 i = 0; i < step.cycles; i++)
                        {
                            // StepAPU(nes);
                        }

                        //cycles -= step.cycles;
                    }

                    if (debugging)
                    {
                        stepping = FALSE;
                    }
                }

                // GUI
                CPU *cpu = &nes->cpu;
                PPU *ppu = &nes->ppu;

                nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE;

                if (nk_begin(ctx, "FPS INFO", nk_rect(10, 10, 250, 150), flags))
                {
                    nk_layout_row_dynamic(ctx, 20, 1);
                    nk_label(ctx, DebugText("dt: %.4f", dt), NK_TEXT_LEFT);

                    nk_layout_row_dynamic(ctx, 20, 2);
                    if (nk_button_label(ctx, "Run"))
                    {
                        hitRun = TRUE;
                        debugging = FALSE;
                        stepping = FALSE;
                    }

                    if (nk_button_label(ctx, "Reset"))
                    {
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
                }
                nk_end(ctx);

                if (nk_begin(ctx, "CPU INFO", nk_rect(270, 10, 250, 300), flags))
                {
                    nk_layout_row_dynamic(ctx, 20, 1);

                    nk_label(ctx, DebugText("%s:%02X", "A", cpu->a), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%s:%02X", "X", cpu->x), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%s:%02X", "Y", cpu->y), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%s:%02X", "P", cpu->p), NK_TEXT_LEFT);
                    nk_label(ctx, "    N V   B D I Z C", NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("    %d %d %d %d %d %d %d %d",
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

                if (nk_begin(ctx, "PPU INFO", nk_rect(530, 10, 350, 300), flags))
                {
                    nk_layout_row_dynamic(ctx, 20, 2);

                    nk_label(ctx, DebugText("CTRL (0x2000):%02X", ppu->control), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SCANLINE:%3d", ppu->scanline), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("MASK (0x2001):%02X", ppu->mask), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("CYCLE:%3d", ppu->cycle), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("STAT (0x2002):%02X", ppu->status), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("CYCLES:%d", ppu->totalCycles), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SPRA (0x2003):%02X", ppu->oamAddress), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("FRAMES:%d", ppu->frameCount), NK_TEXT_LEFT);

                    nk_layout_row_dynamic(ctx, 20, 1);
                    nk_label(ctx, DebugText("SPRD (0x2004):%02X", ppu->oamData), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("SCRR (0x2005):%02X", ppu->scroll), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("MEMA (0x2006):%02X", ppu->address), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("    %s:%04X  %s:%04X", "v", ppu->v, "t", ppu->t), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("    %s:%04X  %s:%01X", "x", ppu->x, "w", ppu->w), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("MEMD (0x2007):%02X", ppu->data), NK_TEXT_LEFT);
                }
                nk_end(ctx);

                if (nk_begin(ctx, "SCREEN", nk_rect(890, 10, 300, 300), flags))
                {
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

                        GUI *gui = &nes->gui;

                        struct nk_image image;
                        image.w = gui->width;
                        image.h = gui->height;
                        image.handle.ptr = gui->pixels;
                        image.region[0] = space.x;
                        image.region[1] = space.y;
                        image.region[2] = space.w;
                        image.region[3] = space.h;

                        nk_draw_image(canvas, space, &image, nk_rgb(255, 0, 0));
                    }
                }
                nk_end(ctx);

                if (nk_begin(ctx, "INSTRUCTIONS", nk_rect(10, 170, 250, 620), flags))
                {
                    // POR AHORA PARECE QUE ESTA EJECUTANDOSE BIEN HASTA LA INSTRUCCION: '0x85DC'
                    // ESTA ES EL HANDLER DE 'VBLANK', EN LA QUE SE EJECUTAN LAS INSTRUCCIONES DE LOS SPRITES
                    // LUEGO, REVISAR BIEN LA FUNCIONALIDAD DE SPRITES, OAMADDRESS, OAMDATA Y DEMAS.
                    // HASTA ESTE PUNTO TODO COINCIDE CON NINTENDULATOR, DESPUES DE ESCRIBIR EN '4014' la memoria de OAM no 
                    // es igual a la de Nintendulator, chequear esto. Tampoco es igual despues de unas cuantas instrucciones VRAM address.

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

                        if (col < 15)
                        {
                            memset(debugBuffer + col, ' ', 15 - col);
                            col = 15;
                        }

                        col += sprintf(debugBuffer + col, "%10s", GetInstructionStr(instruction->instruction));

                        nk_label(ctx, debugBuffer, NK_TEXT_LEFT);

                        pc += instruction->bytesCount;
                    }
                }
                nk_end(ctx);

                if (nk_begin(ctx, "MEMORY", nk_rect(270, 320, 425, 470), flags))
                {
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

                if (nk_begin(ctx, "VIDEO", nk_rect(705, 320, 485, 470), flags))
                {
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

                                            u32 c = 0xFF000000;
                                            if (v == 1)
                                                c |= 0xFF0000;
                                            else if (v == 2)
                                                c |= 0x666666;
                                            else if (v == 3)
                                                c |= 0xFFFFFF;

                                            patterns[patternTable][pixel] = c;
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

                        for (s32 index = 0; index < 64; ++index)
                        {
                            u8 spriteY = ReadU8(&nes->oamMemory, index * 4 + 0);
                            u8 spriteI = ReadU8(&nes->oamMemory, index * 4 + 1);
                            u8 spriteA = ReadU8(&nes->oamMemory, index * 4 + 2);
                            u8 spriteX = ReadU8(&nes->oamMemory, index * 4 + 3);

                            u8 highColorBits = spriteI & 0x03;

                            for (s32 y = 0; y < 8; ++y)
                            {
                                u8 row1 = ReadPPUU8(nes, spriteI * 16 + y);
                                u8 row2 = ReadPPUU8(nes, spriteI * 16 + 8 + y);

                                for (s32 x = 0; x < 8; ++x)
                                {
                                    s32 pixelIndex = y * 8 + x;

                                    u8 h = ((row2 >> (7 - x)) & 0x1);
                                    u8 l = ((row1 >> (7 - x)) & 0x1);
                                    u8 v = (h << 0x1) | l;

                                    u32 paletteIndex = (highColorBits << 2) | v;
                                    u32 colorIndex = ReadPPUU8(nes, 0x3F00 + paletteIndex);
                                    Color color = systemPalette[colorIndex % 64];

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
                            u8 spriteI = ReadU8(&nes->oamMemory2, index * 4 + 1);
                            u8 spriteA = ReadU8(&nes->oamMemory2, index * 4 + 2);
                            u8 spriteX = ReadU8(&nes->oamMemory2, index * 4 + 3);

                            u8 highColorBits = spriteI & 0x03;

                            for (s32 y = 0; y < 8; ++y)
                            {
                                u8 row1 = ReadPPUU8(nes, spriteI * 16 + y);
                                u8 row2 = ReadPPUU8(nes, spriteI * 16 + 8 + y);

                                for (s32 x = 0; x < 8; ++x)
                                {
                                    s32 pixelIndex = y * 8 + x;

                                    u8 h = ((row2 >> (7 - x)) & 0x1);
                                    u8 l = ((row1 >> (7 - x)) & 0x1);
                                    u8 v = (h << 0x1) | l;

                                    u32 paletteIndex = (highColorBits << 2) | v;
                                    u32 colorIndex = ReadPPUU8(nes, 0x3F00 + paletteIndex);
                                    Color color = systemPalette[colorIndex % 64];

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

                                    u8 highColorBits;

                                    switch (attributeTableLookup[attributeOffsetX][attributeOffsetY])
                                    {
                                        case 0x00:
                                        case 0x01:
                                        case 0x02:
                                        case 0x03:
                                        {
                                            highColorBits = (attributeByte & 0x03);
                                            break;
                                        }

                                        case 0x04:
                                        case 0x05:
                                        case 0x06:
                                        case 0x07:
                                        {
                                            highColorBits = ((attributeByte >> 2) & 0x03);
                                            break;
                                        }

                                        case 0x08:
                                        case 0x09:
                                        case 0x0A:
                                        case 0x0B:
                                        {
                                            highColorBits = ((attributeByte >> 4) & 0x03);
                                            break;
                                        }

                                        case 0x0C:
                                        case 0x0D:
                                        case 0x0E:
                                        case 0x0F:
                                        {
                                            highColorBits = ((attributeByte >> 6) & 0x03);
                                            break;
                                        }
                                    }

                                    for (s32 y = 0; y < 8; ++y)
                                    {
                                        u8 row1 = ReadPPUU8(nes, patternIndex * 16 + y);
                                        u8 row2 = ReadPPUU8(nes, patternIndex * 16 + 8 + y);

                                        for (s32 x = 0; x < 8; ++x)
                                        {
                                            u8 h = ((row2 >> (7 - x)) & 0x1);
                                            u8 l = ((row1 >> (7 - x)) & 0x1);
                                            u8 v = (h << 0x1) | l;

                                            u32 paletteIndex = (highColorBits << 2) | v;
                                            u32 colorIndex = ReadPPUU8(nes, 0x3F00 + paletteIndex);
                                            Color color = systemPalette[colorIndex % 64];
                                            
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

                                    u8 highColorBits;

                                    switch (attributeTableLookup[attributeOffsetX][attributeOffsetY])
                                    {
                                        case 0x00:
                                        case 0x01:
                                        case 0x02:
                                        case 0x03:
                                        {
                                            highColorBits = (attributeByte & 0x03);
                                            break;
                                        }

                                        case 0x04:
                                        case 0x05:
                                        case 0x06:
                                        case 0x07:
                                        {
                                            highColorBits = ((attributeByte >> 2) & 0x03);
                                            break;
                                        }

                                        case 0x08:
                                        case 0x09:
                                        case 0x0A:
                                        case 0x0B:
                                        {
                                            highColorBits = ((attributeByte >> 4) & 0x03);
                                            break;
                                        }

                                        case 0x0C:
                                        case 0x0D:
                                        case 0x0E:
                                        case 0x0F:
                                        {
                                            highColorBits = ((attributeByte >> 6) & 0x03);
                                            break;
                                        }
                                    }

                                    for (s32 y = 0; y < 8; ++y)
                                    {
                                        u8 row1 = ReadPPUU8(nes, patternIndex * 16 + y);
                                        u8 row2 = ReadPPUU8(nes, patternIndex * 16 + 8 + y);

                                        for (s32 x = 0; x < 8; ++x)
                                        {
                                            u8 h = ((row2 >> (7 - x)) & 0x1);
                                            u8 l = ((row1 >> (7 - x)) & 0x1);
                                            u8 v = (h << 0x1) | l;

                                            u32 paletteIndex = (highColorBits << 2) | v;
                                            u32 colorIndex = ReadPPUU8(nes, 0x3F00 + paletteIndex);
                                            Color color = systemPalette[colorIndex % 64];

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

                nk_gdi_render(nk_rgb(0, 0, 0));
                //nk_gdip_render(nk_rgb(0, 0, 0));

                LARGE_INTEGER endCounter = Win32GetWallClock();
                dt = Win32GetSecondsElapsed(startCounter, endCounter);
                startCounter = endCounter;
            }
        }
    }

    return 0;
}
