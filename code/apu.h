#pragma once

#include <types.h>

// CPU_FREQ / 240 = 1789773 = 7457.38
// Frequency of FrameCounter
// from: http://wiki.nesdev.com/w/index.php/APU#Frame_Counter_.28.244017.29
#define FRAME_COUNTER_RATE 7457.38f

global u8 lengthTable[32]
{
    10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

global u8 dutyTable[4][8]
{
    { 0, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 1, 1, 0, 0, 0 },
    { 1, 0, 0, 1, 1, 1, 1, 1 }
};

global f32 pulseTable[31];
global f32 tndTable[203];

inline u8 ReadAPUStatus(NES *nes)
{
    APU *apu = &nes->apu;

    u8 result = 0;

    if (apu->pulse1.enabled && apu->pulse1.lengthValue)
    {
        result |= 1;
    }

    if (apu->pulse2.enabled && apu->pulse2.lengthValue)
    {
        result |= 2;
    }

    /*if (apu->triangle.lengthValue)
    {
        result |= 4;
    }

    if (apu->noise.lengthValue)
    {
        result |= 8;
    }

    if (apu->dmc.lengthValue)
    {
        result |= 16;
    }*/

    return result;
}

inline void WriteAPUStatus(NES *nes, u8 value)
{
    APU *apu = &nes->apu;

    apu->pulse1.enabled = (value & 1);
    if (!apu->pulse1.enabled)
    {
        apu->pulse1.lengthValue = 0;
    }

    apu->pulse2.enabled = (value & 2) >> 1;
    if (!apu->pulse2.enabled)
    {
        apu->pulse2.lengthValue = 0;
    }

    /*apu->triangle.enabled = (value & 4) >> 1;
    if (!apu->triangle.enabled)
    {
        apu->triangle.lengthValue = 0;
    }

    apu->noise.enabled = (value & 8) >> 1;
    if (!apu->noise.enabled)
    {
        apu->noise.lengthValue = 0;
    }

    apu->dmc.enabled = (value & 16) >> 1;
    if (!apu->dmc.enabled)
    {
        apu->dmc.lengthValue = 0;
    }
    else if (!apu->dmc.lengthValue)
    {
        RestartDmc(&apu->dmc);
    }*/
}

// Pulse 1
inline void WriteAPUPulseEnvelope(NES *nes, APU::Pulse *pulse, u8 value)
{
    pulse->dutyMode = (value >> 6) & 3;
    pulse->lengthEnabled = !((value >> 5) & 1);
    pulse->constantVolume = value & 15;
    pulse->envelopeLoop = (value >> 5) & 1;
    pulse->envelopeEnabled = !((value >> 4) & 1);
    pulse->envelopePeriod = value & 15;
    pulse->envelopeStart = TRUE;
}

inline void WriteAPUPulseSweep(NES *nes, APU::Pulse *pulse, u8 value)
{
    pulse->sweepEnabled = (value >> 7) & 1;
    pulse->sweepPeriod = ((value >> 4) & 7) + 1;
    pulse->sweepNegate = (value >> 3) & 1;
    pulse->sweepShift = value & 7;
    pulse->sweepReload = TRUE;
}

inline void WriteAPUPulseTimer(NES *nes, APU::Pulse *pulse, u8 value)
{
    pulse->timerPeriod = (pulse->timerPeriod & 0xFF00) | (u16)value;
}

inline void WriteAPUPulseLength(NES *nes, APU::Pulse *pulse, u8 value)
{
    pulse->lengthValue = lengthTable[value >> 3];
    pulse->timerPeriod = (pulse->timerPeriod & 0x00FF) | ((u16)(value & 7) << 8);
    pulse->envelopeStart = TRUE;
    pulse->dutyValue = 0;
}

void ResetAPU(NES *nes);
void PowerAPU(NES *nes);
void InitAPU(NES *nes);
void StepAPU(NES *nes);
void WriteAPUFrameCounter(NES *nes, u8 value);