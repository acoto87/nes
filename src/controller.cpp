
#include "controller.h"

void InitController(NES *nes, s32 index)
{
    ResetController(nes, index);
}

void ResetController(NES *nes, s32 index)
{
    Controller *controller = &nes->controllers[index];
    controller->state = 0;
    controller->index = 0;
    controller->strobe = 0;
}

