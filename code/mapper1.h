#pragma once

#include "types.h"
#include "memory.h"

struct Mapper1Data
{
    u8 shiftRegister;
    u8 control;
    u8 pgrMode;
    u8 chrMode;
    u8 pgrBank;
    u8 chrBank0;
    u8 chrBank1;
    s32 pgrOffsets[2];
    s32 chrOffsets[2];
    s32 pgrBanks;
    s32 chrBanks;
};

inline s32 PgrBankOffset(Mapper1Data* data, s32 index)
{
    if (index >= 0x80)
    {
        index -= 0x100;
    }
    index %= data->pgrBanks;
    s32 offset = index * 0x4000;
    if (offset < 0) {
        offset += data->pgrBanks * 0x4000;
    }
    return offset;
}

inline s32 ChrBankOffset(Mapper1Data* data, s32 index)
{
    if (data->chrBanks == 0)
        return 0;

    if (index >= 0x80)
    {
        index -= 0x100;
    }
    index %= data->chrBanks;
    s32 offset = index * 0x1000;
    if (offset < 0) {
        offset += data->chrBanks * 0x1000;
    }
    return offset;
}

inline void UpdateOffsets(Mapper1Data *data)
{
    switch (data->pgrMode)
    {
        case 0:
        case 1:
        {
            data->pgrOffsets[0] = PgrBankOffset(data, data->pgrBank & 0xFE);
            data->pgrOffsets[1] = PgrBankOffset(data, data->pgrBank & 0x01);
            break;
        }

        case 2:
        {
            data->pgrOffsets[0] = 0;
            data->pgrOffsets[1] = PgrBankOffset(data, data->pgrBank);
            break;
        }

        case 3:
        {
            data->pgrOffsets[0] = PgrBankOffset(data, data->pgrBank);
            data->pgrOffsets[1] = PgrBankOffset(data, -1);
            break;
        }

    }

    switch (data->chrMode)
    {
        case 0:
        {
            data->chrOffsets[0] = ChrBankOffset(data, data->chrBank0 & 0xFE);
            data->chrOffsets[1] = ChrBankOffset(data, data->chrBank0 | 0x01);
            break;
        }

        case 1:
        {
            data->chrOffsets[0] = ChrBankOffset(data, data->chrBank0);
            data->chrOffsets[1] = ChrBankOffset(data, data->chrBank1);
            break;
        }

    }
}

inline void WriteControl(NES *nes, Mapper1Data *data, u8 value)
{
    data->control = value;
    data->chrMode = (value >> 4) & 1;
    data->pgrMode = (value >> 2) & 3;
    s32 mirror = value & 3;
    switch (mirror)
    {
        case 0:
        {
            // @FIX: SUPPORT HERE MIRRORING SINGLE0

            nes->cartridge.mirrorType = MIRROR_HORIZONTAL;
            break;
        }

        case 1:
        {
            // @FIX: SUPPORT HERE MIRRORING SINGLE1

            nes->cartridge.mirrorType = MIRROR_HORIZONTAL;
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

    UpdateOffsets(data);
}

inline void WriteChrBank0(Mapper1Data *data, u8 value)
{
    data->chrBank0 = value;
    UpdateOffsets(data);
}

inline void WriteChrBank1(Mapper1Data *data, u8 value)
{
    data->chrBank1 = value;
    UpdateOffsets(data);
}

inline void WritePgrBank(Mapper1Data *data, u8 value)
{
    data->pgrBank = value;
    UpdateOffsets(data);
}

inline void WriteRegister(NES* nes, Mapper1Data *data, u16 address, u8 value)
{
    if (address <= 0x9FFF)
    {
        WriteControl(nes, data, value);
    }
    else if (address <= 0xBFFF)
    {
        WriteChrBank0(data, value);
    }
    else if (address <= 0xDFFF)
    {
        WriteChrBank1(data, value);
    }
    else if (address <= 0xFFFF)
    {
        WritePgrBank(data, value);
    }
}

inline void LoadRegister(NES *nes, Mapper1Data *data, u16 address, u8 value)
{
    if (value & 0x80 == 0x80)
    {
        data->shiftRegister = 0x10;
        WriteControl(nes, data, data->control | 0x0C);
    }
    else
    {
        b32 complete = HAS_FLAG(data->shiftRegister, 1);
        data->shiftRegister >>= 1;
        data->shiftRegister |= (value & 1) << 4;
        if (complete)
        {
            WriteRegister(nes, data, address, data->shiftRegister);
            data->shiftRegister = 0x10;
        }
    }
}

inline void Mapper1Init(NES *nes)
{
    Mapper1Data *data = (Mapper1Data*)Allocate(sizeof(Mapper1Data));
    data->pgrBanks = nes->cartridge.pgrBanks;
    data->chrBanks = nes->cartridge.chrBanks;
    data->shiftRegister = 1;
    data->pgrOffsets[1] = PgrBankOffset(data, -1);

    nes->mapperData = (void*)data;
}

inline u8 Mapper1ReadU8(NES *nes, u16 address)
{
    Mapper1Data* data = (Mapper1Data*)nes->mapperData;

    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        u8* chr = nes->cartridge.chr;
        s32 bank = address / 0x1000;
        s32 offset = address % 0x1000;
        return chr[data->chrOffsets[bank] + offset];
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        u8* pgr = nes->cartridge.pgr;

        address = address - 0x8000;
        s32 bank = address / 0x4000;
        s32 offset = address % 0x4000;
        return pgr[data->pgrOffsets[bank] + offset];
    }

    ASSERT(FALSE);
    return 0;
}

inline void Mapper1WriteU8(NES *nes, u16 address, u8 value)
{
    Mapper1Data* data = (Mapper1Data*)nes->mapperData;

    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        u8* chr = nes->cartridge.chr;

        s32 bank = address / 0x1000;
        s32 offset = address % 0x1000;
        chr[data->chrOffsets[bank] + offset] = value;
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        LoadRegister(nes, data, address, value);
        return;
    }

    ASSERT(FALSE);
}