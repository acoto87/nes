#pragma once
#ifndef NES_H
#define NES_H

#include "types.h"

#define HEADER_SIZE 16
#define CPU_PRG_BANK_SIZE KILOBYTES(16)
#define CHR_BANK_SIZE KILOBYTES(8)

#define VMIRROR_MASK BIT0_MASK
#define BATTERYPACKED_MASK BIT1_MASK
#define TRAINER_MASK BIT2_MASK
#define VRAMLAYOUT_MASK BIT3_MASK
#define MAPPERLOWER_MASK LOWERBITS_MASK
#define MAPPERHIGHER_MASK HIGHERBITS_MASK
#define VSSYSTEM_MASK BIT0_MASK
#define NTSC_MASK BIT0_MASK

b32 LoadNesRom(char *filePath, Cartridge *cartridge);
NES* CreateNES(Cartridge cartridge);
void ResetNES(NES *nes);
void Destroy(NES *nes);

#endif
