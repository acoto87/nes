#pragma once

#include "types.h"

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
