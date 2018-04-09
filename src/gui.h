#pragma once
#ifndef GUI_H
#define GUI_H

#include "types.h"

void InitGUI(NES *nes);
void ResetGUI(NES *nes);
void SetPixel(GUI *gui, u32 x, u32 y, Color color);

#endif

