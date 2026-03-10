#ifndef NES_H
#define NES_H

#include "types.h"

b32 LoadNesRom(char *filePath, Cartridge *cartridge);
NES* CreateNES(Cartridge cartridge);
void ResetNES(NES *nes);
void Destroy(NES *nes);
void Save(NES *nes, char *filePath);
NES* LoadNESSave(char *filePath);
void InitMapper(NES *nes);

#endif
