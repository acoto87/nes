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
    // Run until 81FB is about to execute, comparing with Nintendulator
    // Render VRAM address to debug

    LARGE_INTEGER initialCounter = Win32GetWallClock();

    LARGE_INTEGER perfCountFrequencyResult;
    QueryPerformanceFrequency(&perfCountFrequencyResult);
    globalPerfCountFrequency = perfCountFrequencyResult.QuadPart;

    f32 dt = 0;

    Cartridge cartridge = {};
    if (!LoadNesRom("palette.nes", &cartridge))
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
                    if (nes->cpu.pc == breakpoint)
                    {
                        debugging = TRUE;
                        stepping = FALSE;
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

                if (nk_begin(ctx, "FPS INFO", nk_rect(10, 10, 300, 150), flags))
                {
                    nk_layout_row_dynamic(ctx, 20, 1);
                    nk_label(ctx, DebugText("dt: %.4f", dt), NK_TEXT_LEFT);

                    nk_layout_row_dynamic(ctx, 20, 3);
                    if (nk_button_label(ctx, "Run"))
                    {
                        debugging = FALSE;
                        stepping = FALSE;
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

                if (nk_begin(ctx, "CPU INFO", nk_rect(320, 10, 350, 320), flags))
                {
                    nk_layout_row_dynamic(ctx, 20, 3);

                    nk_label(ctx, DebugText("%s:%02X", "A", cpu->a), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%2s:%02X", "P", cpu->p), NK_TEXT_LEFT);
                    nk_label(ctx, "N V   B D I Z C", NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%s:%02X", "X", cpu->x), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%2s:%02X", "SP", cpu->sp), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%d %d %d %d %d %d %d %d",
                        GetBitFlag(cpu->p, NEGATIVE_FLAG) ? 1 : 0,
                        GetBitFlag(cpu->p, OVERFLOW_FLAG) ? 1 : 0,
                        GetBitFlag(cpu->p, EMPTY_FLAG) ? 1 : 0,
                        GetBitFlag(cpu->p, BREAK_FLAG) ? 1 : 0,
                        GetBitFlag(cpu->p, DECIMAL_FLAG) ? 1 : 0,
                        GetBitFlag(cpu->p, INTERRUPT_FLAG) ? 1 : 0,
                        GetBitFlag(cpu->p, ZERO_FLAG) ? 1 : 0,
                        GetBitFlag(cpu->p, CARRY_FLAG) ? 1 : 0), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("%s:%02X", "Y", cpu->y), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%2s:%02X", "PC", cpu->pc), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%s:%d", "CYCLES", cpu->cycles), NK_TEXT_LEFT);

                    nk_layout_row_dynamic(ctx, 20, 1);
                    nk_label(ctx, DebugText("%s:%02X", "VRAM (v)", ppu->v), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%s:%02X", "VRAM (t)", ppu->t), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%s:%02X", "Fine X (x)", ppu->x), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("%s:%02X", "Write Toggle (w)", ppu->w), NK_TEXT_LEFT);
                }
                nk_end(ctx);

                if (nk_begin(ctx, "PPU INFO", nk_rect(680, 10, 200, 320), flags))
                {
                    nk_layout_row_dynamic(ctx, 20, 1);

                    nk_label(ctx, DebugText("CTRL (0x2000):%02X", ppu->control), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("MASK (0x2001):%02X", ppu->mask), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("STAT (0x2002):%02X", ppu->status), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("SPRA (0x2003):%02X", ppu->oamAddress), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("SPRD (0x2004):%02X", ppu->oamData), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("SCRR (0x2005):%02X", ppu->scroll), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("MEMA (0x2006):%02X", ppu->address), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("MEMD (0x2007):%02X", ppu->data), NK_TEXT_LEFT);

                    nk_label(ctx, DebugText("SCANLINE:%3d", ppu->scanline), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("CYCLE:%3d", ppu->cycle), NK_TEXT_LEFT);
                    nk_label(ctx, DebugText("TOTAL_CYCLES:%d", ppu->totalCycles), NK_TEXT_LEFT);
                }
                nk_end(ctx);

                if (nk_begin(ctx, "INSTRUCTIONS", nk_rect(10, 170, 300, windowHeight - 180), flags))
                {
                    // POR AHORA PARECE QUE ESTA EJECUTANDOSE BIEN HASTA LA INSTRUCCION: '0x85DC'
                    // ESTA ES EL HANDLER DE 'VBLANK', EN LA QUE SE EJECUTAN LAS INSTRUCCIONES DE LOS SPRITES
                    // LUEGO, REVISAR BIEN LA FUNCIONALIDAD DE SPRITES, OAMADDRESS, OAMDATA Y DEMAS.
                    // HASTA ESTE PUNTO TODO COINCIDE CON NINTENDULATOR, DESPUES DE ESCRIBIR EN '4014' DIFIEREN LA CANTIDAD
                    // DE CICLOS DEL PPU, PROBABLMENTE PORQUE EL CPU DEBA ESPERAR UNA CIERTA CANTIDAD DE CICLOS CORRESPONDIENTE
                    // A LA SUPUESTA COPIA DE LOS 256 BYTES DE LOS SPRITES.
                    
                    // REVISAR TAMBIEN ESTA SECCION DEL CODIGO PARA CUANDO SE PONGA UNA DIRECCION EN LA SECCION DE INSTRUCCTIONS
                    // SE VAYA A LA INSTRUCCION MAS CERCANA, Y NO COJA LA DIRECCION LITERAL, YA QUE PUEDE QUE EN ESA DIRECCION NO
                    // HAYA NINGUNA INSTRUCCION O SEA UN PARAMETRO

                    static const float ratio[] = { 100, 100 };
                    static char text[12], text2[5];
                    static s32 len, len2;

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

                if (nk_begin(ctx, "MEMORY", nk_rect(320, 340, windowWidth - 640, windowHeight - 350), flags))
                {
                    enum options { CPU_MEM, PPU_MEM };
                    static s32 option;

                    static const float ratio[] = { 80, 80, 100, 100 };
                    static char text[12];
                    static s32 len;

                    nk_layout_row(ctx, NK_STATIC, 25, 4, ratio);
                    option = nk_option_label(ctx, "CPU", option == CPU_MEM) ? CPU_MEM : option;
                    option = nk_option_label(ctx, "PPU", option == PPU_MEM) ? PPU_MEM : option;
                    nk_label(ctx, "  Address: ", NK_TEXT_LEFT);
                    nk_edit_string(ctx, NK_EDIT_SIMPLE, text, &len, 12, nk_filter_hex);

                    nk_layout_row_dynamic(ctx, 20, 1);

                    u16 address = 0x0000;

                    if (len > 0)
                    {
                        text[len] = 0;
                        address = (u16)strtol(text, NULL, 16);
                        if (address < 0x0000 || address > 0xFFFF)
                        {
                            address = 0x0000;
                        }
                    }

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
                nk_end(ctx);

                if (nk_begin(ctx, "SCREEN", nk_rect(windowWidth - 310, 10, 300, 320), flags))
                {
                    nk_layout_row_static(ctx, 256, 240, 1);

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

                        /*local u8 pixels[128 * 128 * 4];

                        for (s32 index = 0; index < 256; ++index)
                        {
                            for (s32 y = 0; y < 8; ++y)
                            {
                                u8 row1 = ReadPPUU8(nes, index * 16 + y);
                                u8 row2 = ReadPPUU8(nes, index * 16 + 8 + y);

                                for (s32 x = 0; x < 8; ++x)
                                {
                                    Color c = {};
                                    c.a = 0xFF;

                                    u8 h = ((row2 >> (7 - x)) & 0x1);
                                    u8 l = ((row1 >> (7 - x)) & 0x1);
                                    u8 v = (h << 0x1) | l;
                                    switch (v)
                                    {
                                        case 1:
                                        {
                                            c.r = 0xFF;
                                            break;
                                        }

                                        case 2:
                                        {
                                            c.g = 0xFF;
                                            break;
                                        }

                                        case 3:
                                        {
                                            c.b = 0xFF;
                                            break;
                                        }

                                        default:
                                        {
                                            break;
                                        }
                                    }

                                    s32 x1 = (index % 16) * 8 + x;
                                    s32 y1 = (index / 16) * 8 + y;
                                    s32 b = y1 * 128 + x1;

                                    pixels[b * 4 + 0] = c.b;
                                    pixels[b * 4 + 1] = c.g;
                                    pixels[b * 4 + 2] = c.r;
                                    pixels[b * 4 + 3] = c.a;
                                }
                            }
                        }

                        struct nk_image image;
                        image.w = 128;
                        image.h = 128;
                        image.handle.ptr = pixels;
                        image.region[0] = space.x;
                        image.region[1] = space.y;
                        image.region[2] = space.w;
                        image.region[3] = space.h;

                        nk_draw_image(canvas, space, &image, nk_rgb(255, 0, 0));*/
                    }
                }
                nk_end(ctx);

                if (nk_begin(ctx, "STACK", nk_rect(windowWidth - 310, 340, 300, windowHeight - 350), flags))
                {
                    static const float ratio[] = { 100, 100 };
                    static char text[12];
                    static s32 len;

                    nk_layout_row(ctx, NK_STATIC, 25, 2, ratio);
                    nk_label(ctx, "  Address: ", NK_TEXT_LEFT);
                    nk_edit_string(ctx, NK_EDIT_SIMPLE, text, &len, 12, nk_filter_hex);

                    nk_layout_row_dynamic(ctx, 20, 1);

                    u16 address = 0x0100;

                    if (len > 0)
                    {
                        text[len] = 0;
                        address = (u16)strtol(text, NULL, 16);
                        if (address < 0x0100 || address > 0x0200)
                        {
                            address = 0x0100;
                        }
                    }

                    for (s32 i = 0; i < 16; ++i)
                    {
                        memset(debugBuffer, 0, sizeof(debugBuffer));

                        s32 col = sprintf(debugBuffer, "%04X: ", address + i * 8);

                        for (s32 j = 0; j < 8; ++j)
                        {
                            u8 v = ReadU8(&nes->cpuMemory, address + i * 8 + j);
                            col += sprintf(debugBuffer + col, " %02X", v);
                        }

                        nk_label(ctx, debugBuffer, NK_TEXT_LEFT);
                    }
                }
                nk_end(ctx);

                nk_gdi_render(nk_rgb(0, 0, 0));

                LARGE_INTEGER endCounter = Win32GetWallClock();
                dt = Win32GetSecondsElapsed(startCounter, endCounter);
                startCounter = endCounter;
            }
        }
    }

    return 0;
}
