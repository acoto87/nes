#include "mapper66.h"
#include "memory.h"

void Mapper66Init(NES* nes)
{
    u32 prgBanks = nes->cartridge.prgBanks;
    u32 chrBanks = nes->cartridge.chrBanks;

    if (prgBanks >= 2) {
        u8* prg = nes->cartridge.prg;
        CopyMemoryBytes(&nes->cpuMemory, 0x8000, prg, 0x8000);
    }

    if (chrBanks > 0) {
        u8* chr = nes->cartridge.chr;
        CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr, 0x2000);
    }
}

u8 Mapper66ReadU8(NES* nes, u16 address)
{
    if (ISBETWEEN(address, 0x0000, 0x2000)) {
        return ReadU8(&nes->ppuMemory, address);
    }

    if (ISBETWEEN(address, 0x8000, 0x10000)) {
        return ReadU8(&nes->cpuMemory, address);
    }

    ASSERT(false);
    return 0;
}

void Mapper66WriteU8(NES* nes, u16 address, u8 value)
{
    if (ISBETWEEN(address, 0x0000, 0x2000)) {
        WriteU8(&nes->ppuMemory, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000)) {
        u8* prg = nes->cartridge.prg;
        u8* chr = nes->cartridge.chr;
        s32 prg32kBanks = (s32)(nes->cartridge.prgSizeInBytes / 0x8000);
        s32 chr8kBanks = (s32)(nes->cartridge.chrSizeInBytes / 0x2000);

        if (prg32kBanks > 0) {
            s32 prgBank = ((value >> 4) & 0x03) % prg32kBanks;
            CopyMemoryBytes(&nes->cpuMemory, 0x8000, prg + prgBank * 0x8000, 0x8000);
        }

        if (chr8kBanks > 0) {
            s32 chrBank = (value & 0x03) % chr8kBanks;
            CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr + chrBank * 0x2000, 0x2000);
        }

        return;
    }

    ASSERT(false);
}
