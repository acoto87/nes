#pragma once

inline void Mapper3Init(NES *nes)
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
}

inline u8 Mapper3ReadU8(NES *nes, u16 address)
{
    if (ISBETWEEN(address, 0x00, 0x2000))
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

inline void Mapper3WriteU8(NES *nes, u16 address, u8 value)
{
    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        WriteU8(&nes->ppuMemory, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        u8* chr = nes->cartridge.chr;
        s32 chrBank = (value & 0x03);
        CopyMemoryBytes(&nes->ppuMemory, 0x0000, chr + chrBank * 0x2000, 0x2000);
        return;
    }
    
    ASSERT(FALSE);
}