#pragma once
#ifndef NES_H
#define NES_H

#include "types.h"

NES* CreateNES(Cartridge cartridge);
void ResetNES(NES *nes);
void Destroy(NES *nes);

#endif
