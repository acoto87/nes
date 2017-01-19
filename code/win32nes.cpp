//#include "types.h"
//#include "cartridge.h"
//#include "nes.h"
//#include "cpu.h"
//#include "ppu.h"
//
//#define STB_TRUETYPE_IMPLEMENTATION
//#include "stb_truetype.h"
//
//#define BREAKPOINT 0xC021
//
//global s64 globalPerfCountFrequency;
//global Win32BackBuffer backBuffer;
//global b32 running;
//global char debugBuffer[256];
//global stbtt_fontinfo fontInfo;
//global b32 showDebugInfo = TRUE;
//global b32 debugging = TRUE;
//global b32 stepping;
//
//internal inline LARGE_INTEGER Win32GetWallClock(void)
//{
//    LARGE_INTEGER result;
//    QueryPerformanceCounter(&result);
//    return result;
//}
//
//internal inline f32 Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
//{
//    f32 result = ((f32)(end.QuadPart - start.QuadPart) / (f32)globalPerfCountFrequency);
//    return result;
//}
//
//internal stbtt_fontinfo Win32LoadFontFile(char *fontFilePath)
//{
//    LoadedFile fontFile = LoadEntireFile(fontFilePath);
//
//    stbtt_fontinfo fontInfo;
//
//    s32 offset = stbtt_GetFontOffsetForIndex(fontFile.contents, 0);
//    stbtt_InitFont(&fontInfo, fontFile.contents, offset);
//
//    return fontInfo;
//}
//
//internal LoadedBitmap Win32LoadFontGlyph(stbtt_fontinfo fontInfo, s32 codepoint, f32 fontSize)
//{
//    LoadedBitmap bitmap = {};
//
//    s32 width, height, xoffset, yoffset;
//
//    f32 scaleY = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
//    u8 *glyphBitmap = stbtt_GetCodepointBitmap(&fontInfo, 0, scaleY, codepoint, &width, &height, &xoffset, &yoffset);
//
//    bitmap.width = width;
//    bitmap.height = height;
//    bitmap.xoffset = xoffset;
//    bitmap.yoffset = yoffset;
//    bitmap.pixels = (Color*)Allocate(width * height * sizeof(Color));
//
//    for (s32 y = 0; y < height; ++y)
//    {
//        for (s32 x = 0; x < width; ++x)
//        {
//            s32 pixel = y * width + x;
//            u8 alpha = glyphBitmap[pixel];
//            bitmap.pixels[pixel].argb = ARGB(alpha, alpha, alpha, alpha);
//        }
//    }
//
//    stbtt_FreeBitmap(glyphBitmap, NULL);
//
//    return bitmap;
//}
//
//internal void Win32ResizeDIBSection(Win32BackBuffer *buffer, int width, int height)
//{
//    buffer->bytesPerPixel = 4;
//    buffer->pitch = width * buffer->bytesPerPixel;
//
//    s32 bitmapSize = width * height * buffer->bytesPerPixel;
//
//    if (buffer->bitmap)
//    {
//        Free(buffer->bitmap);
//    }
//
//    buffer->bitmapWidth = width;
//    buffer->bitmapHeight = height;
//
//    BITMAPINFOHEADER header;
//
//    header.biSize = sizeof(BITMAPINFOHEADER);
//    header.biWidth = width;
//    header.biHeight = -height;
//    header.biPlanes = 1;
//    header.biBitCount = 32;
//    header.biCompression = BI_RGB;
//    header.biSizeImage = 0;
//    header.biXPelsPerMeter = 0;
//    header.biYPelsPerMeter = 0;
//    header.biClrUsed = 0;
//    header.biClrImportant = 0;
//
//    buffer->bitmapInfo.bmiHeader = header;
//
//    buffer->bitmap = Allocate(bitmapSize);
//}
//
//internal void Win32UpdateWindow(HDC dc, RECT *windowClientRect, Win32BackBuffer *buffer, int x, int y, int width, int height)
//{
//    int clientWidth = windowClientRect->right - windowClientRect->left;
//    int clientHeight = windowClientRect->bottom - windowClientRect->top;
//
//    StretchDIBits(
//        dc,
//        0, 0, clientWidth, clientHeight,
//        0, 0, width, height,
//        buffer->bitmap, &buffer->bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
//}
//
//internal void Win32RenderRect(s32 x, s32 y, s32 width, s32 height, Color color)
//{
//    u8 *backBitmap = (u8*)backBuffer.bitmap;
//    for (s32 i = 0; i < height; ++i)
//    {
//        for (s32 j = 0; j < width; ++j)
//        {
//            s32 backBufferX = (x + j);
//            s32 backBufferY = (y + i);
//
//            if ((backBufferY >= 0 && backBufferY < backBuffer.bitmapHeight) &&
//                (backBufferX >= 0 && backBufferX < backBuffer.bitmapWidth))
//            {
//                s32 index = i * width + j;
//
//                s32 backBufferIndex = backBufferY * backBuffer.bitmapWidth + backBufferX;
//                backBitmap[backBufferIndex * 4 + 0] = color.b;
//                backBitmap[backBufferIndex * 4 + 1] = color.g;
//                backBitmap[backBufferIndex * 4 + 2] = color.r;
//                backBitmap[backBufferIndex * 4 + 3] = color.a;
//            }
//        }
//    }
//}
//
//internal void Win32RenderBitmap(LoadedBitmap bitmap, s32 x, s32 y)
//{
//    u8 *backBitmap = (u8*)backBuffer.bitmap;
//    for (s32 bitmapY = 0; bitmapY < bitmap.height; ++bitmapY)
//    {
//        for (s32 bitmapX = 0; bitmapX < bitmap.width; ++bitmapX)
//        {
//            s32 backBufferX = (x + bitmapX + bitmap.xoffset);
//            s32 backBufferY = (y + bitmapY + bitmap.yoffset);
//
//            if ((backBufferY >= 0 && backBufferY < backBuffer.bitmapHeight) &&
//                (backBufferX >= 0 && backBufferX < backBuffer.bitmapWidth))
//            {
//                s32 index = bitmapY * bitmap.width + bitmapX;
//                Color pixel = bitmap.pixels[index];
//
//                s32 backBufferIndex = backBufferY * backBuffer.bitmapWidth + backBufferX;
//                backBitmap[backBufferIndex * 4 + 0] = pixel.b;
//                backBitmap[backBufferIndex * 4 + 1] = pixel.g;
//                backBitmap[backBufferIndex * 4 + 2] = pixel.r;
//                backBitmap[backBufferIndex * 4 + 3] = pixel.a;
//            }
//        }
//    }
//}
//
//internal void Win32RenderText(char *text, s32 x, s32 y, f32 fontSize, stbtt_fontinfo fontInfo)
//{
//    f32 scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
//
//    for (s32 ch = 0; text[ch]; ++ch)
//    {
//        s32 advance, lsb;
//        stbtt_GetCodepointHMetrics(&fontInfo, text[ch], &advance, &lsb);
//
//        LoadedBitmap bitmap = Win32LoadFontGlyph(fontInfo, text[ch], fontSize);
//        Win32RenderBitmap(bitmap, x, y);
//
//        x += (advance * scale);
//        if (text[ch + 1])
//            x += scale * stbtt_GetCodepointKernAdvance(&fontInfo, text[ch], text[ch + 1]);
//
//        Free(bitmap.pixels);
//    }
//}
//
//internal void Win32DebugPrintInfo(NES *nes, f32 dt, f32 elapsed, s32 cycles)
//{
//    f32 fontSize = 16;
//
//    CPU *cpu = &nes->cpu;
//    PPU *ppu = &nes->ppu;
//
//    {
//        Win32RenderText("SCREEN", 872, 20, fontSize, fontInfo);
//    }
//
//    {
//        s32 fpsDebugInfoX = 10, fpsDebugInfoY = 20;
//
//        Win32RenderText("FPS INFO", fpsDebugInfoX, fpsDebugInfoY, fontSize, fontInfo);
//        fpsDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "  dt: %.4f, elapsed: %.4f, cycles: %d", dt, elapsed, cycles);
//        Win32RenderText(debugBuffer, fpsDebugInfoX, fpsDebugInfoY, fontSize, fontInfo);
//    }
//
//    if (showDebugInfo)
//    {
//        s32 cpuDebugInfoX = 400, cpuDebugInfoY = 20;
//
//        Win32RenderText("CPU INFO", cpuDebugInfoX, cpuDebugInfoY, fontSize, fontInfo);
//        cpuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%6s:%02X", "A", cpu->a);
//        Win32RenderText(debugBuffer, cpuDebugInfoX, cpuDebugInfoY, fontSize, fontInfo);
//        cpuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%6s:%02X", "X", cpu->x);
//        Win32RenderText(debugBuffer, cpuDebugInfoX, cpuDebugInfoY, fontSize, fontInfo);
//        cpuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%6s:%02X", "Y", cpu->y);
//        Win32RenderText(debugBuffer, cpuDebugInfoX, cpuDebugInfoY, fontSize, fontInfo);
//        cpuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%6s:%02X", "P", cpu->p);
//        Win32RenderText(debugBuffer, cpuDebugInfoX, cpuDebugInfoY, fontSize, fontInfo);
//        cpuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "N V   B D I Z C");
//        Win32RenderText(debugBuffer, cpuDebugInfoX, cpuDebugInfoY, fontSize, fontInfo);
//        cpuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%d %d %d %d %d %d %d %d", 
//            GetBitFlag(cpu->p, NEGATIVE_FLAG) ? 1 : 0,
//            GetBitFlag(cpu->p, OVERFLOW_FLAG) ? 1 : 0,
//            GetBitFlag(cpu->p, EMPTY_FLAG) ? 1 : 0,
//            GetBitFlag(cpu->p, BREAK_FLAG) ? 1 : 0,
//            GetBitFlag(cpu->p, DECIMAL_FLAG) ? 1 : 0,
//            GetBitFlag(cpu->p, INTERRUPT_FLAG) ? 1 : 0,
//            GetBitFlag(cpu->p, ZERO_FLAG) ? 1 : 0,
//            GetBitFlag(cpu->p, CARRY_FLAG) ? 1 : 0);
//        Win32RenderText(debugBuffer, cpuDebugInfoX, cpuDebugInfoY, fontSize, fontInfo);
//        cpuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%6s:%02X", "SP", cpu->sp);
//        Win32RenderText(debugBuffer, cpuDebugInfoX, cpuDebugInfoY, fontSize, fontInfo);
//        cpuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%6s:%02X", "PC", cpu->pc);
//        Win32RenderText(debugBuffer, cpuDebugInfoX, cpuDebugInfoY, fontSize, fontInfo);
//        cpuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%6s:%d", "CYCLES", cpu->cycles);
//        Win32RenderText(debugBuffer, cpuDebugInfoX, cpuDebugInfoY, fontSize, fontInfo);
//    }
//
//    if (showDebugInfo)
//    {
//        s32 ppuDebugInfoX = 550, ppuDebugInfoY = 20;
//
//        Win32RenderText("PPU INFO", ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%02X", "CTRL (0x2000)", ppu->control);
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%02X", "MASK (0x2001)", ppu->mask);
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%02X", "STATUS 0x2002", ppu->status);
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%02X", "SPRITE_ADDR (0x2003)", ReadU8(&nes->cpuMemory, 0x2003));
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%02X", "SPRITE_DATA (0x2004)", ReadU8(&nes->cpuMemory, 0x2004));
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%02X", "SCROLL_REG (0x2005)", ReadU8(&nes->cpuMemory, 0x2005));
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%02X", "MEMORY_ADDR (0x2006)", ReadU8(&nes->cpuMemory, 0x2006));
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%02X", "MEMORY_DATA (0x2007)", ReadU8(&nes->cpuMemory, 0x2007));
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%3d", "SCANLINE", ppu->scanline);
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%3d", "CYCLE", ppu->cycle);
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//        ppuDebugInfoY += 20;
//
//        memset(debugBuffer, 0, sizeof(debugBuffer));
//        sprintf(debugBuffer, "%20s:%d", "TOTAL_CYC", ppu->totalCycles);
//        Win32RenderText(debugBuffer, ppuDebugInfoX, ppuDebugInfoY, fontSize, fontInfo);
//    }
//
//    if (showDebugInfo)
//    {
//        s32 instrDebugInfoX = 10, instrDebugInfoY = 70;
//        u16 pc = cpu->pc;
//
//        Win32RenderText("INSTRUCTIONS", instrDebugInfoX, instrDebugInfoY, fontSize, fontInfo);
//        instrDebugInfoY += 20;
//
//        for (s32 i = 0; i < 20; ++i)
//        {
//            memset(debugBuffer, 0, sizeof(debugBuffer));
//
//            u8 opcode = ReadCPUU8(nes, pc);
//            CPUInstruction *instruction = &cpuInstructions[opcode];
//
//            s32 col = 0;
//            b32 currentInstr = (pc == cpu->pc);
//            b32 breakpointHit = (pc == BREAKPOINT);
//
//            col += currentInstr
//                ? (breakpointHit 
//                    ? sprintf(debugBuffer, "O>%04X %02X", pc, instruction->opcode)
//                    : sprintf(debugBuffer, "> %04X %02X", pc, instruction->opcode))
//                : (breakpointHit
//                    ? sprintf(debugBuffer, "O %04X %02X", pc, instruction->opcode)
//                    : sprintf(debugBuffer, "  %04X %02X", pc, instruction->opcode));
//            
//            switch (instruction->bytesCount)
//            {
//                case 2:
//                {
//                    col += sprintf(debugBuffer + col, " %02X", ReadU8(&nes->cpuMemory, nes->cpu.pc + 1));
//                    break;
//                }
//                case 3:
//                {
//                    col += sprintf(debugBuffer + col, " %02X %02X", ReadU8(&nes->cpuMemory, cpu->pc + 1), ReadU8(&nes->cpuMemory, cpu->pc + 2));
//                    break;
//                }
//                default:
//                {
//                    break;
//                }
//            }
//
//            while (col < 15)
//            {
//                col += sprintf(debugBuffer + col, " ");
//            }
//
//            col += sprintf(debugBuffer + col, "%10s", GetInstructionStr(instruction->instruction));
//
//            Win32RenderText(debugBuffer, instrDebugInfoX, instrDebugInfoY, fontSize, fontInfo);
//
//            pc += instruction->bytesCount;
//            instrDebugInfoY += 20;
//        }
//    }
//
//    if (showDebugInfo)
//    {
//        s32 memoryDebugInfoX = 10, memoryDebugInfoY = 540;
//
//        Win32RenderText("ZERO PAGE", memoryDebugInfoX, memoryDebugInfoY, fontSize, fontInfo);
//        memoryDebugInfoY += 20;
//
//        u32 linesCount = 0x100 / 16;
//        for (s32 i = 0; i < linesCount; ++i)
//        {
//            memset(debugBuffer, 0, sizeof(debugBuffer));
//
//            s32 c = sprintf(debugBuffer, "%04X", i * 32);
//
//            for (s32 j = 0; j < 16; ++j)
//            {
//                u8 v = ReadU8(&nes->cpuMemory, i * 32 + j);
//                c += sprintf(debugBuffer + c, " %02X", v);
//            }
//
//            Win32RenderText(debugBuffer, memoryDebugInfoX, memoryDebugInfoY, fontSize, fontInfo);
//            memoryDebugInfoY += 20;
//        }
//
//        memoryDebugInfoX = 600, memoryDebugInfoY = 540;
//
//        Win32RenderText("STACK", memoryDebugInfoX, memoryDebugInfoY, fontSize, fontInfo);
//        memoryDebugInfoY += 20;
//        
//        linesCount = 0x100 / 16;
//        for (s32 i = 0; i < linesCount; ++i)
//        {
//            memset(debugBuffer, 0, sizeof(debugBuffer));
//
//            s32 c = sprintf(debugBuffer, "%04X", 0x100 + i * 32);
//
//            for (s32 j = 0; j < 16; ++j)
//            {
//                u8 v = ReadU8(&nes->cpuMemory, i * 32 + j);
//                c += sprintf(debugBuffer + c, " %02X", v);
//            }
//
//            Win32RenderText(debugBuffer, memoryDebugInfoX, memoryDebugInfoY, fontSize, fontInfo);
//            memoryDebugInfoY += 20;
//        }
//    }
//}
//
//LRESULT CALLBACK Win32MainWindowCallback(
//    HWND   window,
//    UINT   msg,
//    WPARAM wParam,
//    LPARAM lParam)
//{
//    LRESULT result = 0;
//
//    switch (msg)
//    {
//        case WM_DESTROY:
//        {
//            running = false;
//            break;
//        }
//
//        case WM_CLOSE:
//        {
//            running = false;
//            break;
//        }
//
//        case WM_ACTIVATEAPP:
//        {
//            //OutputDebugStringA("WM_ACTIVATEAPP\n");
//            break;
//        }
//
//        case WM_KEYUP:
//        {
//            u32 key = (u32)wParam;
//
//            if (key == VK_F8)
//            {
//                showDebugInfo = !showDebugInfo;
//            }
//            else if (key == VK_F9)
//            {
//                debugging = !debugging;
//                stepping = FALSE;
//            }
//            else if (key == VK_F11)
//            {
//                stepping = debugging;
//            }
//
//            break;
//        }
//
//        case WM_PAINT:
//        {
//            //OutputDebugString("WM_PAINT\n");
//
//            RECT clientRect;
//            GetClientRect(window, &clientRect);
//
//            PAINTSTRUCT paint;
//            HDC dc = BeginPaint(window, &paint);
//            if (dc)
//            {
//                int x = paint.rcPaint.left;
//                int y = paint.rcPaint.top;
//                int width = paint.rcPaint.right - x;
//                int height = paint.rcPaint.bottom - y;
//
//                Win32UpdateWindow(dc, &clientRect, &backBuffer, x, y, width, height);
//                EndPaint(window, &paint);
//            }
//            break;
//        }
//
//        default:
//        {
//            result = DefWindowProc(window, msg, wParam, lParam);
//            break;
//        }
//    }
//
//    return result;
//}
//
//int CALLBACK WinMain(
//    HINSTANCE instance,
//    HINSTANCE prevInstance,
//    LPSTR     cmdLine,
//    int       cmdShow)
//{
//    LARGE_INTEGER initialCounter = Win32GetWallClock();
//
//    fontInfo = Win32LoadFontFile("C:/Windows/Fonts/consola.ttf");
//
//    s32 monitorRefreshHz = 60;
//    f32 gameUpdateHz = (f32)(monitorRefreshHz);
//    f32 targetSecondsPerFrame = 1.0f / (f32)gameUpdateHz;
//    f32 dt = 0;
//
//    LARGE_INTEGER perfCountFrequencyResult;
//    QueryPerformanceFrequency(&perfCountFrequencyResult);
//    globalPerfCountFrequency = perfCountFrequencyResult.QuadPart;
//
//    Cartridge cartridge = {};
//    if (LoadNesRom("nestest.nes", &cartridge))
//    {
//        NES *nes = CreateNES(cartridge);
//        if (nes)
//        {
//            CPUInstruction debugCPUInstructions[CPU_PGR_BANK_SIZE * 2];
//            for (s32 i = 0; i < CPU_PGR_BANK_SIZE * 2; ++i)
//            {
//                u8 opcode = ReadCPUU8(nes, CPU_PGR_BANK_0_OFFSET + i);
//                debugCPUInstructions[i] = cpuInstructions[opcode];
//            }
//
//            WNDCLASS windowClass = {};
//            windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
//            windowClass.lpfnWndProc = Win32MainWindowCallback;
//            windowClass.hInstance = instance;
//            windowClass.hCursor = LoadCursor(0, IDC_ARROW);
//            windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
//            // windowClass.hIcon = ;
//            windowClass.lpszClassName = "NESWindowClass";
//
//            if (RegisterClassA(&windowClass))
//            {
//                u32 windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
//
//                RECT windowRect = { 0, 0, 1200, 900 };
//                AdjustWindowRectEx(
//                    &windowRect,
//                    windowStyle,
//                    NULL,
//                    NULL);
//
//                HWND window = CreateWindowEx(
//                    NULL,
//                    windowClass.lpszClassName,
//                    "Nes emulator",
//                    windowStyle,
//                    CW_USEDEFAULT,
//                    CW_USEDEFAULT,
//                    windowRect.right - windowRect.left,
//                    windowRect.bottom - windowRect.top,
//                    NULL,
//                    NULL,
//                    instance,
//                    NULL);
//
//                if (window)
//                {
//                    int width, height;
//
//                    RECT clientRect;
//                    if (GetClientRect(window, &clientRect))
//                    {
//                        width = clientRect.right - clientRect.left;
//                        height = clientRect.bottom - clientRect.top;
//                        Win32ResizeDIBSection(&backBuffer, width, height);
//                    }
//
//                    running = true;
//
//                    LARGE_INTEGER startCounter = Win32GetWallClock();
//                    dt = Win32GetSecondsElapsed(initialCounter, startCounter);
//
//                    while (running)
//                    {
//                        MSG msg;
//                        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
//                        {
//                            if (msg.message == WM_QUIT)
//                            {
//                                running = false;
//                            }
//
//                            TranslateMessage(&msg);
//                            DispatchMessage(&msg);
//                        }
//
//                        Win32RenderRect(0, 0, width, height, GetColor(255, 0, 0, 0));
//                        Win32RenderRect(2 * width / 3, 0, 5, 500, GetColor(255, 127, 127, 127));
//                        Win32RenderRect(0, 500, width, 5, GetColor(255, 127, 127, 127));
//
//                        LARGE_INTEGER wallClock = Win32GetWallClock();
//                        f32 elapsed = Win32GetSecondsElapsed(startCounter, wallClock);
//
//                        // This is how many cycles the cpu run on this frame
//                        //s32 cycles = (s32)(CPU_FREQ * dt);
//                        //s32 originalCycles = cycles;
//
//                        if (!debugging)
//                        {
//                            if (nes->cpu.pc == BREAKPOINT)
//                            {
//                                showDebugInfo = TRUE;
//                                debugging = TRUE;
//                                stepping = FALSE;
//                            }
//                        }
//
//                        if (!debugging || stepping)
//                        {
//                            //while (cycles > 0)
//                            {
//                                CPUStep step = StepCPU(nes);
//
//                                for (u32 i = 0; i < 3 * step.cycles; i++)
//                                {
//                                    StepPPU(nes);
//                                }
//
//                                for (u32 i = 0; i < step.cycles; i++)
//                                {
//                                    // StepAPU(nes);
//                                }
//
//                                //cycles -= step.cycles;
//                            }
//
//                            if (debugging)
//                            {
//                                stepping = FALSE;
//                            }
//                        }
//
//                        wallClock = Win32GetWallClock();
//                        elapsed = Win32GetSecondsElapsed(startCounter, wallClock) - elapsed;
//
//                        // render audio
//
//                        // render frame
//                        GUI *gui = &nes->gui;
//                        LoadedBitmap bitmap = { gui->width , gui->height, 0, 0, gui->pixels };
//                        Win32RenderBitmap(bitmap, 872, 72);
//
//                        // debug text
//                        Win32DebugPrintInfo(nes, dt, elapsed, 1);
//                        
//                        // invalidate window
//                        RECT clientRect;
//                        GetClientRect(window, &clientRect);
//
//                        InvalidateRect(window, &clientRect, FALSE);
//
//                        LARGE_INTEGER endCounter = Win32GetWallClock();
//                        dt = Win32GetSecondsElapsed(startCounter, endCounter);
//                        startCounter = endCounter;
//                    }
//                }
//            }
//        }
//    }
//
//    return 0;
//}
//
////int main()
////{
////    s32 monitorRefreshHz = 60;
////    f32 gameUpdateHz = (f32)(monitorRefreshHz);
////    f32 targetSecondsPerFrame = 1.0f / (f32)gameUpdateHz;
////
////    LARGE_INTEGER perfCountFrequencyResult;
////    QueryPerformanceFrequency(&perfCountFrequencyResult);
////    globalPerfCountFrequency = perfCountFrequencyResult.QuadPart;
////
////    LARGE_INTEGER initialCounter = Win32GetWallClock();
////
////    f32 dt = 0;
////
////    Win32ResizeDIBSection(&backBuffer, 256, 240);
////
////    Cartridge cartridge = {};
////    b32 loaded = LoadNesRom("nestest.nes", &cartridge);
////    if (loaded)
////    {
////        NES *nes = CreateNES(cartridge);
////        if (nes)
////        {
////            LARGE_INTEGER startCounter = Win32GetWallClock();
////            dt = Win32GetSecondsElapsed(initialCounter, startCounter);
////
////            while (true)
////            {
////                // This is how many cycles the cpu run on this frame
////                s32 cycles = (s32)(CPU_FREQ * dt);
////
////                while (cycles > 0)
////                {
////                    u32 cpuCycles = StepCPU(nes);
////
////                    for (u32 i = 0; i < 3 * cpuCycles; i++)
////                    {
////                        StepPPU(nes);
////                    }
////
////                    for (u32 i = 0; i < cpuCycles; i++)
////                    {
////                        // StepAPU(nes);
////                    }
////
////                    cycles -= cpuCycles;
////                }
////
////                // render audio
////
////                // render frame
////                GUI *gui = &nes->gui;
////
////                u8 *buffer = (u8*)backBuffer.bitmap;
////                for (u32 y = 0; y < gui->height; ++y)
////                {
////                    for (u32 x = 0; x < gui->width; ++x)
////                    {
////                        u32 index = y * gui->width + x;
////                        Color pixel = gui->screenBuffer[index];
////
////                        buffer[index * 4 + 0] = pixel.r;
////                        buffer[index * 4 + 1] = pixel.g;
////                        buffer[index * 4 + 2] = pixel.b;
////                        buffer[index * 4 + 3] = pixel.a;
////
////                        /*buffer[index * 4 + 0] = pixel.b;
////                        buffer[index * 4 + 1] = pixel.g;
////                        buffer[index * 4 + 2] = pixel.r;
////                        buffer[index * 4 + 3] = pixel.a;*/
////                    }
////                }
////
////                //s32 err = stbi_write_png("frame.png", backBuffer.bitmapWidth, backBuffer.bitmapHeight, 4, &backBuffer.bitmap, backBuffer.bitmapWidth * 4);
////                //s32 err = stbi_write_bmp("frame.bmp", backBuffer.bitmapWidth, backBuffer.bitmapHeight, 4, &backBuffer.bitmap);
////
////                LARGE_INTEGER endCounter = Win32GetWallClock();
////                dt = Win32GetSecondsElapsed(startCounter, endCounter);
////                startCounter = endCounter;
////            }
////        }
////    }
////
////    getchar();
////
////    return 0;
////}