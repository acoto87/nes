#include "cpu.h"
#include "memory.h"

#pragma region [ Instructions ]

internal void HandleInterrupt(NES *nes, u16 interruptAddress, b32 fromBRK)
{
    CPU *cpu = &nes->cpu;

    SetBreak(cpu, fromBRK);

    PushStackU16(nes, cpu->pc);
    PushStackU8(nes, cpu->p | 0x30);

    SetInterrupt(cpu, 1);

    cpu->pc = ReadCPUU16(nes, interruptAddress);

    if (!fromBRK)
    {
        cpu->cycles += 7;
    }
}

internal inline void ADC(NES* nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    u16 acc = cpu->a + data;

    if (GetCarry(cpu))
    {
        acc += 1;
    }

    SetCarry(cpu, acc > 0xFF);
    SetNegative(cpu, ISNEG(acc));
    SetZero(cpu, !(acc & 0xFF));
    SetOverflow(cpu, !((cpu->a ^ data) & 0x80) && ((cpu->a ^ acc) & 0x80));

    cpu->a = (u8)acc;
}

internal inline void AND(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    data = cpu->a & data;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void ASL(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u16 data;

    // if address = 0, then it's ACC addressing mode
    if (!address)
    {
        data = cpu->a;
    }
    else
    {
        data = ReadCPUU8(nes, address);
    }

    SetCarry(cpu, data & 0x80);

    data <<= 1;
    data &= 0xFF;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    // if address = 0, then it's ACC addressing mode
    if (!address)
    {
        cpu->a = (u8)data;
    }
    else
    {
        WriteCPUU8(nes, address, (u8)data);
    }
}

internal inline void BCC(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    s8 data = (s8)ReadCPUU8(nes, address);
    u8 C = GetCarry(cpu);

    b32 hasCrossPage = PAGE_CROSS(cpu->pc, cpu->pc + data);

    if (!C)
    {
        cpu->pc += data;
        cpu->cycles += 1;
        if (hasCrossPage)
        {
            cpu->cycles += 1;
        }
    }
}

internal inline void BCS(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    s8 data = (s8)ReadCPUU8(nes, address);
    u8 C = GetCarry(cpu);

    b32 hasCrossPage = PAGE_CROSS(cpu->pc, cpu->pc + data);

    if (C)
    {
        cpu->pc += data;
        cpu->cycles += 1;
        if (hasCrossPage)
        {
            cpu->cycles += 1;
        }
    }
}

internal inline void BEQ(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    s8 data = (s8)ReadCPUU8(nes, address);
    u8 Z = GetZero(cpu);

    b32 hasCrossPage = PAGE_CROSS(cpu->pc, cpu->pc + data);

    if (Z)
    {
        cpu->pc += data;
        cpu->cycles += 1;
        if (hasCrossPage)
        {
            cpu->cycles += 1;
        }
    }
}

internal inline void BIT(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    SetNegative(cpu, HAS_FLAG(data, BIT7_MASK));
    SetOverflow(cpu, HAS_FLAG(data, BIT6_MASK));
    SetZero(cpu, !(cpu->a & data));
}

internal inline void BMI(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    s8 data = (s8)ReadCPUU8(nes, address);
    u8 N = GetNegative(cpu);

    b32 hasCrossPage = PAGE_CROSS(cpu->pc, cpu->pc + data);

    if (N)
    {
        cpu->pc += data;
        cpu->cycles += 1;
        if (hasCrossPage)
        {
            cpu->cycles += 1;
        }
    }
}

internal inline void BNE(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    s8 data = (s8)ReadCPUU8(nes, address);
    u8 Z = GetZero(cpu);

    b32 hasCrossPage = PAGE_CROSS(cpu->pc, cpu->pc + data);

    if (!Z)
    {
        cpu->pc += data;
        cpu->cycles += 1;
        if (hasCrossPage)
        {
            cpu->cycles += 1;
        }
    }
}

internal inline void BPL(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    s8 data = (s8)ReadCPUU8(nes, address);
    u8 N = GetNegative(cpu);

    b32 hasCrossPage = PAGE_CROSS(cpu->pc, cpu->pc + data);

    if (!N)
    {
        cpu->pc += data;
        cpu->cycles += 1;
        if (hasCrossPage)
        {
            cpu->cycles += 1;
        }
    }
}

internal inline void BRK(NES *nes)
{
    HandleInterrupt(nes, CPU_IRQ_ADDRESS, TRUE);
}

internal inline void BVC(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    s8 data = (s8)ReadCPUU8(nes, address);
    u8 V = GetOverflow(cpu);

    b32 hasCrossPage = PAGE_CROSS(cpu->pc, cpu->pc + data);

    if (!V)
    {
        cpu->pc += data;
        cpu->cycles += 1;
        if (hasCrossPage)
        {
            cpu->cycles += 1;
        }
    }
}

internal inline void BVS(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    s8 data = (s8)ReadCPUU8(nes, address);
    u8 V = GetOverflow(cpu);

    b32 hasCrossPage = PAGE_CROSS(cpu->pc, cpu->pc + data);

    if (V)
    {
        cpu->pc += data;
        cpu->cycles += 1;
        if (hasCrossPage)
        {
            cpu->cycles += 1;
        }
    }
}

internal inline void CLC(NES *nes)
{
    CPU *cpu = &nes->cpu;
    SetCarry(cpu, 0);
    // ClearBitFlag(&cpu->p, CARRY_FLAG);
}

internal inline void CLD(NES *nes)
{
    CPU *cpu = &nes->cpu;
    SetDecimal(cpu, 0);
    // ClearBitFlag(&cpu->p, DECIMAL_FLAG);
}

internal inline void CLI(NES *nes)
{
    CPU *cpu = &nes->cpu;
    SetInterrupt(cpu, 0);
    // ClearBitFlag(&cpu->p, INTERRUPT_FLAG);
}

internal inline void CLV(NES *nes)
{
    CPU *cpu = &nes->cpu;
    SetOverflow(cpu, 0);
    // ClearBitFlag(&cpu->p, OVERFLOW_FLAG);
}

internal inline void CMP(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    u16 sub = cpu->a - data;

    SetNegative(cpu, ISNEG(sub));
    SetZero(cpu, !sub);
    SetCarry(cpu, sub < 0x100);
}

internal inline void CPX(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    u16 sub = cpu->x - data;

    SetNegative(cpu, ISNEG(sub));
    SetZero(cpu, !sub);
    SetCarry(cpu, sub < 0x100);
}

internal inline void CPY(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    u16 sub = cpu->y - data;

    SetNegative(cpu, ISNEG(sub));
    SetZero(cpu, !sub);
    SetCarry(cpu, sub < 0x100);
}

internal inline void DEC(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    data -= 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    WriteCPUU8(nes, address, data);
}

internal inline void DEX(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = cpu->x;
    data -= 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->x = data;
}

internal inline void DEY(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = cpu->y;
    data -= 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->y = data;
}

internal inline void EOR(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    data = cpu->a ^ data;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void INC(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    data += 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    WriteCPUU8(nes, address, data);
}

internal inline void INX(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = cpu->x;
    data += 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->x = data;
}

internal inline void INY(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = cpu->y;
    data += 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->y = data;
}

internal inline void JMP(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    cpu->pc = address;
}

internal inline void JSR(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    PushStackU16(nes, cpu->pc - 1);

    cpu->pc = address;
}

internal inline void LDA(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void LDX(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->x = data;
}

internal inline void LDY(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->y = data;
}

internal inline void LSR(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data;

    // if address = 0, then it's ACC addressing mode
    if (!address)
    {
        data = cpu->a;
    }
    else
    {
        data = ReadCPUU8(nes, address);
    }

    SetCarry(cpu, data & 0x01);

    data >>= 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    // if address = 0, then it's ACC addressing mode
    if (!address)
    {
        cpu->a = data;
    }
    else
    {
        WriteCPUU8(nes, address, data);
    }
}

internal inline void NOP(NES *nes)
{
    // No operation
}

internal inline void ORA(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    data |= cpu->a;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void PHA(NES *nes)
{
    CPU *cpu = &nes->cpu;

    PushStackU8(nes, cpu->a);
}

internal inline void PHP(NES *nes)
{
    CPU *cpu = &nes->cpu;

    PushStackU8(nes, cpu->p | 0x30);
}

internal inline void PLA(NES *nes)
{
    CPU *cpu = &nes->cpu;

    cpu->a = PopStackU8(nes);

    SetNegative(cpu, ISNEG(cpu->a));
    SetZero(cpu, !cpu->a);
}

internal inline void PLP(NES *nes)
{
    CPU *cpu = &nes->cpu;

    u8 v = PopStackU8(nes);
    cpu->p = (v & 0xC0) | (cpu->p & 0x30) | (v & 0x0F);
}

internal inline void ROL(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data;

    // if address = 0, then it's ACC addressing mode
    if (!address)
    {
        data = cpu->a;
    }
    else
    {
        data = ReadCPUU8(nes, address);
    }

    b32 setCarry = data & 0x80;

    data <<= 1;

    if (GetCarry(cpu))
    {
        data |= 0x01;
    }

    SetCarry(cpu, setCarry);
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    // if address = 0, then it's ACC addressing mode
    if (!address)
    {
        cpu->a = data;
    }
    else
    {
        WriteCPUU8(nes, address, data);
    }
}

internal inline void ROR(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data;

    // if address = 0, then it's ACC addressing mode
    if (!address)
    {
        data = cpu->a;
    }
    else
    {
        data = ReadCPUU8(nes, address);
    }

    b32 setCarry = data & 0x01;

    data >>= 1;

    if (GetCarry(cpu))
    {
        data |= 0x80;
    }

    SetCarry(cpu, setCarry);
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    // if address = 0, then it's ACC addressing mode
    if (!address)
    {
        cpu->a = data;
    }
    else
    {
        WriteCPUU8(nes, address, data);
    }
}

internal inline void RTI(NES *nes)
{
    CPU *cpu = &nes->cpu;

    u8 v = PopStackU8(nes);
    cpu->p = (v & 0xC0) | (cpu->p & 0x20) | (v & 0x1F);
    cpu->pc = PopStackU16(nes);
}

internal inline void RTS(NES *nes)
{
    CPU *cpu = &nes->cpu;

    cpu->pc = PopStackU16(nes);
    cpu->pc += 1;
}

internal inline void SBC(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    u16 acc = cpu->a - data;

    if (!GetCarry(cpu))
    {
        acc -= 1;
    }

    SetCarry(cpu, acc < 0x100);
    SetNegative(cpu, ISNEG(acc));
    SetZero(cpu, !(acc & 0xFF));
    SetOverflow(cpu, ((cpu->a ^ data) & 0x80) && ((cpu->a ^ acc) & 0x80));

    cpu->a = (u8)acc;
}

internal inline void SEC(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    SetCarry(cpu, 1);
    // SetBitFlag(&cpu->p, CARRY_FLAG);
}

internal inline void SED(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    SetDecimal(cpu, 1);
    // SetBitFlag(&cpu->p, DECIMAL_FLAG);
}

internal inline void SEI(NES *nes)
{
    CPU *cpu = &nes->cpu;

    SetInterrupt(cpu, 1);
}

internal inline void STA(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    WriteCPUU8(nes, address, cpu->a);
}

internal inline void STX(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    WriteCPUU8(nes, address, cpu->x);
}

internal inline void STY(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    WriteCPUU8(nes, address, cpu->y);
}

internal inline void TAX(NES *nes)
{
    CPU *cpu = &nes->cpu;

    u8 data = cpu->a;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->x = data;
}

internal inline void TAY(NES *nes)
{
    CPU *cpu = &nes->cpu;

    u8 data = cpu->a;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->y = data;
}

internal inline void TSX(NES *nes)
{
    CPU *cpu = &nes->cpu;

    u8 data = cpu->sp;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->x = data;
}

internal inline void TXA(NES *nes)
{
    CPU *cpu = &nes->cpu;

    u8 data = cpu->x;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void TXS(NES *nes)
{
    CPU *cpu = &nes->cpu;
    u8 data = cpu->x;

    cpu->sp = data;
}

internal inline void TYA(NES *nes)
{
    CPU *cpu = &nes->cpu;

    u8 data = cpu->y;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

//
// Illegal or unofficial opcodes
//

internal inline void SLO(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    SetCarry(cpu, data & 0x80);

    data <<= 1;

    WriteCPUU8(nes, address, data);

    data |= cpu->a;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void ANC(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    data = cpu->a & data;

    SetCarry(cpu, data & 0x80);
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void RLA(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    b32 setCarry = data & 0x80;

    data <<= 1;

    if (GetCarry(cpu))
    {
        data |= 0x01;
    }

    u8 acc = cpu->a & data;

    SetCarry(cpu, setCarry);
    SetNegative(cpu, ISNEG(acc));
    SetZero(cpu, !acc);

    WriteCPUU8(nes, address, data);
    cpu->a = acc;
}

internal inline void SRE(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    SetCarry(cpu, data & 0x01);

    data >>= 1;

    WriteCPUU8(nes, address, data);

    data = cpu->a ^ data;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void ALR(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    data = cpu->a & data;

    SetCarry(cpu, data & 0x01);

    data >>= 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void RRA(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    b32 setCarry = data & 0x01;

    data >>= 1;

    if (GetCarry(cpu))
    {
        data |= 0x80;
    }

    u16 acc = cpu->a + data;

    if (setCarry)
    {
        acc += 1;
    }

    SetCarry(cpu, acc > 0xFF);
    SetNegative(cpu, ISNEG(acc));
    SetZero(cpu, !(acc & 0xFF));
    SetOverflow(cpu, !((cpu->a ^ data) & 0x80) && ((cpu->a ^ acc) & 0x80));

    WriteCPUU8(nes, address, data);
    cpu->a = (u8)acc;
}

internal inline void ARR(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    data = cpu->a & data;

    b32 setCarry = data & 0x01;

    data >>= 1;

    if (GetCarry(cpu))
    {
        data |= 0x80;
    }

    SetOverflow(cpu, (data & 0x20) ^ (data & 0x40));
    SetCarry(cpu, data & 0x40);
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void SAX(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    WriteCPUU8(nes, address, cpu->a & cpu->x);
}

internal inline void XAA(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    data &= cpu->x;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

internal inline void AHX(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 H = (u8)((address & 0xFF00) >> 8);
    WriteCPUU8(nes, address, cpu->a & cpu->x & H);
}

internal inline void TAS(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;
    
    cpu->sp = cpu->a & cpu->x;

    u8 H = (u8)((address & 0xFF00) >> 8);
    WriteCPUU8(nes, address, cpu->sp & H);
}

internal inline void SHY(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 H = (u8)((address & 0xFF00) >> 8);
    WriteCPUU8(nes, address, cpu->y & H);
}

internal inline void SHX(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 H = (u8)((address & 0xFF00) >> 8);
    WriteCPUU8(nes, address, cpu->x & H);
}

internal inline void LAX(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
    cpu->x = data;
}

internal inline void LAS(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);

    data &= cpu->sp;

    cpu->a = data;
    cpu->x = data;
    cpu->sp = data;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);
}

internal inline void DCP(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    data -= 1;

    u16 sub = cpu->a - data;

    SetNegative(cpu, ISNEG(sub));
    SetZero(cpu, !sub);
    SetCarry(cpu, sub < 0x100);

    WriteCPUU8(nes, address, data);
}

internal inline void AXS(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u16 data = ReadCPUU8(nes, address);

    data = (u16)(cpu->a & cpu->x) - data;

    SetCarry(cpu, data < 0x100);
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !(data & 0xFF));

    cpu->x = (u8)data;
}

internal inline void ISC(NES *nes, u16 address)
{
    CPU *cpu = &nes->cpu;

    u8 data = ReadCPUU8(nes, address);
    data += 1;

    u16 acc = cpu->a - data;

    if (!GetCarry(cpu))
    {
        acc -= 1;
    }

    SetCarry(cpu, acc < 0x100);
    SetNegative(cpu, ISNEG(acc));
    SetZero(cpu, !(acc & 0xFF));
    SetOverflow(cpu, ((cpu->a ^ data) & 0x80) && ((cpu->a ^ acc) & 0x80));

    WriteCPUU8(nes, address, data);
    cpu->a = (u8)acc;
}

internal inline void KIL(NES *nes)
{
    ASSERT(FALSE);
}

internal inline void FEX(NES *nes)
{
}

internal inline CPUInstruction* GetNextInstruction(NES *nes)
{
    CPU *cpu = &nes->cpu;
    u8 opcode = ReadCPUU8(nes, cpu->pc);
    CPUInstruction *instruction = &cpuInstructions[opcode];
    return instruction;
}

#pragma endregion

void PowerCPU(NES *nes)
{
    CPU *cpu = &nes->cpu;
    cpu->a = 0;
    cpu->x = 0;
    cpu->y = 0;
    cpu->cycles = 0;
    cpu->sp = CPU_STACK_PTR_INITIAL_VALUE;
    cpu->p = CPU_STATUS_INITIAL_VALUE;
    cpu->waitCycles = 0;
    cpu->pc = ReadCPUU16(nes, CPU_RESET_ADDRESS);
    cpu->interrupt = CPU_INTERRUPT_NON;

    PushStackU8(nes, 0xFF);
    PushStackU8(nes, 0xFF);
}

void ResetCPU(NES *nes)
{
    CPU *cpu = &nes->cpu;
    cpu->cycles = 0;
    cpu->waitCycles = 0;
    cpu->sp -= 3;
    cpu->p |= 0x04;
    cpu->pc = ReadCPUU16(nes, CPU_RESET_ADDRESS);
    cpu->interrupt = CPU_INTERRUPT_NON;
}

void InitCPU(NES *nes)
{
    if (!nes->cpuMemory.created)
    {
        CreateMemory(&nes->cpuMemory, CPU_RAM_TOTAL_SIZE);

        u32 pgrBanks = nes->cartridge.pgrBanks;
        u32 pgrSizeInBytes = nes->cartridge.pgrSizeInBytes;
        u8 *pgr = nes->cartridge.pgr;
        CopyMemoryBytes(&nes->cpuMemory, CPU_PGR_BANK_0_OFFSET, pgr, CPU_PGR_BANK_SIZE);

        if (pgrBanks < 2)
        {
            CopyMemoryBytes(&nes->cpuMemory, CPU_PGR_BANK_1_OFFSET, pgr, CPU_PGR_BANK_SIZE);
        }
    }

    PowerCPU(nes);
}

internal void ExecuteInstruction(NES *nes, CPUInstruction *instruction)
{
    ASSERT(instruction->instruction != CPU_KIL);

    CPU *cpu = &nes->cpu;

    u16 address;
    b32 pageCrossed = FALSE;

    switch (instruction->addressingMode)
    {
        // Immediate #$00
        case AM_IMM:
        {
            address = cpu->pc + 1;
            break;
        }

        // Absolute $0000
        case AM_ABS:
        {
            address = ReadCPUU16(nes, cpu->pc + 1);
            break;
        }

        // Absolute $0000, X
        case AM_ABX:
        {
            address = ReadCPUU16(nes, cpu->pc + 1);
            pageCrossed = PAGE_CROSS(address, address + cpu->x);
            address += cpu->x;
            break;
        }

        // Absolute $0000, Y
        case AM_ABY:
        {
            address = ReadCPUU16(nes, cpu->pc + 1);
            pageCrossed = PAGE_CROSS(address, address + cpu->y);
            address += cpu->y;
            break;
        }

        // Zero-Page-Absolute $00
        case AM_ZPA:
        {
            address = ReadCPUU8(nes, cpu->pc + 1);
            break;
        }

        // Zero-Page-Indexed $00, X
        case AM_ZPX:
        {
            u8 operand = ReadCPUU8(nes, cpu->pc + 1);
            // it needs an u8 here for wrapping
            operand += cpu->x;
            address = operand;
            break;
        }

        // Zero-Page-Indexed $00, Y
        case AM_ZPY:
        {
            u8 operand = ReadCPUU8(nes, cpu->pc + 1);
            // it needs an u8 here for wrapping
            operand += cpu->y;
            address = operand;
            break;
        }

        // Indirect ($0000)
        case AM_IND:
        {
            u16 operand = ReadCPUU16(nes, cpu->pc + 1);

            // emulates a 6502 bug that caused the low byte to wrap without incrementing the high byte
            u16 operand2 = (u16)(operand & 0xFF00) | (u8)(operand + 1);

            u8 lo = ReadCPUU8(nes, operand);
            u8 hi = ReadCPUU8(nes, operand2);

            address = (hi << 8) | lo;

            /*address = ReadCPUU16(nes, cpu->pc + 1);
            address = ReadCPUU16(nes, address);*/
            break;
        }

        // Pre-Indexed-Indirect ($00, X)
        case AM_IZX:
        {
            u8 operand = ReadCPUU8(nes, cpu->pc + 1);
            // it needs an u8 here for wrapping
            operand += cpu->x;

            // emulates a 6502 bug that caused the low byte to wrap without incrementing the high byte
            u16 operand2 = (u16)(operand & 0xFF00) | (u8)(operand + 1);

            u8 lo = ReadCPUU8(nes, operand);
            u8 hi = ReadCPUU8(nes, operand2);

            address = (hi << 8) | lo;

            /*address = ReadCPUU16(nes, operand);*/
            break;
        }

        // Post-Indexed-Indirect ($00), Y
        case AM_IZY:
        {
            u8 operand = ReadCPUU8(nes, cpu->pc + 1);

            // emulates a 6502 bug that caused the low byte to wrap without incrementing the high byte
            u16 operand2 = (u16)(operand & 0xFF00) | (u8)(operand + 1);

            u8 lo = ReadCPUU8(nes, operand);
            u8 hi = ReadCPUU8(nes, operand2);

            address = (hi << 8) | lo;

            pageCrossed = PAGE_CROSS(address, address + cpu->y);
            address += cpu->y;
            break;
        }

        // Implied
        case AM_IMP:
        {
            address = 0;
            break;
        }

        // Accumulator
        case AM_ACC:
        {
            address = 0;
            break;
        }

        // Relative $0000
        case AM_REL:
        {
            address = cpu->pc + 1;
            break;
        }

        default:
        {
            ASSERT(false);
            break;
        }
    }

    cpu->pc += instruction->bytesCount;
    cpu->cycles += instruction->cyclesCount;
    if (pageCrossed)
    {
        cpu->cycles += instruction->pageCycles;
    }

    switch (instruction->instruction)
    {
        case CPU_ADC: { ADC(nes, address); break; }
        case CPU_AND: { AND(nes, address); break; }
        case CPU_ASL: { ASL(nes, address); break; }
        case CPU_BCC: { BCC(nes, address); break; }
        case CPU_BCS: { BCS(nes, address); break; }
        case CPU_BEQ: { BEQ(nes, address); break; }
        case CPU_BIT: { BIT(nes, address); break; }
        case CPU_BMI: { BMI(nes, address); break; }
        case CPU_BNE: { BNE(nes, address); break; }
        case CPU_BPL: { BPL(nes, address); break; }
        case CPU_BRK: { BRK(nes); break; }
        case CPU_BVC: { BVC(nes, address); break; }
        case CPU_BVS: { BVS(nes, address); break; }
        case CPU_CLC: { CLC(nes); break; }
        case CPU_CLD: { CLD(nes); break; }
        case CPU_CLI: { CLI(nes); break; }
        case CPU_CLV: { CLV(nes); break; }
        case CPU_CMP: { CMP(nes, address); break; }
        case CPU_CPX: { CPX(nes, address); break; }
        case CPU_CPY: { CPY(nes, address); break; }
        case CPU_DEC: { DEC(nes, address); break; }
        case CPU_DEX: { DEX(nes, address); break; }
        case CPU_DEY: { DEY(nes, address); break; }
        case CPU_EOR: { EOR(nes, address); break; }
        case CPU_INC: { INC(nes, address); break; }
        case CPU_INX: { INX(nes, address); break; }
        case CPU_INY: { INY(nes, address); break; }
        case CPU_JMP: { JMP(nes, address); break; }
        case CPU_JSR: { JSR(nes, address); break; }
        case CPU_LDA: { LDA(nes, address); break; }
        case CPU_LDX: { LDX(nes, address); break; }
        case CPU_LDY: { LDY(nes, address); break; }
        case CPU_LSR: { LSR(nes, address); break; }
        case CPU_NOP: { NOP(nes); break; }
        case CPU_ORA: { ORA(nes, address); break; }
        case CPU_PHA: { PHA(nes); break; }
        case CPU_PHP: { PHP(nes); break; }
        case CPU_PLA: { PLA(nes); break; }
        case CPU_PLP: { PLP(nes); break; }
        case CPU_ROL: { ROL(nes, address); break; }
        case CPU_ROR: { ROR(nes, address); break; }
        case CPU_RTI: { RTI(nes); break; }
        case CPU_RTS: { RTS(nes); break; }
        case CPU_SBC: { SBC(nes, address); break; }
        case CPU_SEC: { SEC(nes, address); break; }
        case CPU_SED: { SED(nes, address); break; }
        case CPU_SEI: { SEI(nes); break; }
        case CPU_STA: { STA(nes, address); break; }
        case CPU_STX: { STX(nes, address); break; }
        case CPU_STY: { STY(nes, address); break; }
        case CPU_TAX: { TAX(nes); break; }
        case CPU_TAY: { TAY(nes); break; }
        case CPU_TSX: { TSX(nes); break; }
        case CPU_TXA: { TXA(nes); break; }
        case CPU_TXS: { TXS(nes); break; }
        case CPU_TYA: { TYA(nes); break; }
        case CPU_SLO: { SLO(nes, address); break; }
        case CPU_ANC: { ANC(nes, address); break; }
        case CPU_RLA: { RLA(nes, address); break; }
        case CPU_SRE: { SRE(nes, address); break; }
        case CPU_ALR: { ALR(nes, address); break; }
        case CPU_RRA: { RRA(nes, address); break; }
        case CPU_ARR: { ARR(nes, address); break; }
        case CPU_SAX: { SAX(nes, address); break; }
        case CPU_XAA: { XAA(nes, address); break; }
        case CPU_AHX: { AHX(nes, address); break; }
        case CPU_TAS: { TAS(nes, address); break; }
        case CPU_SHY: { SHY(nes, address); break; }
        case CPU_SHX: { SHX(nes, address); break; }
        case CPU_LAX: { LAX(nes, address); break; }
        case CPU_LAS: { LAS(nes, address); break; }
        case CPU_DCP: { DCP(nes, address); break; }
        case CPU_AXS: { AXS(nes, address); break; }
        case CPU_ISC: { ISC(nes, address); break; }
        case CPU_KIL: { KIL(nes); break; }
        case CPU_FEX: { FEX(nes); break; }
        default: { break; }
    }
}

CPUStep StepCPU(NES *nes)
{
    CPUStep step = {};

    CPU *cpu = &nes->cpu;
    u32 startCpuCycles = cpu->cycles;

    if (cpu->waitCycles)
    {
        --cpu->waitCycles;
        ++cpu->cycles;

        step.cycles = 1;
        step.instruction = NULL;
        return step;
    }

    // if there is an interrupt, the cpu must handle it first before any instructions
    if (cpu->interrupt)
    {
        switch (cpu->interrupt)
        {
            case CPU_INTERRUPT_RES:
            {
                HandleInterrupt(nes, CPU_RESET_ADDRESS, FALSE);
                break;
            }

            // non-maskable interrupt
            case CPU_INTERRUPT_NMI:
            {
                HandleInterrupt(nes, CPU_NMI_ADDRESS, FALSE);
                break;
            }

            case CPU_INTERRUPT_IRQ:
            {
                if (!GetBitFlag(cpu->p, INTERRUPT_FLAG))
                {
                    HandleInterrupt(nes, CPU_IRQ_ADDRESS, FALSE);
                }
                break;
            }
        }

        cpu->interrupt = CPU_INTERRUPT_NON;

        step.cycles = cpu->cycles - startCpuCycles;
        step.instruction = NULL;
        return step;
    }

    CPUInstruction *instruction = GetNextInstruction(nes);

    ExecuteInstruction(nes, instruction);

    step.cycles = cpu->cycles - startCpuCycles;
    step.instruction = instruction;
    return step;
}