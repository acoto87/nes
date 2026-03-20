#include <string.h>
#include "cpu_trace.h"
#include "cpu.h"
#include "cpu_debug.h"

internal inline bool IsUnofficialOpcode(u8 opcode)
{
    CPUInstruction* inst = &cpuInstructions[opcode];
    if (inst->mnemonic == CPU_NOP && opcode != 0xEA) return true;
    if (inst->mnemonic == CPU_SBC && opcode == 0xEB) return true;
    if (inst->mnemonic >= CPU_SLO && inst->mnemonic <= CPU_FEX) return true;
    return false;
}

void LogCPUState(NES* nes, FILE* logFile)
{
    CPU* cpu = &nes->cpu;
    u16 pc = cpu->pc;
    u8 opcode = PeekCPUU8(nes, pc);
    CPUInstruction* inst = &cpuInstructions[opcode];

    char hexBytes[10] = {0};
    if (inst->bytesCount == 1) {
        snprintf(hexBytes, sizeof(hexBytes), "%02X      ", opcode);
    } else if (inst->bytesCount == 2) {
        u8 op1 = PeekCPUU8(nes, pc + 1);
        snprintf(hexBytes, sizeof(hexBytes), "%02X %02X   ", opcode, op1);
    } else if (inst->bytesCount == 3) {
        u8 op1 = PeekCPUU8(nes, pc + 1);
        u8 op2 = PeekCPUU8(nes, pc + 2);
        snprintf(hexBytes, sizeof(hexBytes), "%02X %02X %02X", opcode, op1, op2);
    } else {
        snprintf(hexBytes, sizeof(hexBytes), "%02X      ", opcode);
    }

    const char* mnemonic = GetInstructionStr(inst->mnemonic);
    char prefix[2] = {0};
    if (IsUnofficialOpcode(opcode)) {
        prefix[0] = '*';
        prefix[1] = '\0';
    } else {
        prefix[0] = ' ';
        prefix[1] = '\0';
    }

    char asmStr[32] = {0};
    u16 address;
    u8 op1, op2;
    u8 val1;
    switch (inst->addressingMode) {
        case AM_IMP:
        case AM_NON:
            snprintf(asmStr, sizeof(asmStr), "%s%s", prefix, mnemonic);
            break;
        case AM_ACC:
            snprintf(asmStr, sizeof(asmStr), "%s%s A", prefix, mnemonic);
            break;
        case AM_IMM:
            op1 = PeekCPUU8(nes, pc + 1);
            snprintf(asmStr, sizeof(asmStr), "%s%s #$%02X", prefix, mnemonic, op1);
            break;
        case AM_ZPA:
            op1 = PeekCPUU8(nes, pc + 1);
            val1 = PeekCPUU8(nes, op1);
            snprintf(asmStr, sizeof(asmStr), "%s%s $%02X = %02X", prefix, mnemonic, op1, val1);
            break;
        case AM_ZPX:
            op1 = PeekCPUU8(nes, pc + 1);
            address = (u8)(op1 + cpu->x);
            val1 = PeekCPUU8(nes, address);
            snprintf(asmStr, sizeof(asmStr), "%s%s $%02X,X @ %02X = %02X", prefix, mnemonic, op1, address, val1);
            break;
        case AM_ZPY:
            op1 = PeekCPUU8(nes, pc + 1);
            address = (u8)(op1 + cpu->y);
            val1 = PeekCPUU8(nes, address);
            snprintf(asmStr, sizeof(asmStr), "%s%s $%02X,Y @ %02X = %02X", prefix, mnemonic, op1, address, val1);
            break;
        case AM_ABS:
            op1 = PeekCPUU8(nes, pc + 1);
            op2 = PeekCPUU8(nes, pc + 2);
            address = (op2 << 8) | op1;
            if (inst->mnemonic == CPU_JMP || inst->mnemonic == CPU_JSR) {
                snprintf(asmStr, sizeof(asmStr), "%s%s $%04X", prefix, mnemonic, address);
            } else {
                val1 = PeekCPUU8(nes, address);
                snprintf(asmStr, sizeof(asmStr), "%s%s $%04X = %02X", prefix, mnemonic, address, val1);
            }
            break;
        case AM_ABX:
            op1 = PeekCPUU8(nes, pc + 1);
            op2 = PeekCPUU8(nes, pc + 2);
            address = (op2 << 8) | op1;
            u16 resolvedX = address + cpu->x;
            val1 = PeekCPUU8(nes, resolvedX);
            snprintf(asmStr, sizeof(asmStr), "%s%s $%04X,X @ %04X = %02X", prefix, mnemonic, address, resolvedX, val1);
            break;
        case AM_ABY:
            op1 = PeekCPUU8(nes, pc + 1);
            op2 = PeekCPUU8(nes, pc + 2);
            address = (op2 << 8) | op1;
            u16 resolvedY = address + cpu->y;
            val1 = PeekCPUU8(nes, resolvedY);
            snprintf(asmStr, sizeof(asmStr), "%s%s $%04X,Y @ %04X = %02X", prefix, mnemonic, address, resolvedY, val1);
            break;
        case AM_IND:
            op1 = PeekCPUU8(nes, pc + 1);
            op2 = PeekCPUU8(nes, pc + 2);
            address = (op2 << 8) | op1;
            u16 jmpAddr;
            if (op1 == 0xFF) {
                jmpAddr = (PeekCPUU8(nes, address & 0xFF00) << 8) | PeekCPUU8(nes, address);
            } else {
                jmpAddr = (PeekCPUU8(nes, address + 1) << 8) | PeekCPUU8(nes, address);
            }
            snprintf(asmStr, sizeof(asmStr), "%s%s ($%04X) = %04X", prefix, mnemonic, address, jmpAddr);
            break;
        case AM_IZX:
            op1 = PeekCPUU8(nes, pc + 1);
            u8 ptrX = (u8)(op1 + cpu->x);
            address = (PeekCPUU8(nes, (u8)(ptrX + 1)) << 8) | PeekCPUU8(nes, ptrX);
            val1 = PeekCPUU8(nes, address);
            snprintf(asmStr, sizeof(asmStr), "%s%s ($%02X,X) @ %02X = %04X = %02X", prefix, mnemonic, op1, ptrX, address, val1);
            break;
        case AM_IZY:
            op1 = PeekCPUU8(nes, pc + 1);
            u16 ptrY = (PeekCPUU8(nes, (u8)(op1 + 1)) << 8) | PeekCPUU8(nes, op1);
            address = ptrY + cpu->y;
            val1 = PeekCPUU8(nes, address);
            snprintf(asmStr, sizeof(asmStr), "%s%s ($%02X),Y = %04X @ %04X = %02X", prefix, mnemonic, op1, ptrY, address, val1);
            break;
        case AM_REL:
            op1 = PeekCPUU8(nes, pc + 1);
            address = pc + 2 + (s8)op1;
            snprintf(asmStr, sizeof(asmStr), "%s%s $%04X", prefix, mnemonic, address);
            break;
        default:
            snprintf(asmStr, sizeof(asmStr), "%s%s", prefix, mnemonic);
            break;
    }

    char leftSide[64];
    snprintf(leftSide, sizeof(leftSide), "%04X  %s %s", pc, hexBytes, asmStr);

    fprintf(logFile, "%-47s A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:%llu\n", leftSide, cpu->a, cpu->x,
            cpu->y, cpu->p, cpu->sp, nes->ppu.scanline, nes->ppu.cycle, cpu->cycles);
}
