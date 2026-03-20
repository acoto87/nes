#include "cpu.h"

typedef struct CPUOperand {
    u16 address;
    u8 value;
    u16 fixupAddress;
    bool pageCrossed;
} CPUOperand;

// Advances one CPU cycle and clocks PPU/APU while latching NMI edges.
internal inline void CPUAdvanceOneCycle(NES* nes)
{
    CPU* cpu = &nes->cpu;

    StepPPUCycles(nes, 1);
    StepAPUCycles(nes, 1);

    if (!cpu->prevNmiLine && cpu->nmiLine) {
        cpu->nmiPending = true;
    }

    cpu->prevNmiLine = cpu->nmiLine;
    cpu->cycles++;
}

// Spends one pending wait cycle without executing an instruction.
internal inline void CPUStallCycle(NES* nes)
{
    nes->cpu.waitCycles--;
    CPUAdvanceOneCycle(nes);
}

// Reads one byte on the CPU bus and accounts for the cycle.
internal inline u8 CPUBusRead(NES* nes, u16 address)
{
    u8 value = ReadCPUU8(nes, address);
    CPUAdvanceOneCycle(nes);
    return value;
}

// Writes one byte on the CPU bus and accounts for the cycle.
internal inline void CPUBusWrite(NES* nes, u16 address, u8 value)
{
    WriteCPUU8(nes, address, value);
    CPUAdvanceOneCycle(nes);
}

// Models a throwaway read needed for bus-accurate timing.
internal inline void CPUDummyRead(NES* nes, u16 address)
{
    ReadCPUU8(nes, address);
    CPUAdvanceOneCycle(nes);
}

// Performs an implied-mode discard read from the current PC.
internal inline void CPUDummyReadPC(NES* nes)
{
    CPUDummyRead(nes, nes->cpu.pc);
}

// Computes the unstable high-byte mask for abs-indexed unofficial stores.
internal inline u8 CPUUnstableStoreMask(CPUOperand* operand)
{
    return (u8)(((operand->fixupAddress >> 8) + 1) & 0xFF);
}

// Applies the page-cross address glitch for unstable abs-indexed stores.
internal inline u16 CPUUnstableStoreAddress(CPUOperand* operand, u8 value)
{
    if (operand->pageCrossed) {
        return ((u16)value << 8) | (operand->address & 0x00FF);
    }

    return operand->address;
}

// Fetches the next byte from PC and increments PC.
internal inline u8 CPUFetchPC(NES* nes)
{
    u8 value = CPUBusRead(nes, nes->cpu.pc);
    nes->cpu.pc += 1;
    return value;
}

// Fetches a little-endian 16-bit operand from PC.
internal inline u16 CPUFetchPC16(NES* nes)
{
    u8 low = CPUFetchPC(nes);
    u8 high = CPUFetchPC(nes);
    return (high << 8) | low;
}

// Pushes one byte with live stack bus timing.
internal inline void CPUPushStack8(NES* nes, u8 value)
{
    PushStackU8(nes, value);
    CPUAdvanceOneCycle(nes);
}

// Pushes a 16-bit value high byte first.
internal inline void CPUPushStack16(NES* nes, u16 value)
{
    CPUPushStack8(nes, (value & 0xFF00) >> 8);
    CPUPushStack8(nes, value & 0xFF);
}

// Pops one byte with live stack bus timing.
internal inline u8 CPUPopStack8(NES* nes)
{
    u8 value = PopStackU8(nes);
    CPUAdvanceOneCycle(nes);
    return value;
}

// Pops a 16-bit value low byte first.
internal inline u16 CPUPopStack16(NES* nes)
{
    u8 low = CPUPopStack8(nes);
    u8 high = CPUPopStack8(nes);
    return (high << 8) | low;
}

// Converts SP into an absolute stack address.
internal inline u16 CPUStackAddress(u8 sp)
{
    return 0x0100 | sp;
}

// Consumes one reset-time stack byte without writing data.
internal inline void CPUDiscardStackByte(NES* nes)
{
    CPU* cpu = &nes->cpu;
    CPUDummyRead(nes, CPUStackAddress(cpu->sp));
    cpu->sp = cpu->sp > 0 ? cpu->sp - 1 : 0xFF;
}

// Reads a 16-bit value as two timed bus reads.
internal inline u16 CPUBusRead16(NES* nes, u16 address)
{
    u8 low = CPUBusRead(nes, address);
    u8 high = CPUBusRead(nes, address + 1);
    return (high << 8) | low;
}

// Samples NMI/IRQ state and queues the next interrupt service.
internal inline void CPUPollInterrupts(NES* nes)
{
    CPU* cpu = &nes->cpu;
    bool irqDisabled = GetInterrupt(cpu);

    if (cpu->pendingService == CPU_INTERRUPT_RES) {
        return;
    }

    if (cpu->irqPollIOverrideValid) {
        irqDisabled = cpu->irqPollIOverride;
        cpu->irqPollIOverrideValid = false;
    }

    if (cpu->nmiPending) {
        cpu->pendingService = CPU_INTERRUPT_NMI;
        cpu->nmiPending = false;
        return;
    }

    if (cpu->irqSources && !irqDisabled) {
        cpu->pendingService = CPU_INTERRUPT_IRQ;
    }
}

// Normalizes P for PHP/BRK/interrupt stack pushes.
internal inline u8 CPUComposePForPush(CPU* cpu, bool breakFlag)
{
    u8 status = (cpu->p | 0x20) & 0xEF;
    if (breakFlag) {
        status |= 0x10;
    }
    return status;
}

// Restores P while keeping the 6502 constant bits sane.
internal inline void CPURestoreP(CPU* cpu, u8 pulled)
{
    cpu->p = (pulled | 0x20) & 0xEF;
}

// Promotes a wrapped zero-page offset to a bus address.
internal inline u16 CPUZeroPageWrap(u8 addr)
{
    return addr;
}

// Checks whether indexed addressing crossed a page.
internal inline bool CPUPageCrossed(u16 baseAddress, u16 indexedAddress)
{
    return (baseAddress & 0xFF00) != (indexedAddress & 0xFF00);
}

// Builds the wrong-page address used by indexed dummy reads.
internal inline u16 CPUIndexedFixupAddress(u16 baseAddress, u16 indexedAddress)
{
    return (baseAddress & 0xFF00) | (indexedAddress & 0x00FF);
}

// Resolves read-class addressing and fetches the operand value.
internal CPUOperand CPUFetchReadOperand(NES* nes, CPUAddressingMode mode)
{
    CPU* cpu = &nes->cpu;
    CPUOperand operand = {0};

    switch (mode) {
        case AM_IMM: {
            operand.address = cpu->pc;
            operand.value = CPUFetchPC(nes);
            break;
        }

        case AM_ZPA: {
            operand.address = CPUZeroPageWrap(CPUFetchPC(nes));
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_ZPX: {
            u8 baseAddress = CPUFetchPC(nes);
            CPUDummyRead(nes, CPUZeroPageWrap(baseAddress));
            operand.address = CPUZeroPageWrap((u8)(baseAddress + cpu->x));
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_ZPY: {
            u8 baseAddress = CPUFetchPC(nes);
            CPUDummyRead(nes, CPUZeroPageWrap(baseAddress));
            operand.address = CPUZeroPageWrap((u8)(baseAddress + cpu->y));
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_ABS: {
            operand.address = CPUFetchPC16(nes);
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_ABX: {
            u16 baseAddress = CPUFetchPC16(nes);
            operand.address = baseAddress + cpu->x;
            operand.pageCrossed = CPUPageCrossed(baseAddress, operand.address);
            operand.fixupAddress = CPUIndexedFixupAddress(baseAddress, operand.address);
            operand.value = CPUBusRead(nes, operand.fixupAddress);

            if (operand.pageCrossed) {
                operand.value = CPUBusRead(nes, operand.address);
            } else {
                operand.address = operand.fixupAddress;
            }
            break;
        }

        case AM_ABY: {
            u16 baseAddress = CPUFetchPC16(nes);
            operand.address = baseAddress + cpu->y;
            operand.pageCrossed = CPUPageCrossed(baseAddress, operand.address);
            operand.fixupAddress = CPUIndexedFixupAddress(baseAddress, operand.address);
            operand.value = CPUBusRead(nes, operand.fixupAddress);

            if (operand.pageCrossed) {
                operand.value = CPUBusRead(nes, operand.address);
            } else {
                operand.address = operand.fixupAddress;
            }
            break;
        }

        case AM_IZX: {
            u8 pointerBase = CPUFetchPC(nes);
            CPUDummyRead(nes, CPUZeroPageWrap(pointerBase));

            u8 pointer = (u8)(pointerBase + cpu->x);
            u8 low = CPUBusRead(nes, CPUZeroPageWrap(pointer));
            u8 high = CPUBusRead(nes, CPUZeroPageWrap((u8)(pointer + 1)));

            operand.address = ((u16)high << 8) | low;
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_IZY: {
            u8 pointerBase = CPUFetchPC(nes);
            u8 low = CPUBusRead(nes, CPUZeroPageWrap(pointerBase));
            u8 high = CPUBusRead(nes, CPUZeroPageWrap((u8)(pointerBase + 1)));
            u16 baseAddress = ((u16)high << 8) | low;

            operand.address = baseAddress + cpu->y;
            operand.pageCrossed = CPUPageCrossed(baseAddress, operand.address);
            operand.fixupAddress = CPUIndexedFixupAddress(baseAddress, operand.address);
            operand.value = CPUBusRead(nes, operand.fixupAddress);

            if (operand.pageCrossed) {
                operand.value = CPUBusRead(nes, operand.address);
            } else {
                operand.address = operand.fixupAddress;
            }
            break;
        }

        default: {
            ASSERT(false);
            break;
        }
    }

    return operand;
}

// Resolves write-class addressing and performs its dummy reads.
internal CPUOperand CPUResolveWriteTarget(NES* nes, CPUAddressingMode mode)
{
    CPU* cpu = &nes->cpu;
    CPUOperand target = {0};

    switch (mode) {
        case AM_ZPA: {
            target.address = CPUZeroPageWrap(CPUFetchPC(nes));
            break;
        }

        case AM_ZPX: {
            u8 baseAddress = CPUFetchPC(nes);
            CPUDummyRead(nes, CPUZeroPageWrap(baseAddress));
            target.address = CPUZeroPageWrap((u8)(baseAddress + cpu->x));
            break;
        }

        case AM_ZPY: {
            u8 baseAddress = CPUFetchPC(nes);
            CPUDummyRead(nes, CPUZeroPageWrap(baseAddress));
            target.address = CPUZeroPageWrap((u8)(baseAddress + cpu->y));
            break;
        }

        case AM_ABS: {
            target.address = CPUFetchPC16(nes);
            break;
        }

        case AM_ABX: {
            u16 baseAddress = CPUFetchPC16(nes);
            target.address = baseAddress + cpu->x;
            target.pageCrossed = CPUPageCrossed(baseAddress, target.address);
            target.fixupAddress = CPUIndexedFixupAddress(baseAddress, target.address);
            CPUDummyRead(nes, target.fixupAddress);
            break;
        }

        case AM_ABY: {
            u16 baseAddress = CPUFetchPC16(nes);
            target.address = baseAddress + cpu->y;
            target.pageCrossed = CPUPageCrossed(baseAddress, target.address);
            target.fixupAddress = CPUIndexedFixupAddress(baseAddress, target.address);
            CPUDummyRead(nes, target.fixupAddress);
            break;
        }

        case AM_IZX: {
            u8 pointerBase = CPUFetchPC(nes);
            CPUDummyRead(nes, CPUZeroPageWrap(pointerBase));

            u8 pointer = (u8)(pointerBase + cpu->x);
            u8 low = CPUBusRead(nes, CPUZeroPageWrap(pointer));
            u8 high = CPUBusRead(nes, CPUZeroPageWrap((u8)(pointer + 1)));
            target.address = ((u16)high << 8) | low;
            break;
        }

        case AM_IZY: {
            u8 pointerBase = CPUFetchPC(nes);
            u8 low = CPUBusRead(nes, CPUZeroPageWrap(pointerBase));
            u8 high = CPUBusRead(nes, CPUZeroPageWrap((u8)(pointerBase + 1)));
            u16 baseAddress = ((u16)high << 8) | low;

            target.address = baseAddress + cpu->y;
            target.pageCrossed = CPUPageCrossed(baseAddress, target.address);
            target.fixupAddress = CPUIndexedFixupAddress(baseAddress, target.address);
            CPUDummyRead(nes, target.fixupAddress);
            break;
        }

        default: {
            ASSERT(false);
            break;
        }
    }

    return target;
}

// Resolves RMW addressing and fetches the old memory value.
internal CPUOperand CPUFetchRmwOperand(NES* nes, CPUAddressingMode mode)
{
    CPU* cpu = &nes->cpu;
    CPUOperand operand = {0};

    switch (mode) {
        case AM_ZPA: {
            operand.address = CPUZeroPageWrap(CPUFetchPC(nes));
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_ZPX: {
            u8 baseAddress = CPUFetchPC(nes);
            CPUDummyRead(nes, CPUZeroPageWrap(baseAddress));
            operand.address = CPUZeroPageWrap((u8)(baseAddress + cpu->x));
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_ABS: {
            operand.address = CPUFetchPC16(nes);
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_ABX: {
            u16 baseAddress = CPUFetchPC16(nes);
            operand.address = baseAddress + cpu->x;
            operand.pageCrossed = CPUPageCrossed(baseAddress, operand.address);
            operand.fixupAddress = CPUIndexedFixupAddress(baseAddress, operand.address);
            CPUDummyRead(nes, operand.fixupAddress);
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_ABY: {
            u16 baseAddress = CPUFetchPC16(nes);
            operand.address = baseAddress + cpu->y;
            operand.pageCrossed = CPUPageCrossed(baseAddress, operand.address);
            operand.fixupAddress = CPUIndexedFixupAddress(baseAddress, operand.address);
            CPUDummyRead(nes, operand.fixupAddress);
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_IZX: {
            u8 pointerBase = CPUFetchPC(nes);
            CPUDummyRead(nes, CPUZeroPageWrap(pointerBase));

            u8 pointer = (u8)(pointerBase + cpu->x);
            u8 low = CPUBusRead(nes, CPUZeroPageWrap(pointer));
            u8 high = CPUBusRead(nes, CPUZeroPageWrap((u8)(pointer + 1)));

            operand.address = ((u16)high << 8) | low;
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        case AM_IZY: {
            u8 pointerBase = CPUFetchPC(nes);
            u8 low = CPUBusRead(nes, CPUZeroPageWrap(pointerBase));
            u8 high = CPUBusRead(nes, CPUZeroPageWrap((u8)(pointerBase + 1)));
            u16 baseAddress = ((u16)high << 8) | low;

            operand.address = baseAddress + cpu->y;
            operand.pageCrossed = CPUPageCrossed(baseAddress, operand.address);
            operand.fixupAddress = CPUIndexedFixupAddress(baseAddress, operand.address);
            CPUDummyRead(nes, operand.fixupAddress);
            operand.value = CPUBusRead(nes, operand.address);
            break;
        }

        default: {
            ASSERT(false);
            break;
        }
    }

    return operand;
}

// Fetches a signed relative branch offset.
internal s8 CPUFetchRelativeOffset(NES* nes)
{
    return (s8)CPUFetchPC(nes);
}

// Applies branch timing and updates PC when the branch is taken.
internal void CPUBranchRelative(NES* nes, bool take, s8 offset)
{
    CPU* cpu = &nes->cpu;

    if (take) {
        u16 oldPC = cpu->pc;
        u16 newPC = cpu->pc + offset;

        CPUDummyReadPC(nes);

        cpu->pc = newPC;

        if (CPUPageCrossed(oldPC, newPC)) {
            CPUDummyRead(nes, CPUIndexedFixupAddress(oldPC, newPC));
        }
    }
}

// Resolves JMP (ind) with the NMOS page-wrap bug.
internal inline u16 CPUResolveAddressIND(NES* nes)
{
    u16 operand = CPUFetchPC16(nes);
    u16 operand2 = (u16)(operand & 0xFF00) | (u8)(operand + 1);
    u8 low = CPUBusRead(nes, operand);
    u8 high = CPUBusRead(nes, operand2);
    return (high << 8) | low;
}

// Runs the reset entry sequence without pushing CPU state.
internal void HandleResetInterrupt(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);
    CPUDummyReadPC(nes);
    CPUDiscardStackByte(nes);
    CPUDiscardStackByte(nes);
    CPUDiscardStackByte(nes);

    SetInterrupt(cpu, 1);
    cpu->pc = CPUBusRead16(nes, CPU_RESET_ADDRESS);
}

// Runs BRK/NMI/IRQ entry microcode and loads the vector.
internal void HandleInterrupt(NES* nes, u16 interruptAddress, bool fromBRK)
{
    CPU* cpu = &nes->cpu;

    if (fromBRK) {
        CPUFetchPC(nes);
    } else {
        CPUDummyReadPC(nes);
        CPUDummyReadPC(nes);
    }

    CPUPushStack16(nes, cpu->pc);
    CPUPushStack8(nes, CPUComposePForPush(cpu, fromBRK));

    SetInterrupt(cpu, 1);
    cpu->pc = CPUBusRead16(nes, interruptAddress);
}

// Dispatches the currently queued interrupt service.
internal inline void CPUServiceInterrupt(NES* nes)
{
    CPU* cpu = &nes->cpu;

    switch (cpu->pendingService) {
        case CPU_INTERRUPT_RES: {
            HandleResetInterrupt(nes);
            break;
        }
        case CPU_INTERRUPT_NMI: {
            // non-maskable interrupt, always gets executed
            HandleInterrupt(nes, CPU_NMI_ADDRESS, false);
            break;
        }
        case CPU_INTERRUPT_IRQ: {
            if (!GetBitFlag(cpu->p, INTERRUPT_FLAG)) {
                HandleInterrupt(nes, CPU_IRQ_ADDRESS, false);
            }
            break;
        }
        case CPU_INTERRUPT_NON: {
            // do nothing
            break;
        }
    }

    cpu->pendingService = CPU_INTERRUPT_NON;
}

// Adds memory and carry into A.
internal inline void ADC(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u16 acc = cpu->a + operand->value;

    if (GetCarry(cpu)) {
        acc += 1;
    }

    SetCarry(cpu, acc > 0xFF);
    SetNegative(cpu, ISNEG(acc));
    SetZero(cpu, !(acc & 0xFF));
    SetOverflow(cpu, !((cpu->a ^ operand->value) & 0x80) && ((cpu->a ^ acc) & 0x80));

    cpu->a = (u8)acc;
}

// ANDs memory into A.
internal inline void AND(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    data = cpu->a & data;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// Shifts left and updates carry.
internal inline void ASL(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u16 data;

    if (!operand) {
        CPUDummyReadPC(nes);
        data = cpu->a;
    } else {
        data = operand->value;
    }

    u8 original = (u8)data;

    SetCarry(cpu, data & 0x80);

    data = (data << 1) & 0xFF;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    if (!operand) {
        cpu->a = (u8)data;
    } else {
        CPUBusWrite(nes, operand->address, original);
        CPUBusWrite(nes, operand->address, (u8)data);
    }
}

// Shares relative branch timing between branch opcodes.
internal inline void Branch(NES* nes, bool condition, CPUOperand* operand)
{
    CPUBranchRelative(nes, condition, (s8)operand->value);
}

// Branches if carry is clear.
internal inline void BCC(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 C = GetCarry(cpu);
    Branch(nes, !C, operand);
}

// Branches if carry is set.
internal inline void BCS(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 C = GetCarry(cpu);
    Branch(nes, C, operand);
}

// Branches if zero is set.
internal inline void BEQ(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 Z = GetZero(cpu);
    Branch(nes, Z, operand);
}

// Tests bits against A and copies N/V from memory.
internal inline void BIT(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    SetNegative(cpu, HAS_FLAG(data, BIT7_MASK));
    SetOverflow(cpu, HAS_FLAG(data, BIT6_MASK));
    SetZero(cpu, !(cpu->a & data));
}

// Branches if negative is set.
internal inline void BMI(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 N = GetNegative(cpu);
    Branch(nes, N, operand);
}

// Branches if zero is clear.
internal inline void BNE(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 Z = GetZero(cpu);
    Branch(nes, !Z, operand);
}

// Branches if negative is clear.
internal inline void BPL(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 N = GetNegative(cpu);
    Branch(nes, !N, operand);
}

// Forces the IRQ/BRK interrupt sequence.
internal inline void BRK(NES* nes)
{
    HandleInterrupt(nes, CPU_IRQ_ADDRESS, true);
}

// Branches if overflow is clear.
internal inline void BVC(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 V = GetOverflow(cpu);
    Branch(nes, !V, operand);
}

// Branches if overflow is set.
internal inline void BVS(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 V = GetOverflow(cpu);
    Branch(nes, V, operand);
}

// Clears carry.
internal inline void CLC(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);
    SetCarry(cpu, 0);
}

// Clears decimal mode.
internal inline void CLD(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);
    SetDecimal(cpu, 0);
}

// Clears interrupt disable with correct IRQ polling semantics.
internal inline void CLI(NES* nes)
{
    CPU* cpu = &nes->cpu;
    bool oldI = GetInterrupt(cpu);

    CPUDummyReadPC(nes);
    cpu->irqPollIOverrideValid = true;
    cpu->irqPollIOverride = oldI;
    SetInterrupt(cpu, 0);
}

// Clears overflow.
internal inline void CLV(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);
    SetOverflow(cpu, 0);
}

// Compares A with memory.
internal inline void CMP(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    u16 sub = cpu->a - data;

    SetNegative(cpu, ISNEG(sub));
    SetZero(cpu, !sub);
    SetCarry(cpu, sub < 0x100);
}

// Compares X with memory.
internal inline void CPX(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    u16 sub = cpu->x - data;

    SetNegative(cpu, ISNEG(sub));
    SetZero(cpu, !sub);
    SetCarry(cpu, sub < 0x100);
}

// Compares Y with memory.
internal inline void CPY(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    u16 sub = cpu->y - data;

    SetNegative(cpu, ISNEG(sub));
    SetZero(cpu, !sub);
    SetCarry(cpu, sub < 0x100);
}

// Decrements memory in place.
internal inline void DEC(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data = operand->value;

    u8 original = data;
    data -= 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    CPUBusWrite(nes, operand->address, original);
    CPUBusWrite(nes, operand->address, data);
}

// Decrements X.
internal inline void DEX(NES* nes)
{
    CPU* cpu = &nes->cpu;

    u8 data = cpu->x;

    CPUDummyReadPC(nes);
    data -= 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->x = data;
}

// Decrements Y.
internal inline void DEY(NES* nes)
{
    CPU* cpu = &nes->cpu;

    u8 data = cpu->y;

    CPUDummyReadPC(nes);
    data -= 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->y = data;
}

// XORs memory into A.
internal inline void EOR(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    data = cpu->a ^ data;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// Increments memory in place.
internal inline void INC(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data = operand->value;

    u8 original = data;
    data += 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    CPUBusWrite(nes, operand->address, original);
    CPUBusWrite(nes, operand->address, data);
}

// Increments X.
internal inline void INX(NES* nes)
{
    CPU* cpu = &nes->cpu;

    u8 data = cpu->x;

    CPUDummyReadPC(nes);
    data += 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->x = data;
}

// Increments Y.
internal inline void INY(NES* nes)
{
    CPU* cpu = &nes->cpu;

    u8 data = cpu->y;

    CPUDummyReadPC(nes);
    data += 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->y = data;
}

// Jumps to the resolved target address.
internal inline void JMP(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    cpu->pc = operand->address;
}

// Pushes the return address and jumps to a subroutine.
internal inline void JSR(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 low = (u8)operand->address;

    CPUDummyRead(nes, CPUStackAddress(cpu->sp));
    CPUPushStack16(nes, cpu->pc);

    u8 high = CPUFetchPC(nes);
    cpu->pc = ((u16)high << 8) | low;
}

// Loads A from memory.
internal inline void LDA(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// Loads X from memory.
internal inline void LDX(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->x = data;
}

// Loads Y from memory.
internal inline void LDY(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->y = data;
}

// Shifts right and updates carry.
internal inline void LSR(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data;

    if (!operand) {
        CPUDummyReadPC(nes);
        data = cpu->a;
    } else {
        data = operand->value;
    }

    u8 original = data;

    SetCarry(cpu, data & 0x01);

    data >>= 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    if (!operand) {
        cpu->a = data;
    } else {
        CPUBusWrite(nes, operand->address, original);
        CPUBusWrite(nes, operand->address, data);
    }
}

// Burns the instruction's required bus cycles without state changes.
internal inline void NOP(NES* nes, CPUOperand* operand)
{
    if (!operand) {
        CPUDummyReadPC(nes);
    }
}

// ORs memory into A.
internal inline void ORA(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    data |= cpu->a;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// Pushes A onto the stack.
internal inline void PHA(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);
    CPUPushStack8(nes, cpu->a);
}

// Pushes the processor status onto the stack.
internal inline void PHP(NES* nes)
{
    CPUDummyReadPC(nes);
    CPUPushStack8(nes, CPUComposePForPush(&nes->cpu, true));
}

// Pulls A from the stack.
internal inline void PLA(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);
    CPUDummyRead(nes, CPUStackAddress(cpu->sp));
    cpu->a = CPUPopStack8(nes);

    SetNegative(cpu, ISNEG(cpu->a));
    SetZero(cpu, !cpu->a);
}

// Pulls the processor status from the stack.
internal inline void PLP(NES* nes)
{
    CPU* cpu = &nes->cpu;
    bool oldI = GetInterrupt(cpu);

    CPUDummyReadPC(nes);
    CPUDummyRead(nes, CPUStackAddress(cpu->sp));
    u8 v = CPUPopStack8(nes);
    cpu->irqPollIOverrideValid = true;
    cpu->irqPollIOverride = oldI;
    CPURestoreP(cpu, v);
}

// Rotates left through carry.
internal inline void ROL(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data;

    if (!operand) {
        CPUDummyReadPC(nes);
        data = cpu->a;
    } else {
        data = operand->value;
    }

    u8 original = data;

    bool setCarry = data & 0x80;

    data <<= 1;

    if (GetCarry(cpu)) {
        data |= 0x01;
    }

    SetCarry(cpu, setCarry);
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    if (!operand) {
        cpu->a = data;
    } else {
        CPUBusWrite(nes, operand->address, original);
        CPUBusWrite(nes, operand->address, data);
    }
}

// Rotates right through carry.
internal inline void ROR(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data;

    if (!operand) {
        CPUDummyReadPC(nes);
        data = cpu->a;
    } else {
        data = operand->value;
    }

    u8 original = data;

    bool setCarry = data & 0x01;

    data >>= 1;

    if (GetCarry(cpu)) {
        data |= 0x80;
    }

    SetCarry(cpu, setCarry);
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    if (!operand) {
        cpu->a = data;
    } else {
        CPUBusWrite(nes, operand->address, original);
        CPUBusWrite(nes, operand->address, data);
    }
}

// Restores flags and PC from an interrupt frame.
internal inline void RTI(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);
    CPUDummyRead(nes, CPUStackAddress(cpu->sp));
    u8 v = CPUPopStack8(nes);
    CPURestoreP(cpu, v);

    cpu->pc = CPUPopStack16(nes);
}

// Returns from a subroutine.
internal inline void RTS(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);
    CPUDummyRead(nes, CPUStackAddress(cpu->sp));
    cpu->pc = CPUPopStack16(nes);
    CPUDummyRead(nes, cpu->pc);
    cpu->pc += 1;
}

// Subtracts memory and borrow from A.
internal inline void SBC(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    u16 acc = cpu->a - data;

    if (!GetCarry(cpu)) {
        acc -= 1;
    }

    SetCarry(cpu, acc < 0x100);
    SetNegative(cpu, ISNEG(acc));
    SetZero(cpu, !(acc & 0xFF));
    SetOverflow(cpu, ((cpu->a ^ data) & 0x80) && ((cpu->a ^ acc) & 0x80));

    cpu->a = (u8)acc;
}

// Sets carry.
internal inline void SEC(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);
    SetCarry(cpu, 1);
}

// Sets decimal mode.
internal inline void SED(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);
    SetDecimal(cpu, 1);
}

// Sets interrupt disable with correct IRQ polling semantics.
internal inline void SEI(NES* nes)
{
    CPU* cpu = &nes->cpu;
    bool oldI = GetInterrupt(cpu);

    CPUDummyReadPC(nes);
    cpu->irqPollIOverrideValid = true;
    cpu->irqPollIOverride = oldI;
    SetInterrupt(cpu, 1);
}

// Stores A to memory.
internal inline void STA(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    CPUBusWrite(nes, operand->address, cpu->a);
}

// Stores X to memory.
internal inline void STX(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    CPUBusWrite(nes, operand->address, cpu->x);
}

// Stores Y to memory.
internal inline void STY(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    CPUBusWrite(nes, operand->address, cpu->y);
}

// Transfers A to X.
internal inline void TAX(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);

    u8 data = cpu->a;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->x = data;
}

// Transfers A to Y.
internal inline void TAY(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);

    u8 data = cpu->a;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->y = data;
}

// Transfers SP to X.
internal inline void TSX(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);

    u8 data = cpu->sp;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->x = data;
}

// Transfers X to A.
internal inline void TXA(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);

    u8 data = cpu->x;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// Transfers X to SP.
internal inline void TXS(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);

    u8 data = cpu->x;
    cpu->sp = data;
}

// Transfers Y to A.
internal inline void TYA(NES* nes)
{
    CPU* cpu = &nes->cpu;

    CPUDummyReadPC(nes);

    u8 data = cpu->y;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

//
// Illegal or unofficial opcodes
//

// Shifts memory left, then ORs the result into A.
internal inline void SLO(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data = operand->value;

    u8 original = data;

    SetCarry(cpu, data & 0x80);

    data <<= 1;

    CPUBusWrite(nes, operand->address, original);
    CPUBusWrite(nes, operand->address, data);

    data |= cpu->a;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// ANDs into A and copies bit 7 into carry.
internal inline void ANC(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    data = cpu->a & data;

    SetCarry(cpu, data & 0x80);
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// Rotates memory left, then ANDs the result into A.
internal inline void RLA(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data = operand->value;

    u8 original = data;

    bool setCarry = data & 0x80;

    data <<= 1;

    if (GetCarry(cpu)) {
        data |= 0x01;
    }

    u8 acc = cpu->a & data;

    SetCarry(cpu, setCarry);
    SetNegative(cpu, ISNEG(acc));
    SetZero(cpu, !acc);

    CPUBusWrite(nes, operand->address, original);
    CPUBusWrite(nes, operand->address, data);
    cpu->a = acc;
}

// Shifts memory right, then XORs the result into A.
internal inline void SRE(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data = operand->value;

    u8 original = data;

    SetCarry(cpu, data & 0x01);

    data >>= 1;

    CPUBusWrite(nes, operand->address, original);
    CPUBusWrite(nes, operand->address, data);

    data = cpu->a ^ data;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// ANDs into A, then logical-shifts A right.
internal inline void ALR(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    data = cpu->a & data;

    SetCarry(cpu, data & 0x01);

    data >>= 1;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// Rotates memory right, then ADCs the result into A.
internal inline void RRA(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data = operand->value;

    u8 original = data;

    bool setCarry = data & 0x01;

    data >>= 1;

    if (GetCarry(cpu)) {
        data |= 0x80;
    }

    u16 acc = cpu->a + data;

    if (setCarry) {
        acc += 1;
    }

    SetCarry(cpu, acc > 0xFF);
    SetNegative(cpu, ISNEG(acc));
    SetZero(cpu, !(acc & 0xFF));
    SetOverflow(cpu, !((cpu->a ^ data) & 0x80) && ((cpu->a ^ acc) & 0x80));

    CPUBusWrite(nes, operand->address, original);
    CPUBusWrite(nes, operand->address, data);
    cpu->a = (u8)acc;
}

// ANDs into A, then rotates right with ARR flag behavior.
internal inline void ARR(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    data = cpu->a & data;
    data >>= 1;

    if (GetCarry(cpu)) {
        data |= 0x80;
    }

    SetOverflow(cpu, ((data >> 5) ^ (data >> 6)) & 1);
    SetCarry(cpu, data & 0x40);
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// Stores A & X.
internal inline void SAX(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    CPUBusWrite(nes, operand->address, cpu->a & cpu->x);
}

// Compatibility XAA: mixes A, X, and the immediate value.
internal inline void XAA(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    data &= cpu->x;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
}

// Stores A & X with unstable abs-indexed masking.
internal inline void AHX(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 value = cpu->a & cpu->x & CPUUnstableStoreMask(operand);
    CPUBusWrite(nes, CPUUnstableStoreAddress(operand, value), value);
}

// Sets SP to A & X, then stores with unstable masking.
internal inline void TAS(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    cpu->sp = cpu->a & cpu->x;

    u8 value = cpu->sp & CPUUnstableStoreMask(operand);
    CPUBusWrite(nes, CPUUnstableStoreAddress(operand, value), value);
}

// Stores Y with unstable abs,X masking.
internal inline void SHY(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 value = cpu->y & CPUUnstableStoreMask(operand);
    CPUBusWrite(nes, CPUUnstableStoreAddress(operand, value), value);
}

// Stores X with unstable abs,Y masking.
internal inline void SHX(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 value = cpu->x & CPUUnstableStoreMask(operand);
    CPUBusWrite(nes, CPUUnstableStoreAddress(operand, value), value);
}

// Loads the same value into A and X.
internal inline void LAX(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);

    cpu->a = data;
    cpu->x = data;
}

// Loads A, X, and SP from memory masked by SP.
internal inline void LAS(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 data = operand->value;
    data &= cpu->sp;

    cpu->a = data;
    cpu->x = data;
    cpu->sp = data;

    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !data);
}

// Decrements memory, then compares the result with A.
internal inline void DCP(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data = operand->value;

    u8 original = data;

    data -= 1;

    u16 sub = cpu->a - data;

    SetNegative(cpu, ISNEG(sub));
    SetZero(cpu, !sub);
    SetCarry(cpu, sub < 0x100);

    CPUBusWrite(nes, operand->address, original);
    CPUBusWrite(nes, operand->address, data);
}

// Stores (A & X) - value into X and updates flags.
internal inline void AXS(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;
    u8 value = operand->value;
    u16 data = (u16)(cpu->a & cpu->x) - value;

    SetCarry(cpu, data < 0x100);
    SetNegative(cpu, ISNEG(data));
    SetZero(cpu, !(data & 0xFF));

    cpu->x = (u8)data;
}

// Increments memory, then SBCs the result from A.
internal inline void ISB(NES* nes, CPUOperand* operand)
{
    CPU* cpu = &nes->cpu;

    u8 data = operand->value;

    u8 original = data;

    data += 1;

    u16 acc = cpu->a - data;

    if (!GetCarry(cpu)) {
        acc -= 1;
    }

    SetCarry(cpu, acc < 0x100);
    SetNegative(cpu, ISNEG(acc));
    SetZero(cpu, !(acc & 0xFF));
    SetOverflow(cpu, ((cpu->a ^ data) & 0x80) && ((cpu->a ^ acc) & 0x80));

    CPUBusWrite(nes, operand->address, original);
    CPUBusWrite(nes, operand->address, data);
    cpu->a = (u8)acc;
}

// Represents a jammed CPU opcode that should never execute here.
internal inline void KIL(NES* nes)
{
    ASSERT(false);
}

// Placeholder for an internal fake opcode used by tooling.
internal inline void FEX(NES* nes) {}

// Initializes CPU state for power-on.
void PowerCPU(NES* nes)
{
    CPU* cpu = &nes->cpu;
    cpu->a = 0;
    cpu->x = 0;
    cpu->y = 0;
    cpu->cycles = 7;
    cpu->sp = CPU_STACK_PTR_INITIAL_VALUE;
    cpu->p = CPU_STATUS_INITIAL_VALUE;
    cpu->waitCycles = 0;
    cpu->pc = ReadCPUU16(nes, CPU_RESET_ADDRESS);
    cpu->pendingService = CPU_INTERRUPT_NON;
    cpu->nmiLine = false;
    cpu->prevNmiLine = false;
    cpu->nmiPending = false;
    cpu->irqSources = 0;
    cpu->irqPollIOverrideValid = false;
    cpu->irqPollIOverride = false;

    PushStackU8(nes, 0xFF);
    PushStackU8(nes, 0xFF);
}

// Queues a hardware-style reset sequence.
void ResetCPU(NES* nes)
{
    CPU* cpu = &nes->cpu;
    cpu->waitCycles = 0;
    CPURequestReset(nes);
}

// Allocates CPU RAM on first use.
void InitCPU(NES* nes)
{
    if (!nes->cpuMemory.created) {
        CreateMemory(&nes->cpuMemory, CPU_RAM_TOTAL_SIZE);
        ZeroMemoryBytes(&nes->cpuMemory);
    }
}

// Resolves operands, then dispatches one decoded instruction.
internal void ExecuteInstruction(NES* nes, CPUInstruction* instruction)
{
    ASSERT(instruction->mnemonic != CPU_KIL);

    CPUOperand operand = {0};

    switch (instruction->execClass) {
        case CPU_EXEC_READ:
        case CPU_EXEC_NOP_READ: {
            operand = CPUFetchReadOperand(nes, instruction->addressingMode);
            break;
        }

        case CPU_EXEC_WRITE: {
            operand = CPUResolveWriteTarget(nes, instruction->addressingMode);
            break;
        }

        case CPU_EXEC_RMW: {
            if (instruction->addressingMode != AM_ACC) {
                operand = CPUFetchRmwOperand(nes, instruction->addressingMode);
            }
            break;
        }

        case CPU_EXEC_BRANCH: {
            operand.value = (u8)CPUFetchRelativeOffset(nes);
            break;
        }

        case CPU_EXEC_JUMP: {
            if (instruction->addressingMode == AM_ABS) {
                if (instruction->mnemonic == CPU_JSR) {
                    operand.address = CPUFetchPC(nes);
                } else {
                    operand.address = CPUFetchPC16(nes);
                }
            } else if (instruction->addressingMode == AM_IND) {
                operand.address = CPUResolveAddressIND(nes);
            }
            break;
        }

        case CPU_EXEC_SPECIAL: {
            if (instruction->mnemonic != CPU_BRK) {
                ASSERT(instruction->addressingMode == AM_NON);
            }
            break;
        }

        case CPU_EXEC_IMPLIED:
        case CPU_EXEC_ACCUMULATOR:
        case CPU_EXEC_STACK: {
            break;
        }

        default: {
            ASSERT(false);
            break;
        }
    }

    switch (instruction->mnemonic) {
        case CPU_ADC: {
            ADC(nes, &operand);
            break;
        }
        case CPU_AND: {
            AND(nes, &operand);
            break;
        }
        case CPU_ASL: {
            ASL(nes, instruction->addressingMode == AM_ACC ? NULL : &operand);
            break;
        }
        case CPU_BCC: {
            BCC(nes, &operand);
            break;
        }
        case CPU_BCS: {
            BCS(nes, &operand);
            break;
        }
        case CPU_BEQ: {
            BEQ(nes, &operand);
            break;
        }
        case CPU_BIT: {
            BIT(nes, &operand);
            break;
        }
        case CPU_BMI: {
            BMI(nes, &operand);
            break;
        }
        case CPU_BNE: {
            BNE(nes, &operand);
            break;
        }
        case CPU_BPL: {
            BPL(nes, &operand);
            break;
        }
        case CPU_BRK: {
            BRK(nes);
            break;
        }
        case CPU_BVC: {
            BVC(nes, &operand);
            break;
        }
        case CPU_BVS: {
            BVS(nes, &operand);
            break;
        }
        case CPU_CLC: {
            CLC(nes);
            break;
        }
        case CPU_CLD: {
            CLD(nes);
            break;
        }
        case CPU_CLI: {
            CLI(nes);
            break;
        }
        case CPU_CLV: {
            CLV(nes);
            break;
        }
        case CPU_CMP: {
            CMP(nes, &operand);
            break;
        }
        case CPU_CPX: {
            CPX(nes, &operand);
            break;
        }
        case CPU_CPY: {
            CPY(nes, &operand);
            break;
        }
        case CPU_DEC: {
            DEC(nes, &operand);
            break;
        }
        case CPU_DEX: {
            DEX(nes);
            break;
        }
        case CPU_DEY: {
            DEY(nes);
            break;
        }
        case CPU_EOR: {
            EOR(nes, &operand);
            break;
        }
        case CPU_INC: {
            INC(nes, &operand);
            break;
        }
        case CPU_INX: {
            INX(nes);
            break;
        }
        case CPU_INY: {
            INY(nes);
            break;
        }
        case CPU_JMP: {
            JMP(nes, &operand);
            break;
        }
        case CPU_JSR: {
            JSR(nes, &operand);
            break;
        }
        case CPU_LDA: {
            LDA(nes, &operand);
            break;
        }
        case CPU_LDX: {
            LDX(nes, &operand);
            break;
        }
        case CPU_LDY: {
            LDY(nes, &operand);
            break;
        }
        case CPU_LSR: {
            LSR(nes, instruction->addressingMode == AM_ACC ? NULL : &operand);
            break;
        }
        case CPU_NOP: {
            NOP(nes, instruction->execClass == CPU_EXEC_NOP_READ ? &operand : NULL);
            break;
        }
        case CPU_ORA: {
            ORA(nes, &operand);
            break;
        }
        case CPU_PHA: {
            PHA(nes);
            break;
        }
        case CPU_PHP: {
            PHP(nes);
            break;
        }
        case CPU_PLA: {
            PLA(nes);
            break;
        }
        case CPU_PLP: {
            PLP(nes);
            break;
        }
        case CPU_ROL: {
            ROL(nes, instruction->addressingMode == AM_ACC ? NULL : &operand);
            break;
        }
        case CPU_ROR: {
            ROR(nes, instruction->addressingMode == AM_ACC ? NULL : &operand);
            break;
        }
        case CPU_RTI: {
            RTI(nes);
            break;
        }
        case CPU_RTS: {
            RTS(nes);
            break;
        }
        case CPU_SBC: {
            SBC(nes, &operand);
            break;
        }
        case CPU_SEC: {
            SEC(nes);
            break;
        }
        case CPU_SED: {
            SED(nes);
            break;
        }
        case CPU_SEI: {
            SEI(nes);
            break;
        }
        case CPU_STA: {
            STA(nes, &operand);
            break;
        }
        case CPU_STX: {
            STX(nes, &operand);
            break;
        }
        case CPU_STY: {
            STY(nes, &operand);
            break;
        }
        case CPU_TAX: {
            TAX(nes);
            break;
        }
        case CPU_TAY: {
            TAY(nes);
            break;
        }
        case CPU_TSX: {
            TSX(nes);
            break;
        }
        case CPU_TXA: {
            TXA(nes);
            break;
        }
        case CPU_TXS: {
            TXS(nes);
            break;
        }
        case CPU_TYA: {
            TYA(nes);
            break;
        }
        case CPU_SLO: {
            SLO(nes, &operand);
            break;
        }
        case CPU_ANC: {
            ANC(nes, &operand);
            break;
        }
        case CPU_RLA: {
            RLA(nes, &operand);
            break;
        }
        case CPU_SRE: {
            SRE(nes, &operand);
            break;
        }
        case CPU_ALR: {
            ALR(nes, &operand);
            break;
        }
        case CPU_RRA: {
            RRA(nes, &operand);
            break;
        }
        case CPU_ARR: {
            ARR(nes, &operand);
            break;
        }
        case CPU_SAX: {
            SAX(nes, &operand);
            break;
        }
        case CPU_XAA: {
            XAA(nes, &operand);
            break;
        }
        case CPU_AHX: {
            AHX(nes, &operand);
            break;
        }
        case CPU_TAS: {
            TAS(nes, &operand);
            break;
        }
        case CPU_SHY: {
            SHY(nes, &operand);
            break;
        }
        case CPU_SHX: {
            SHX(nes, &operand);
            break;
        }
        case CPU_LAX: {
            LAX(nes, &operand);
            break;
        }
        case CPU_LAS: {
            LAS(nes, &operand);
            break;
        }
        case CPU_DCP: {
            DCP(nes, &operand);
            break;
        }
        case CPU_AXS: {
            AXS(nes, &operand);
            break;
        }
        case CPU_ISB: {
            ISB(nes, &operand);
            break;
        }
        case CPU_KIL: {
            KIL(nes);
            break;
        }
        case CPU_FEX: {
            FEX(nes);
            break;
        }
        default: {
            break;
        }
    }
}

// Requests reset service on the next CPU step.
void CPURequestReset(NES* nes)
{
    CPU* cpu = &nes->cpu;
    cpu->pendingService = CPU_INTERRUPT_RES;
}

// Updates the sampled NMI line level.
void CPUSetNMILine(NES* nes, bool asserted)
{
    nes->cpu.nmiLine = asserted;
}

// Raises or clears one IRQ source bit.
void CPUSetIRQSource(NES* nes, u32 sourceMask, bool asserted)
{
    CPU* cpu = &nes->cpu;

    if (asserted) {
        cpu->irqSources |= sourceMask;
    } else {
        cpu->irqSources &= ~sourceMask;
    }
}

// Executes one CPU instruction or stall cycle.
CPUStep StepCPU(NES* nes)
{
    CPUStep step = {0};

    CPU* cpu = &nes->cpu;
    u64 startCpuCycles = cpu->cycles;

    if (cpu->waitCycles) {
        CPUStallCycle(nes);

        step.cycles = 1;
        step.instruction = NULL;
        return step;
    }

    CPUPollInterrupts(nes);

    if (cpu->pendingService != CPU_INTERRUPT_NON) {
        CPUServiceInterrupt(nes);

        step.cycles = cpu->cycles - startCpuCycles;
        step.instruction = NULL;
        return step;
    }

    u8 opcode = CPUFetchPC(nes);
    CPUInstruction* instruction = &cpuInstructions[opcode];
    ExecuteInstruction(nes, instruction);

    step.cycles = cpu->cycles - startCpuCycles;
    step.instruction = instruction;
    return step;
}
