#pragma once

#include "types.h"

inline void Mapper0Init(NES *nes)
{
    // mapper 0 doesn't need any special data
}

inline u8 Mapper0ReadU8(NES *nes, u16 address)
{
    if (ISBETWEEN(address, 0x00, 0x2000))
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
    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        WriteU8(&nes->ppuMemory, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        WriteU8(&nes->cpuMemory, address, value);
        return;
    }

    // it should no get here
    ASSERT(FALSE);
}