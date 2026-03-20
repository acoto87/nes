#ifndef APU_H
#define APU_H

#include "types.h"

void CPUSetIRQSource(NES* nes, u32 sourceMask, bool asserted);

u8 ReadCPUU8(NES* nes, u16 address);

// (CPU_FREQ / 240) = (1789773 / 240) = 7457.38
// Frequency of FrameCounter
// from: http://wiki.nesdev.com/w/index.php/APU#Frame_Counter_.28.244017.29
#define FRAME_COUNTER_RATE 7457

// The NES CPU runs at 1.789773 MHz (NTSC).
// Defined here (rather than cpu.h) so that apu.c can reference it for
// audio timing without creating a circular include with cpu.h.
// cpu.h re-exports the same symbol — always use CPU_FREQ, not the literal.
// see https://wiki.nesdev.com/w/index.php/Clock_rate
#define CPU_FREQ 1789773

#define APU_SAMPLES_PER_SECOND 48000

// Audio sample timing — fractional accumulator technique
//
// The naive approach is to emit a sample every N CPU cycles where
//   N = CPU_FREQ / SAMPLE_RATE = 1789773 / 48000 = 37.2869...
// Truncating to 37 means samples are generated at 1789773/37 = 48372 Hz
// instead of 48000 Hz — 372 extra samples per second that pile up as delay.
//
// The fix: use the accumulator pattern.  Each CPU cycle we add SAMPLE_RATE to
// a running counter.  When the counter reaches CPU_FREQ we emit one sample and
// subtract CPU_FREQ (keeping the fractional remainder).  The ratio is:
//   counter += 48000  every CPU cycle
//   emit + counter -= 1789773  when counter >= 1789773
// This produces exactly 48000 samples per 1789773 CPU cycles with no rounding
// error and no floating-point arithmetic in the hot path.
//
// APU_CYCLES_PER_SAMPLE (the truncated integer) is no longer used.

#define APU_BYTES_PER_SAMPLE sizeof(s16)
#define APU_AMPLIFIER_VALUE 10000

extern u8 lengthTable[32];
extern u8 dutyTable[4][8];
extern u8 triangleTable[32];
extern u16 noiseTable[16];
extern u16 dmcTable[16];
extern f32 pulseTable[31];
extern f32 tndTable[203];

/*************************************
            PULSE CHANNEL
**************************************/

static inline void WriteAPUPulseEnvelope(APUPulse* pulse, u8 value)
{
    pulse->dutyMode = (value >> 6) & 3;
    pulse->lengthEnabled = !((value >> 5) & 1);
    pulse->constantVolume = value & 15;
    pulse->envelopeLoop = (value >> 5) & 1;
    pulse->envelopeEnabled = !((value >> 4) & 1);
    pulse->envelopePeriod = value & 15;
    pulse->envelopeStart = true;
}

static inline void WriteAPUPulseSweep(APUPulse* pulse, u8 value)
{
    pulse->sweepEnabled = (value >> 7) & 1;
    pulse->sweepPeriod = ((value >> 4) & 7) + 1;
    pulse->sweepNegate = (value >> 3) & 1;
    pulse->sweepShift = value & 7;
    pulse->sweepReload = true;
}

static inline void WriteAPUPulseTimer(APUPulse* pulse, u8 value)
{
    pulse->timerPeriod = (pulse->timerPeriod & 0xFF00) | (u16)value;
}

static inline void WriteAPUPulseLength(APUPulse* pulse, u8 value)
{
    if (pulse->enabled) {
        pulse->lengthValue = lengthTable[value >> 3];
        pulse->timerPeriod = (pulse->timerPeriod & 0x00FF) | ((u16)(value & 7) << 8);
        pulse->envelopeStart = true;
        pulse->dutyValue = 0;
    }
}

/*************************************
            TRIANGLE CHANNEL
**************************************/

static inline void WriteAPUTriangleLinear(APUTriangle* triangle, u8 value)
{
    triangle->linearEnabled = !((value >> 7) & 1);
    triangle->lengthEnabled = !((value >> 7) & 1);
    triangle->linearPeriod = value & 0x7F;
}

static inline void WriteAPUTriangleTimer(APUTriangle* triangle, u8 value)
{
    triangle->timerPeriod = (triangle->timerPeriod & 0xFF00) | (u16)value;
}

static inline void WriteAPUTriangleLength(APUTriangle* triangle, u8 value)
{
    if (triangle->enabled) {
        triangle->lengthValue = lengthTable[value >> 3];
        triangle->timerPeriod = (triangle->timerPeriod & 0x00FF) | ((u16)(value & 7) << 8);
        triangle->timerValue = triangle->timerPeriod;
        triangle->linearReload = true;
    }
}

/*************************************
            NOISE CHANNEL
**************************************/

static inline void WriteAPUNoiseEnvelope(APUNoise* noise, u8 value)
{
    noise->lengthEnabled = !((value >> 5) & 1);
    noise->constantVolume = value & 15;
    noise->envelopeLoop = (value >> 5) & 1;
    noise->envelopeEnabled = !((value >> 4) & 1);
    noise->envelopePeriod = value & 15;
    noise->envelopeStart = true;
}

static inline void WriteAPUNoisePeriod(APUNoise* noise, u8 value)
{
    noise->timerMode = (value >> 7) & 1;
    noise->timerPeriod = noiseTable[value & 0x0F];
}

static inline void WriteAPUNoiseLength(APUNoise* noise, u8 value)
{
    if (noise->enabled) {
        noise->lengthValue = lengthTable[value >> 3];
        noise->envelopeStart = true;
    }
}

/*************************************
            DMC CHANNEL
**************************************/

static inline void WriteAPUDMCControl(APUDMC* dmc, u8 value)
{
    dmc->irq = (value & 0x80) >> 7;
    dmc->loop = (value & 0x40) >> 6;
    dmc->timerPeriod = dmcTable[value & 0x0F];
}

static inline void WriteAPUDMCValue(APUDMC* dmc, u8 value)
{
    dmc->value = (value & 0x7F);
}

static inline void WriteAPUDMCAddress(APUDMC* dmc, u8 value)
{
    // Sample address = %11AAAAAA.AA000000
    dmc->sampleAddress = 0xC000 + (u16)value * 64;
}

static inline void WriteAPUDMCLength(APUDMC* dmc, u8 value)
{
    // Sample length = %0000LLLL.LLLL0001
    dmc->sampleLength = (u16)value * 16 + 1;
}

static inline void RestartDMC(APUDMC* dmc)
{
    dmc->currentAddress = dmc->sampleAddress;
    dmc->currentLength = dmc->sampleLength;
}

/*******************************************************
            STATUS AND FRAMECOUNTER CHANNEL
********************************************************/

static inline u8 ReadAPUStatus(NES* nes)
{
    APU* apu = &nes->apu;

    u8 result = 0;

    if (apu->pulse1.lengthValue > 0) {
        result |= 1;
    }

    if (apu->pulse2.lengthValue > 0) {
        result |= 2;
    }

    if (apu->triangle.lengthValue) {
        result |= 4;
    }

    if (apu->noise.lengthValue > 0) {
        result |= 8;
    }

    if (apu->dmc.value > 0) {
        result |= 16;
    }

    if (apu->frameIRQ) {
        result |= 0x40;
    }

    if (apu->dmcIRQ) {
        result |= 0x80;
    }

    apu->frameIRQ = false;
    CPUSetIRQSource(nes, CPU_IRQSRC_APU_FRAME, false);

    return result;
}

static inline void WriteAPUStatus(NES* nes, u8 value)
{
    APU* apu = &nes->apu;

    apu->pulse1.enabled = (value & 1);
    if (!apu->pulse1.enabled) {
        apu->pulse1.lengthValue = 0;
    }

    apu->pulse2.enabled = (value & 2) >> 1;
    if (!apu->pulse2.enabled) {
        apu->pulse2.lengthValue = 0;
    }

    apu->triangle.enabled = (value & 4) >> 1;
    if (!apu->triangle.enabled) {
        apu->triangle.lengthValue = 0;
    }

    apu->noise.enabled = (value & 8) >> 1;
    if (!apu->noise.enabled) {
        apu->noise.lengthValue = 0;
    }

    apu->dmc.enabled = (value & 16) >> 1;
    if (!apu->dmc.enabled) {
        apu->dmc.currentLength = 0;
    } else if (!apu->dmc.currentLength) {
        RestartDMC(&apu->dmc);
    }

    apu->dmcIRQ = false;
    CPUSetIRQSource(nes, CPU_IRQSRC_APU_DMC, false);
}

void ResetAPU(NES* nes);
void PowerAPU(NES* nes);
void InitAPU(NES* nes);
void StepAPU(NES* nes);
void WriteAPUFrameCounter(NES* nes, u8 value);

static inline void StepAPUCycles(NES* nes, s32 cycles)
{
    for (s32 i = 0; i < cycles; ++i) {
        StepAPU(nes);
    }
}

#endif // APU_H
