#include "cpu.h"

u8 ReadCPUU8(NES *nes, u16 address)
{
    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        // Memory locations $0000-$07FF are mirrored three times at $0800-$1FFF.
        // This means that, for example, any data written to $0000 will also be written to $0800, $1000 and $1800.
        address = (address % 0x800);
        return ReadU8(&nes->cpuMemory, address);
    }

    if (ISBETWEEN(address, 0x2000, 0x4000))
    {
        // Memory locations $2000-$2008 are mirrored each 8 bytes.
        // This means that, for example, any data written to $2000 will also be written to $2008, $2010 and so on...
        address = 0x2000 + ((address - 0x2000) % 0x08);

        switch (address)
        {
            case 0x2000:
            {
                return ReadPPUCtrl(nes);
            }

            case 0x2001:
            {
                return ReadPPUMask(nes);
            }

            case 0x2002:
            {
                return ReadPPUStatus(nes);
            }

            case 0x2003:
            {
                return ReadPPUOamAddr(nes);
            }

            case 0x2004:
            {
                return ReadPPUOamData(nes);
            }

            case 0x2005:
            {
                return ReadPPUScroll(nes);
            }

            case 0x2006:
            {
                return ReadPPUVramAddr(nes);
            }

            case 0x2007:
            {
                return ReadPPUVramData(nes);
            }
        }
    }

    if (ISBETWEEN(address, 0x4000, 0x4020))
    {
        switch (address)
        {
            case 0x4014:
            {
                return 0;
            }

            case 0x4015:
            {
                return ReadAPUStatus(nes);
            }

            case 0x4016:
            {
                return ReadControllerU8(nes, 0);
            }

            case 0x4017:
            {
                return ReadControllerU8(nes, 1);
            }
        }
    }

    if (ISBETWEEN(address, 0x6000, 0x8000))
    {
        return ReadU8(&nes->cpuMemory, address);
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        return nes->mapperReadU8(nes, address);
    }

    // it should no get here
    return 0;
}

void WriteCPUU8(NES *nes, u16 address, u8 value)
{
    if (ISBETWEEN(address, 0x00, 0x2000))
    {
        // Memory locations $0000-$07FF are mirrored three times at $0800-$1FFF.
        // This means that, for example, any data written to $0000 will also be written to $0800, $1000 and $1800.
        address = (address % 0x800);
        WriteU8(&nes->cpuMemory, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x2000, 0x4000))
    {
        // Memory locations $2000-$2008 are mirrored each 8 bytes.
        // This means that, for example, any data written to $2000 will also be written to $2008, $2010 and so on...
        address = 0x2000 + ((address - 0x2000) % 0x08);

        switch (address)
        {
            case 0x2000:
            {
                WritePPUCtrl(nes, value);
                return;
            }

            case 0x2001:
            {
                WritePPUMask(nes, value);
                return;
            }

            case 0x2002:
            {
                WritePPUStatus(nes, value);
                return;
            }

            case 0x2003:
            {
                WritePPUOamAddr(nes, value);
                return;
            }

            case 0x2004:
            {
                WritePPUOamData(nes, value);
                return;
            }

            case 0x2005:
            {
                WritePPUScroll(nes, value);
                return;
            }

            case 0x2006:
            {
                WritePPUVramAddr(nes, value);
                return;
            }

            case 0x2007:
            {
                WritePPUVramData(nes, value);
                return;
            }
        }
    }

    if (ISBETWEEN(address, 0x4000, 0x4020))
    {
        switch (address)
        {
            case 0x4000:
            {
                WriteAPUPulseEnvelope(&nes->apu.pulse1, value);
                break;
            }

            case 0x4001:
            {
                WriteAPUPulseSweep(&nes->apu.pulse1, value);
                break;
            }

            case 0x4002:
            {
                WriteAPUPulseTimer(&nes->apu.pulse1, value);
                break;
            }

            case 0x4003:
            {
                WriteAPUPulseLength(&nes->apu.pulse1, value);
                break;
            }

            case 0x4004:
            {
                WriteAPUPulseEnvelope(&nes->apu.pulse2, value);
                break;
            }

            case 0x4005:
            {
                WriteAPUPulseSweep(&nes->apu.pulse2, value);
                break;
            }

            case 0x4006:
            {
                WriteAPUPulseTimer(&nes->apu.pulse2, value);
                break;
            }

            case 0x4007:
            {
                WriteAPUPulseLength(&nes->apu.pulse2, value);
                break;
            }

            case 0x4008:
            {
               WriteAPUTriangleLinear(&nes->apu.triangle, value);
               break;
            }

            case 0x4009:
            {
               // unused
               break;
            }

            case 0x400A:
            {
               WriteAPUTriangleTimer(&nes->apu.triangle, value);
               break;
            }

            case 0x400B:
            {
               WriteAPUTriangleLength(&nes->apu.triangle, value);
               break;
            }

            case 0x400C:
            {
               WriteAPUNoiseEnvelope(&nes->apu.noise, value);
               break;
            }

            case 0x400D:
            {
               // unused
               break;
            }

            case 0x400E:
            {
               WriteAPUNoisePeriod(&nes->apu.noise, value);
               break;
            }

            case 0x400F:
            {
               WriteAPUNoiseLength(&nes->apu.noise, value);
               break;
            }

            case 0x4010:
            {
               WriteAPUDMCControl(&nes->apu.dmc, value);
               break;
            }

            case 0x4011:
            {
               WriteAPUDMCValue(&nes->apu.dmc, value);
               break;
            }

            case 0x4012:
            {
               WriteAPUDMCAddress(&nes->apu.dmc, value);
               break;
            }

            case 0x4013:
            {
               WriteAPUDMCLength(&nes->apu.dmc, value);
               break;
            }

            case 0x4014:
            {
                WritePPUDMA(nes, value);
                break;
            }

            case 0x4015:
            {
                WriteAPUStatus(nes, value);
                break;
            }

            case 0x4016:
            {
                WriteControllerU8(nes, 0, value);
                WriteControllerU8(nes, 1, value);
                break;
            }

            case 0x4017:
            {
                WriteAPUFrameCounter(nes, value);
                break;
            }
        }

        return;
    }

    if (ISBETWEEN(address, 0x6000, 0x8000))
    {
        WriteU8(&nes->cpuMemory, address, value);
        return;
    }

    if (ISBETWEEN(address, 0x8000, 0x10000))
    {
        nes->mapperWriteU8(nes, address, value);
        return;
    }
}

