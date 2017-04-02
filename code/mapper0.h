#pragma once

#include "types.h"

inline void Mapper0Init(NES *nes)
{
    // mapper 0 doesn't need any special data

    u32 chrBanks = nes->cartridge.chrBanks;
    u32 prgBanks = nes->cartridge.prgBanks;

    if (prgBanks > 0)
    {
        u8 *pgr = nes->cartridge.prg;
        CopyMemoryBytes(&nes->cpuMemory, 0x8000, pgr, 0x4000);

        if (prgBanks > 1)
        {
            CopyMemoryBytes(&nes->cpuMemory, 0xC000, pgr + 0x4000, 0x4000);
        }
        else
        {
            CopyMemoryBytes(&nes->cpuMemory, 0xC000, pgr, 0x4000);
        }
    }

    if (chrBanks > 0)
    {
        u8* chr = nes->cartridge.chr;
        CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr, 0x2000);
    }
}

inline u8 Mapper0ReadU8(NES *nes, u16 address)
{
    if (ISBETWEEN(address, 0x0000, 0x2000))
    {
        return ReadU8(&nes->ppuMemory, address);
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        return ReadU8(&nes->cpuMemory, address);
    }

    // it should no get here
    ASSERT(FALSE);
    return 0;
}

inline void Mapper0WriteU8(NES *nes, u16 address, u8 value)
{
    if (ISBETWEEN(address, 0x0000, 0x2000))
    {
        WriteU8(&nes->ppuMemory, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        // this mapper doesn't support writing to PRG
        //ASSERT(FALSE);

        WriteU8(&nes->cpuMemory, address, value);
        return;
    }

    // it should no get here
    ASSERT(FALSE);
}