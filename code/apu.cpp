#include "apu.h"

void ResetAPU(NES *nes)
{
    APU *apu = &nes->apu;

    apu->pulse1.enabled = FALSE;
    apu->pulse1.lengthValue = 0;

    apu->pulse2.enabled = FALSE;
    apu->pulse2.lengthValue = 0;

    apu->pulse1.channel = 1;
    apu->pulse2.channel = 2;
    // apu->noise.shiftRegister = 1

    apu->bufferPos = 0;
    apu->sampleRate = 48000;
    memset(apu->buffer, 0, sizeof(apu->buffer));

    WriteAPUFrameCounter(nes, 0);
}

void PowerAPU(NES *nes)
{
    ResetAPU(nes);
}

void InitAPU(NES *nes)
{
    // from: http://wiki.nesdev.com/w/index.php/APU_Mixer
    //
    // pulse_table[n] = 95.52 / (8128.0 / n + 100)
    // tnd_table[n] = 163.67 / (24329.0 / n + 100)
    //
    // output = pulse_out + tnd_out
    // pulse_out = pulse_table[pulse1 + pulse2]
    // tnd_out = tnd_table[3 * triangle + 2 * noise + dmc]

    for (s32 i = 0; i < 31; ++i)
    {
        pulseTable[i] = 95.52f / (8128.0f / (f32)i + 100.0f);
    }

    for (s32 i = 0; i < 203; ++i)
    {
        tndTable[i] = 163.67f / (24329.0f / (f32)i + 100.0f);
    }
}

internal void StepPulseEvenlope(APU::Pulse *pulse)
{
    if (pulse->envelopeStart)
    {
        pulse->envelopeVolume = 15;
        pulse->envelopeValue = pulse->envelopePeriod;
        pulse->envelopeStart = FALSE;
    }
    else if (pulse->envelopeValue > 0)
    {
        pulse->envelopeValue--;
    }
    else
    {
        if (pulse->envelopeVolume > 0)
        {
            pulse->envelopeVolume--;
        }
        else if (pulse->envelopeLoop)
        {
            pulse->envelopeVolume = 15;
        }

        pulse->envelopeValue = pulse->envelopePeriod;
    }
}

internal void PulseSweep(APU::Pulse *pulse)
{
    u8 delta = pulse->timerPeriod >> pulse->sweepShift;
    if (pulse->sweepNegate)
    {
        pulse->timerPeriod -= delta;
        if (pulse->channel == 1)
        {
            pulse->timerPeriod--;
        }
    }
    else
    {
        pulse->timerPeriod += delta;
    }
}

internal void StepPulseSweep(APU::Pulse *pulse)
{
    if (pulse->sweepReload)
    {
        if (pulse->sweepEnabled && !pulse->sweepValue)
        {
            PulseSweep(pulse);
        }

        pulse->sweepValue = pulse->sweepPeriod;
        pulse->sweepReload = FALSE;
    }
    else if (pulse->sweepValue > 0)
    {
        pulse->sweepValue--;
    }
    else
    {
        if (pulse->sweepEnabled)
        {
            PulseSweep(pulse);
        }

        pulse->sweepValue = pulse->sweepPeriod;
    }
}

internal void StepPulseTimer(APU::Pulse *pulse)
{
    if (!pulse->timerValue)
    {
        pulse->timerValue = pulse->timerPeriod;
        pulse->dutyValue = (pulse->dutyValue + 1) % 8;
    }
    else
    {
        pulse->timerValue--;
    }
}

internal void StepPulseLength(APU::Pulse *pulse)
{
    if (pulse->lengthEnabled && pulse->lengthValue > 0)
    {
        pulse->lengthValue--;
    }
}

internal u8 GetPulseOutput(APU::Pulse *pulse)
{
    if (!pulse->enabled)
    {
        return 0;
    }

    if (!pulse->lengthValue)
    {
        return 0;
    }

    if (!dutyTable[pulse->dutyMode][pulse->dutyValue])
    {
        return 0;
    }

    if (pulse->timerPeriod < 8 || pulse->timerPeriod > 0x7FF)
    {
        return 0;
    }

    if (pulse->envelopeEnabled)
    {
        return pulse->envelopeVolume;
    }

    return pulse->constantVolume;
}

internal void StepAPUTimer(NES *nes)
{
    APU *apu = &nes->apu;
    if (!(apu->cycles & 1))
    {
        StepPulseTimer(&apu->pulse1);
        StepPulseTimer(&apu->pulse2);
        // StepNoiseTimer(&apu->noise);
        // StepDmcTimer(&apu->dmc);
    }
    // StepTriangleTimer(&apu->triangle);
}

internal void StepAPUEnvenlope(APU *apu)
{
    StepPulseEvenlope(&apu->pulse1);
    StepPulseEvenlope(&apu->pulse2);
    // StepTriangleEnvelope(&apu->triangle);
    // StepNoiseEnvelope(&apu->noise);
}

internal void StepAPUSweep(APU *apu)
{
    StepPulseSweep(&apu->pulse1);
    StepPulseSweep(&apu->pulse2);
}

internal void StepAPUTimer(APU *apu)
{
    if (!(apu->cycles & 1))
    {
        StepPulseTimer(&apu->pulse1);
        StepPulseTimer(&apu->pulse2);
        // StepNoiseTimer(&apu->noise);
        // StepDmcTimer(&apu->dmc);
    }

    // StepTriangleTimer(&apu->triangle);
}

internal void StepAPULength(APU *apu)
{
    StepPulseLength(&apu->pulse1);
    StepPulseLength(&apu->pulse2);
    // StepTriangleLength(&apu->triangle);
    // StepNoiseLength(&apu->noise);
}

// mode 0:    mode 1:       function
// ---------  -----------  -----------------------------
//  - - - f    - - - - -    IRQ (if bit 6 is clear)
//  - l - l    l - l - -    Length counter and sweep
//  e e e e    e e e e -    Envelope and linear counter
// from: http://wiki.nesdev.com/w/index.php/APU#Frame_Counter_.28.244017.29
internal void StepAPUFrameCounter(NES *nes)
{
    APU *apu = &nes->apu;

    switch (apu->framePeriod)
    {
        // 4-step mode
        case 0:
        {
            apu->frameValue = (apu->frameValue + 1) % 4;

            switch (apu->frameValue)
            {
                case 0:
                {
                    StepAPUEnvenlope(apu);
                    break;
                }

                case 1:
                {
                    StepAPUEnvenlope(apu);
                    StepAPUSweep(apu);
                    StepAPULength(apu);
                    break;
                }

                case 2:
                {
                    StepAPUEnvenlope(apu);
                    break;
                }

                case 3:
                {
                    StepAPUEnvenlope(apu);
                    StepAPUSweep(apu);
                    StepAPULength(apu);

                    if (apu->frameIRQ)
                    {
                        // check for interrup flag in cpu
                        nes->cpu.interrupt = CPU_INTERRUPT_IRQ;
                    }
                    break;
                }
            }

            break;
        }

        // 5-step mode
        case 1:
        {
            apu->frameValue = (apu->frameValue + 1) % 5;

            switch (apu->frameValue)
            {
                case 0:
                {
                    StepAPUEnvenlope(apu);
                    StepAPUSweep(apu);
                    StepAPULength(apu);
                    break;
                }

                case 1:
                {
                    StepAPUEnvenlope(apu);
                    break;
                }

                case 2:
                {
                    StepAPUEnvenlope(apu);
                    StepAPUSweep(apu);
                    StepAPULength(apu);
                    break;
                }

                case 3:
                {
                    StepAPUEnvenlope(apu);
                    break;
                }

                case 4:
                {
                    break;
                }
            }

            break;
        }
    }
}

internal f32 GetOutput(APU *apu)
{
    u8 p1 = GetPulseOutput(&apu->pulse1);
    u8 p2 = GetPulseOutput(&apu->pulse2);
    /*f32 t = GetTriangleOutput(&apu->triangle);
    f32 n = GetNoiseOutput(&apu->noise);
    f32 d = GetDmcOutput(&apu->dmc);*/

    f32 pulseOut = pulseTable[p1 + p2];
    f32 tndOut = 0; // tndTable[3 * t + 2 * n + d];
    return pulseOut + tndOut;
}

void StepAPU(NES *nes)
{
    APU *apu = &nes->apu;

    u64 cycle1 = apu->cycles;
    apu->cycles++;
    u64 cycle2 = apu->cycles;

    StepAPUTimer(nes);

    s32 f1 = (s32)((f32)cycle1 / FRAME_COUNTER_RATE);
    s32 f2 = (s32)((f32)cycle2 / FRAME_COUNTER_RATE);
    if (f1 != f2)
    {
        StepAPUFrameCounter(nes);
    }

    /*s32 s1 = (s32)((f32)cycle1 / apu->sampleRate);
    s32 s2 = (s32)((f32)cycle2 / apu->sampleRate);
    if (s1 != s2)*/
    {
        s16 output = GetOutput(apu);
        apu->buffer[apu->bufferPos] = output;
        apu->bufferPos = (apu->bufferPos + 1) % 4096;
    }
}

void WriteAPUFrameCounter(NES *nes, u8 value)
{
    APU *apu = &nes->apu;

    apu->framePeriod = (value & 0x80) >> 7;
    apu->frameIRQ = (value & 0x40) >> 6;
    apu->frameValue = 0;

    if (apu->framePeriod)
    {
        StepAPUEnvenlope(apu);
        StepAPUSweep(apu);
        StepAPULength(apu);
    }
}