#pragma once

#include "types.h"
#include "memory.h"

struct Mapper3Data
{
    s32 chrBank;
};

inline void Mapper3Init(NES *nes)
{
    Mapper3Data *data = (Mapper3Data*)Allocate(sizeof(Mapper3Data));
    data->chrBank = 0;

    nes->mapperData = (void*)data;
}

inline u8 Mapper3ReadU8(NES *nes, u16 address)
{
    Mapper3Data* data = (Mapper3Data*)nes->mapperData;

    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        u8* chr = nes->cartridge.chr;
        s32 index = data->chrBank * KILOBYTES(8) + address;
        return chr[index];
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        u8* pgr = nes->cartridge.pgr;
        s32 index = address - 0x8000;
        return pgr[index];
    }

    ASSERT(FALSE);
    return 0;
}

inline void Mapper3WriteU8(NES *nes, u16 address, u8 value)
{
    Mapper3Data* data = (Mapper3Data*)nes->mapperData;

    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        u8* chr = nes->cartridge.chr;
        s32 index = data->chrBank * KILOBYTES(8) + address;
        chr[index] = value;
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        data->chrBank = (value & 0x03);
        return;
    }
    
    ASSERT(FALSE);
}