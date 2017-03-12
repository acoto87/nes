#pragma once

#include "types.h"
#include "memory.h"

struct Mapper2Data
{
    s32 pgrBanks;
    s32 pgrBank1;
    s32 pgrBank2;
};

inline void Mapper2Init(NES *nes)
{
    Mapper2Data *data = (Mapper2Data*)Allocate(sizeof(Mapper2Data));
    data->pgrBanks = nes->cartridge.pgrBanks;
    data->pgrBank1 = 0;
    data->pgrBank2 = data->pgrBanks - 1;

    nes->mapperData = (void*)data;
}

inline u8 Mapper2ReadU8(NES *nes, u16 address)
{
    Mapper2Data* data = (Mapper2Data*)nes->mapperData;

    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        return ReadU8(&nes->ppuMemory, address);
    }

    if (ISBETWEEN(address, 0x8000, 0xC000))
    {
        u8* pgr = nes->cartridge.pgr;
        s32 index = data->pgrBank1 * KILOBYTES(16) + (address - 0x8000);
        return pgr[index];
    }

    if (ISBETWEEN(address, 0xC000, 0x10000))
    {
        u8* pgr = nes->cartridge.pgr;
        s32 index = data->pgrBank2 * KILOBYTES(16) + (address - 0xC000);
        return pgr[index];
    }

    ASSERT(FALSE);
    return 0;
}

inline void Mapper2WriteU8(NES *nes, u16 address, u8 value)
{
    Mapper2Data* data = (Mapper2Data*)nes->mapperData;

    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        WriteU8(&nes->ppuMemory, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        data->pgrBank1 = (value & 0x07) % data->pgrBanks;
        return;
    }
    
    ASSERT(FALSE);
}