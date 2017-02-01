#pragma once

#include "types.h"

enum Buttons
{
    BUTTON_A = 0,
    BUTTON_B = 1,
    BUTTON_SELECT = 2,
    BUTTON_START = 3,
    BUTTON_UP = 4,
    BUTTON_DOWN = 5,
    BUTTON_LEFT = 6,
    BUTTON_RIGHT = 7
};

inline void SetButton(NES *nes, s32 index, Buttons button, b32 isDown)
{
    Controller *controller = &nes->controllers[index];

    u32 state = controller->state;

    if (isDown)
    {
        state |= (1 << button);
    }
    else
    {
        state &= ~(1 << button);
    }

    controller->state = state;
}

inline u8 ReadControllerU8(NES *nes, s32 index)
{
    Controller *controller = &nes->controllers[index];

    u8 value = 0;

    if (controller->index < 8)
    {
        if (GetBitFlag(controller->state, controller->index))
            value = 1;
    }

    controller->index++;

    if (controller->strobe)
    {
        controller->index = 0;
    }

    return value;
}

inline void WriteControllerU8(NES *nes, s32 index, u8 value)
{
    Controller *controller = &nes->controllers[index];
    controller->strobe = value & 0x01;
    if (controller->strobe)
    {
        controller->index = 0;
    }
}

void InitController(NES *nes, s32 index);
void ResetController(NES *nes, s32 index);
