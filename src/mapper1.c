#include "mapper1.h"
#include "memory.h"

#include <string.h>

typedef struct Mapper1Data Mapper1Data;
struct Mapper1Data
{
    u8 control;
    u8 shift;
    u8 prgMode;
    u8 chrMode;
};

static void WriteControl(NES *nes, Mapper1Data *data, u8 value)
{
    data->control = value;
    data->chrMode = (value >> 4) & 1;
    data->prgMode = (value >> 2) & 3;

    switch (value & 3)
    {
        case 0:
        case 1:
            nes->cartridge.mirrorType = MIRROR_FOUR;
            break;
        case 2:
            nes->cartridge.mirrorType = MIRROR_VERTICAL;
            break;
        case 3:
            nes->cartridge.mirrorType = MIRROR_HORIZONTAL;
            break;
    }
}

static void WriteChrBank0(NES *nes, Mapper1Data *data, u8 value)
{
    u8 *chr;
    s32 chrBank0;

    if (nes->cartridge.chrBanks == 0)
        return;

    chr = nes->cartridge.chr;
    chrBank0 = value;

    switch (data->chrMode)
    {
        case 0:
            CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr + chrBank0 * 0x2000, 0x2000);
            break;
        case 1:
            CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr + chrBank0 * 0x1000, 0x1000);
            break;
    }
}

static void WriteChrBank1(NES *nes, Mapper1Data *data, u8 value)
{
    u8 *chr;
    s32 chrBank1;

    if (nes->cartridge.chrBanks == 0)
        return;

    chr = nes->cartridge.chr;
    chrBank1 = value;

    if (data->chrMode == 1)
    {
        CopyMemoryBytes(&nes->ppuMemory, 0x1000, chr + chrBank1 * 0x1000, 0x1000);
    }
}

static void WritePrgBank(NES *nes, Mapper1Data *data, u8 value)
{
    u8 *prg = nes->cartridge.prg;
    s32 prgBank = value;

    switch (data->prgMode)
    {
        case 0:
        case 1:
            CopyMemoryBytes(&nes->cpuMemory, 0x8000, prg + prgBank * 0x8000, 0x8000);
            break;
        case 2:
            CopyMemoryBytes(&nes->cpuMemory, 0x8000, prg, 0x4000);
            CopyMemoryBytes(&nes->cpuMemory, 0xC000, prg + prgBank * 0x4000, 0x4000);
            break;
        case 3:
            CopyMemoryBytes(&nes->cpuMemory, 0x8000, prg + prgBank * 0x4000, 0x4000);
            CopyMemoryBytes(&nes->cpuMemory, 0xC000, prg + (nes->cartridge.prgBanks - 1) * 0x4000, 0x4000);
            break;
    }
}

static void WriteRegister(NES *nes, Mapper1Data *data, u16 address, u8 value)
{
    if (address <= 0x9FFF) WriteControl(nes, data, value);
    else if (address <= 0xBFFF) WriteChrBank0(nes, data, value);
    else if (address <= 0xDFFF) WriteChrBank1(nes, data, value);
    else WritePrgBank(nes, data, value);
}

static void LoadRegister(NES *nes, Mapper1Data *data, u16 address, u8 value)
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

void Mapper1Init(NES *nes)
{
    u32 prgBanks = nes->cartridge.prgBanks;
    u32 chrBanks = nes->cartridge.chrBanks;
    Mapper1Data *data;

    if (prgBanks > 0)
    {
        u8 *prg = nes->cartridge.prg;
        CopyMemoryBytes(&nes->cpuMemory, 0x8000, prg, 0x4000);
        CopyMemoryBytes(&nes->cpuMemory, 0xC000, prg + (prgBanks - 1) * 0x4000, 0x4000);
    }

    if (chrBanks > 0)
    {
        u8 *chr = nes->cartridge.chr;
        CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr, 0x2000);
    }

    data = (Mapper1Data*)Allocate(sizeof(Mapper1Data));
    memset(data, 0, sizeof(Mapper1Data));
    data->control = 0x1C;
    data->shift = 0x10;
    nes->mapperData = data;
}

u8 Mapper1ReadU8(NES *nes, u16 address)
{
    if (ISBETWEEN(address, 0x0000, 0x2000))
    {
        return ReadU8(&nes->ppuMemory, address);
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        return ReadU8(&nes->cpuMemory, address);
    }

    ASSERT(FALSE);
    return 0;
}

void Mapper1WriteU8(NES *nes, u16 address, u8 value)
{
    Mapper1Data *data = (Mapper1Data*)nes->mapperData;

    if (ISBETWEEN(address, 0x0000, 0x2000))
    {
        WriteU8(&nes->ppuMemory, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        LoadRegister(nes, data, address, value);
        return;
    }

    ASSERT(FALSE);
}

void Mapper1Save(NES *nes, FILE *file)
{
    fwrite(nes->mapperData, sizeof(Mapper1Data), 1, file);
}

void Mapper1Load(NES *nes, FILE *file)
{
    nes->mapperData = Allocate(sizeof(Mapper1Data));
    fread(nes->mapperData, sizeof(Mapper1Data), 1, file);
}
