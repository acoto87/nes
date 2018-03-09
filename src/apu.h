#pragma once

// (CPU_FREQ / 240) = (1789773 / 240) = 7457.38
// Frequency of FrameCounter
// from: http://wiki.nesdev.com/w/index.php/APU#Frame_Counter_.28.244017.29
#define FRAME_COUNTER_RATE 7457.38f

#define APU_SAMPLES_PER_SECOND 48000

// (CPU_FREQ / 48000) = (1789773 / 48000) = 37.28
#define APU_CYCLES_PER_SAMPLE 37

#define APU_BYTES_PER_SAMPLE sizeof(s16)
#define APU_AMPLIFIER_VALUE 10000

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

global u8 triangleTable[32]
{
    15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0, 
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

global u16 noiseTable[16]
{
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
};

global f32 pulseTable[31];
global f32 tndTable[203];

#pragma region Pulse

inline void WriteAPUPulseEnvelope(APU::Pulse *pulse, u8 value)
{
    pulse->dutyMode = (value >> 6) & 3;
    pulse->lengthEnabled = !((value >> 5) & 1);
    pulse->constantVolume = value & 15;
    pulse->envelopeLoop = (value >> 5) & 1;
    pulse->envelopeEnabled = !((value >> 4) & 1);
    pulse->envelopePeriod = value & 15;
    pulse->envelopeStart = TRUE;
}

inline void WriteAPUPulseSweep(APU::Pulse *pulse, u8 value)
{
    pulse->sweepEnabled = (value >> 7) & 1;
    pulse->sweepPeriod = ((value >> 4) & 7) + 1;
    pulse->sweepNegate = (value >> 3) & 1;
    pulse->sweepShift = value & 7;
    pulse->sweepReload = TRUE;
}

inline void WriteAPUPulseTimer(APU::Pulse *pulse, u8 value)
{
    pulse->timerPeriod = (pulse->timerPeriod & 0xFF00) | (u16)value;
}

inline void WriteAPUPulseLength(APU::Pulse *pulse, u8 value)
{
    if (pulse->enabled)
    {
        pulse->lengthValue = lengthTable[value >> 3];
        pulse->timerPeriod = (pulse->timerPeriod & 0x00FF) | ((u16)(value & 7) << 8);
        pulse->envelopeStart = TRUE;
        pulse->dutyValue = 0;
    }
}

#pragma endregion

#pragma region triangle

inline void WriteAPUTriangleLinear(APU::Triangle *triangle, u8 value)
{
    triangle->linearEnabled = !((value >> 7) & 1);
    triangle->lengthEnabled = !((value >> 7) & 1);
    triangle->linearPeriod = value & 0x7F;
}

inline void WriteAPUTriangleTimer(APU::Triangle *triangle, u8 value)
{
    triangle->timerPeriod = (triangle->timerPeriod & 0xFF00) | (u16)value;
}

inline void WriteAPUTriangleLength(APU::Triangle *triangle, u8 value)
{
    if (triangle->enabled)
    {
        triangle->lengthValue = lengthTable[value >> 3];
        triangle->timerPeriod = (triangle->timerPeriod & 0x00FF) | ((u16)(value & 7) << 8);
        triangle->timerValue = triangle->timerPeriod;
        triangle->linearReload = TRUE;
    }
}

#pragma endregion

#pragma region Noise

inline void WriteAPUNoiseEnvelope(APU::Noise *noise, u8 value)
{
    noise->lengthEnabled = !((value >> 5) & 1);
    noise->constantVolume = value & 15;
    noise->envelopeLoop = (value >> 5) & 1;
    noise->envelopeEnabled = !((value >> 4) & 1);
    noise->envelopePeriod = value & 15;
    noise->envelopeStart = TRUE;
}

inline void WriteAPUNoisePeriod(APU::Noise *noise, u8 value)
{
    noise->timerMode = (value >> 7) & 1;
    noise->timerPeriod = noiseTable[value & 0x0F];
}

inline void WriteAPUNoiseLength(APU::Noise *noise, u8 value)
{
    noise->lengthValue = lengthTable[value >> 3];
    noise->envelopeStart = TRUE;    
}

#pragma endregion

inline u8 ReadAPUStatus(NES *nes)
{
    APU *apu = &nes->apu;

    u8 result = 0;

    if (apu->pulse1.lengthValue > 0)
    {
        result |= 1;
    }

    if (apu->pulse2.lengthValue > 0)
    {
        result |= 2;
    }

    if (apu->triangle.lengthValue)
    {
        result |= 4;
    }

    if (apu->noise.lengthValue > 0)
    {
        result |= 8;
    }

    /*if (apu->dmc.lengthValue)
    {
    result |= 16;
    }*/

    if (apu->frameIRQ)
    {
        result |= 0x40;
    }

    if (apu->dmcIRQ)
    {
        result |= 0x80;
    }

    apu->frameIRQ = FALSE;

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

    apu->triangle.enabled = (value & 4) >> 1;
    if (!apu->triangle.enabled)
    {
        apu->triangle.lengthValue = 0;
    }

    apu->noise.enabled = (value & 8) >> 1;
    if (!apu->noise.enabled)
    {
        apu->noise.lengthValue = 0;
    }

    /*
    apu->dmc.enabled = (value & 16) >> 1;
    if (!apu->dmc.enabled)
    {
    apu->dmc.lengthValue = 0;
    }
    else if (!apu->dmc.lengthValue)
    {
    RestartDmc(&apu->dmc);
    }*/

    apu->dmcIRQ = FALSE;
}

void ResetAPU(NES *nes);
void PowerAPU(NES *nes);
void InitAPU(NES *nes);
void StepAPU(NES *nes);
void WriteAPUFrameCounter(NES *nes, u8 value);