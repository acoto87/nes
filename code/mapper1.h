#pragma once

#include "types.h"
#include "memory.h"

struct Mapper1Data
{
    u8 control;
    u8 shift;
    
    u8 prgMode;
    /*u8 prgBank;*/
    
    u8 chrMode;
    /*u8 chrBank0;
    u8 chrBank1;*/
    
    /*s32 prgBanks;
    s32 chrBanks;
    s32 prgOffsets[2];
    s32 chrOffsets[2];*/
};

//inline s32 PrgBankOffset(Mapper1Data* data, s32 index)
//{
//    if (index >= 0x80)
//    {
//        index -= 0x100;
//    }
//    index %= data->prgBanks;
//    s32 offset = index * 0x4000;
//    if (offset < 0) {
//        offset += data->prgBanks * 0x4000;
//    }
//    return offset;
//}
//
//inline s32 ChrBankOffset(Mapper1Data* data, s32 index)
//{
//    if (data->chrBanks == 0)
//        return 0;
//
//    if (index >= 0x80)
//    {
//        index -= 0x100;
//    }
//    index %= data->chrBanks;
//    s32 offset = index * 0x1000;
//    if (offset < 0) {
//        offset += data->chrBanks * 0x1000;
//    }
//    return offset;
//}
//
//inline void UpdateOffsets(NES *nes, Mapper1Data *data)
//{
//    switch (data->prgMode)
//    {
//        case 0:
//        case 1:
//        {
//            data->prgOffsets[0] = PrgBankOffset(data, data->prgBank & 0xFE);
//            data->prgOffsets[1] = PrgBankOffset(data, data->prgBank & 0x01);
//            break;
//        }
//
//        case 2:
//        {
//            data->prgOffsets[0] = 0;
//            data->prgOffsets[1] = PrgBankOffset(data, data->prgBank);
//            break;
//        }
//
//        case 3:
//        {
//            data->prgOffsets[0] = PrgBankOffset(data, data->prgBank);
//            data->prgOffsets[1] = PrgBankOffset(data, -1);
//            break;
//        }
//
//    }
//
//    switch (data->chrMode)
//    {
//        case 0:
//        {
//            data->chrOffsets[0] = ChrBankOffset(data, data->chrBank0 & 0xFE);
//            data->chrOffsets[1] = ChrBankOffset(data, data->chrBank0 | 0x01);
//            break;
//        }
//
//        case 1:
//        {
//            data->chrOffsets[0] = ChrBankOffset(data, data->chrBank0);
//            data->chrOffsets[1] = ChrBankOffset(data, data->chrBank1);
//            break;
//        }
//
//    }
//}

inline void WriteControl(NES *nes, Mapper1Data *data, u8 value)
{
    data->control = value;
    data->chrMode = (value >> 4) & 1;
    data->prgMode = (value >> 2) & 3;
    s32 mirror = value & 3;
    switch (mirror)
    {
        case 0:
        {
            // @FIX: SUPPORT HERE MIRRORING SINGLE0

            nes->cartridge.mirrorType = MIRROR_FOUR;
            break;
        }

        case 1:
        {
            // @FIX: SUPPORT HERE MIRRORING SINGLE1

            nes->cartridge.mirrorType = MIRROR_FOUR;
            break;
        }

        case 2:
        {
            nes->cartridge.mirrorType = MIRROR_VERTICAL;
            break;
        }

        case 3:
        {
            nes->cartridge.mirrorType = MIRROR_HORIZONTAL;
            break;
        }
    }
}

inline void WriteChrBank0(NES *nes, Mapper1Data *data, u8 value)
{
    if (nes->cartridge.chrBanks == 0)
        return;

    u8* chr = nes->cartridge.chr;
    s32 chrBank0 = value;

    switch (data->chrMode)
    {
        case 0:
        {
            CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr + chrBank0 * 0x2000, 0x2000);
            break;
        }

        case 1:
        {
            CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr + chrBank0 * 0x1000, 0x1000);
            break;
        }
    }
}

inline void WriteChrBank1(NES *nes, Mapper1Data *data, u8 value)
{
    if (nes->cartridge.chrBanks == 0)
        return;

    u8* chr = nes->cartridge.chr;
    s32 chrBank1 = value;
    
    switch (data->chrMode)
    {
        case 1:
        {
            CopyMemoryBytes(&nes->ppuMemory, 0x1000, chr + chrBank1 * 0x1000, 0x1000);
            break;
        }
    }
}

inline void WritePrgBank(NES *nes, Mapper1Data *data, u8 value)
{
    u8* prg = nes->cartridge.prg;
    s32 prgBank = value;

    switch (data->prgMode)
    {
        case 0:
        case 1:
        {
            CopyMemoryBytes(&nes->cpuMemory, 0x8000, prg + prgBank * 0x8000, 0x8000);
            break;
        }

        case 2:
        {
            CopyMemoryBytes(&nes->cpuMemory, 0x8000, prg, 0x4000);
            CopyMemoryBytes(&nes->cpuMemory, 0xC000, prg + prgBank * 0x4000, 0x4000);
            break;
        }

        case 3:
        {
            s32 prgBanks = nes->cartridge.prgBanks;
            CopyMemoryBytes(&nes->cpuMemory, 0x8000, prg + prgBank * 0x4000, 0x4000);
            CopyMemoryBytes(&nes->cpuMemory, 0xC000, prg + (prgBanks - 1) * 0x4000, 0x4000);
            break;
        }

    }
}

inline void WriteRegister(NES* nes, Mapper1Data *data, u16 address, u8 value)
{
    if (address <= 0x9FFF)
    {
        WriteControl(nes, data, value);
    }
    else if (address <= 0xBFFF)
    {
        WriteChrBank0(nes, data, value);
    }
    else if (address <= 0xDFFF)
    {
        WriteChrBank1(nes, data, value);
    }
    else if (address <= 0xFFFF)
    {
        WritePrgBank(nes, data, value);
    }
}

inline void LoadRegister(NES *nes, Mapper1Data *data, u16 address, u8 value)
{
    if (HAS_FLAG(value, 0x80))
    {
        data->shift = 0x10;
        WriteControl(nes, data, data->control | 0x0C);
    }
    else
    {
        b32 complete = HAS_FLAG(data->shift, 1);
        data->shift >>= 1;
        data->shift |= (value & 1) << 4;
        if (complete)
        {
            WriteRegister(nes, data, address, data->shift);
            data->shift = 0x10;
        }
    }
}

inline void Mapper1Init(NES *nes)
{
    u32 prgBanks = nes->cartridge.prgBanks;
    u32 chrBanks = nes->cartridge.chrBanks;

    if (prgBanks > 0)
    {
        s32 prgBank0 = 0;
        s32 prgBank1 = prgBanks - 1;

        u8 *pgr = nes->cartridge.prg;
        CopyMemoryBytes(&nes->cpuMemory, 0x8000, pgr + prgBank0 * 0x4000, 0x4000);
        CopyMemoryBytes(&nes->cpuMemory, 0xC000, pgr + prgBank1 * 0x4000, 0x4000);
    }

    if (chrBanks > 0)
    {
        u8* chr = nes->cartridge.chr;
        CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr, 0x2000);
    }

    Mapper1Data *data = (Mapper1Data*)Allocate(sizeof(Mapper1Data));
    data->control = 0x1C;
    data->shift = 0x10;

    nes->mapperData = (void*)data;
}

inline u8 Mapper1ReadU8(NES *nes, u16 address)
{
    Mapper1Data* data = (Mapper1Data*)nes->mapperData;

    if (ISBETWEEN(address, 0x0000, 0x2000))
    {
        return ReadU8(&nes->ppuMemory, address);

        /*u8* chr = nes->cartridge.chr;
        s32 bank = address / 0x1000;
        s32 offset = address % 0x1000;
        return chr[data->chrOffsets[bank] + offset];*/
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        return ReadU8(&nes->cpuMemory, address);
        
        /*u8* prg = nes->cartridge.prg;

        address = address - 0x8000;
        s32 bank = address / 0x4000;
        s32 offset = address % 0x4000;
        return prg[data->prgOffsets[bank] + offset];*/
    }

    ASSERT(FALSE);
    return 0;
}

inline void Mapper1WriteU8(NES *nes, u16 address, u8 value)
{
    Mapper1Data* data = (Mapper1Data*)nes->mapperData;

    if (ISBETWEEN(address, 0x0000, 0x2000))
    {
        return WriteU8(&nes->ppuMemory, address, value);
/*
        u8* chr = nes->cartridge.chr;

        s32 bank = address / 0x1000;
        s32 offset = address % 0x1000;
        chr[data->chrOffsets[bank] + offset] = value;*/
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        LoadRegister(nes, data, address, value);
        return;
    }

    ASSERT(FALSE);
}