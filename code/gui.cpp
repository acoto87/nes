#include "gui.h"

void InitGUI(NES *nes)
{
    GUI *gui = &nes->gui;

    gui->width = PPU_SCREEN_WIDTH;
    gui->height = PPU_SCREEN_HEIGHT;

    ASSERT(sizeof(gui->pixels) == gui->width * gui->height * sizeof(Color));
}

void ResetGUI(NES *nes)
{
    GUI *gui = &nes->gui;

    gui->width = PPU_SCREEN_WIDTH;
    gui->height = PPU_SCREEN_HEIGHT;

    memset(gui->pixels, 0, PPU_SCREEN_WIDTH * PPU_SCREEN_HEIGHT * sizeof(Color));

    ASSERT(sizeof(gui->pixels) == gui->width * gui->height * sizeof(Color));
}

void SetPixel(GUI *gui, u32 x, u32 y, Color color)
{
    ASSERT(x >= 0 && x < gui->width);
    ASSERT(y >= 0 && y < gui->height);

    gui->pixels[y * gui->width + x] = color;
}