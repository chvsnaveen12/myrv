#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const uint64_t  DRAM_SIZE = 1024 * 1024 * 128;
const uint64_t  DRAM_BASE = 0x80000000;
const uint64_t  DRAM_END = DRAM_BASE+DRAM_SIZE-1;

uint64_t instruction_file_size;

struct CPU {
    uint64_t regs[32];
    uint64_t pc;
    uint8_t* dram;
} CPU;


// ==============DRAM=====================================================================================================
uint8_t* dram_init(char* file_name){
    FILE* instruction_file;
    instruction_file = fopen(file_name,"r");
    
    if(instruction_file == NULL){
        printf("FILE DOESN'T EXIST\n");
        exit(0);
    }

    uint8_t* instruction_buffer;
    fseek(instruction_file, 0L, SEEK_END);
    instruction_file_size = ftell(instruction_file);
    rewind(instruction_file);

    instruction_buffer = (uint8_t*)malloc(DRAM_SIZE * sizeof(uint8_t));
    if(instruction_buffer == NULL){
        printf("MEMORY WASN'T ALLOCATED FOR INSTRUCTIONS\n");
        exit(0);
    }
    fread(instruction_buffer, 1, instruction_file_size, instruction_file);
    fclose(instruction_file);
    return instruction_buffer;
}

uint64_t dram_load(uint64_t address, uint8_t size){
    uint64_t physical_address = address - DRAM_BASE;
    switch(size){
        case 8:
            return (uint64_t)*(uint8_t*)(CPU.dram+physical_address);
            break;
        
        case 16:
            return (uint64_t)*(uint16_t*)(CPU.dram+physical_address);
            break;
        
        case 32:
            return (uint64_t)*(uint32_t*)(CPU.dram+physical_address);
            break;
        
        case 64:
            return (uint64_t)*(uint64_t*)(CPU.dram+physical_address);
            break;
        
        default:
            printf("INVALID LOAD LENGTH FROM DRAM, dram");
            exit(0);
            break;

    }
}

void dram_store(uint64_t address, uint8_t size, uint64_t value){
    uint64_t physical_address = address - DRAM_BASE;
    switch(size){
        case 8:
            *(uint8_t*)(CPU.dram+physical_address) = (uint8_t)value;
            break;
        
        case 16:
            *(uint16_t*)(CPU.dram+physical_address) = (uint16_t)value;
            break;
        
        case 32:
            *(uint32_t*)(CPU.dram+physical_address) = (uint32_t)value;
            break;
        
        case 64:
            *(uint64_t*)(CPU.dram+physical_address) = (uint64_t)value;
            break;
        
        default:
            printf("INVALID STORE LENGTH FROM DRAM, dram");
            exit(0);
            break;

    }
}
// =======================================================================================================================


// ==============BUS======================================================================================================
uint64_t bus_load(uint64_t address, uint8_t size){
    if (address >= DRAM_BASE) {
            return dram_load(address, size);
        }
    else{
        printf("LOAD ADDRESS ERROR, bus");
        exit(0);
    }
}

void bus_store(uint64_t address, uint8_t size, uint64_t value){
    if (address >= DRAM_BASE) {
            return dram_store(address, size, value);
        }
    else{
        printf("STORE ADDRESS ERROR, bus");
        exit(0);
    }
}
// =======================================================================================================================


// ==============CPU======================================================================================================
uint32_t cpu_fetch(){
    return (uint32_t)bus_load(CPU.pc, 32);
}

void cpu_execute(uint32_t instruction){
    uint8_t funct3 = (uint8_t)(instruction >> 12) & 0x07;
    uint8_t opcode = (uint8_t)instruction & 0b01111111;
    uint8_t rd = (uint8_t)(instruction >> 7) & 0b00011111;
    uint8_t rs1 = (uint8_t)(instruction >> 15) & 0b00011111;
    uint8_t rs2 = (uint8_t)(instruction >> 20) & 0b00011111;

    // ALL ARE FOR RV32I UNLESS MENTIONED
    if(opcode == 0x37){
        // LUI
        uint64_t imm = ((uint64_t)instruction) & 0x00000000fffff000;
        imm = (imm & 0x0000000080000000) >> 31 ? (imm | 0xffffffff00000000) : imm;
        CPU.regs[rd] = imm;
    }
    
    else if(opcode == 0x17){
        // AUIPC
        uint64_t imm = ((uint64_t)instruction) & 0x00000000fffff000;
        imm = (imm & 0x0000000080000000) >> 31 ? (imm | 0xffffffff00000000) : imm;
        CPU.regs[rd] = CPU.pc + imm;
    }

    else if(opcode == 0x6f){
        // JAL
        uint64_t imm = (((uint64_t)instruction >> 20) & 0x00000000000007fe)
                        | (((uint64_t)instruction >>  9) & 0x0000000000000800)
                        | (((uint64_t)instruction)       & 0x00000000000ff000)
                        | (((uint64_t)instruction >> 11) & 0x0000000000100000);
        imm = (imm & 0x0000000000100000) >> 20 ? (imm | 0xffffffffffe00000) : imm;
        CPU.regs[rd] = CPU.pc + 4;
        CPU.pc = CPU.pc + imm - 4;
    }

    else if(opcode == 0x67){
        // JALR
        uint8_t rd = (uint8_t)(instruction >> 7) & 0x1f;
        uint8_t rs1 = (uint8_t)(instruction >> 15) & 0x1f;
        uint64_t imm = ((uint64_t)instruction >> 20) & 0x0000000000000fff;
        imm = (imm & 0x0000000000000800) >> 11 ? (imm | 0xfffffffffffff000) : imm;

        uint64_t temp = CPU.pc+4;
        CPU.pc = ((CPU.regs[rs1] + imm) & 0xfffffffffffffffe);
        CPU.regs[rd] = temp;
    }

    else if(opcode == 0x63){
        // BEQ BNE BLT BGE BLTU BGEU
        uint64_t imm = (((uint64_t)instruction >>  7) & 0x000000000000001e)
                        | (((uint64_t)instruction >> 10) & 0x00000000000007e0)
                        | (((uint64_t)instruction <<  4) & 0x0000000000000800)
                        | (((uint64_t)instruction >> 19) & 0x0000000000001000);
        imm = (imm & 0x0000000000001000) >> 12 ? (imm | 0xffffffffffffe000) : imm;

        if(funct3 == 0)
            // BEQ
            if(CPU.regs[rs1] == CPU.regs[rs2])
                CPU.pc = CPU.pc + imm - 4;
        
        else if(funct3 == 1)
            // BNE
            if(CPU.regs[rs1] != CPU.regs[rs2])
                CPU.pc = CPU.pc + imm - 4;

        else if(funct3 == 4)
            // BLT
            if((int64_t)CPU.regs[rs1] < (int64_t)CPU.regs[rs2])
                CPU.pc = CPU.pc + imm - 4;

        else if(funct3 == 5)
            // BGE
            if((int64_t)CPU.regs[rs1] > (int64_t)CPU.regs[rs2])
                CPU.pc = CPU.pc + imm - 4;

        else if(funct3 == 6)
            // BLTU
            if((uint64_t)CPU.regs[rs1] < (uint64_t)CPU.regs[rs2])
                CPU.pc = CPU.pc + imm - 4;

        else if(funct3 == 7)
            // BGEU
            if((uint64_t)CPU.regs[rs1] > (uint64_t)CPU.regs[rs2])
                CPU.pc = CPU.pc + imm - 4;
    }

    else if(opcode == 0x03){
        // LB LH LW LBU LHU (RV64I => LWU LD)
        uint64_t imm = (instruction >> 20) & 0x0000000000000fff;
        imm = (imm & 0x0000000000001000) >> 11 ? (imm | 0xfffffffffffff000) : imm;

        if(funct3 == 0){
            // LB
            uint64_t val = bus_load(CPU.regs[rs1] + imm, 8);
            CPU.regs[rd] = (val & 0x0000000000000080) >> 7 ? (val | 0xffffffffffffff00) : val;     
        }

        else if(funct3 == 1){
            // LH
            uint64_t val = bus_load(CPU.regs[rs1] + imm, 16);
            CPU.regs[rd] = (val & 0x0000000000008000) >> 15 ? (val | 0xffffffffffff0000) : val;     
        }

        else if(funct3 == 2){
            // LW
            uint64_t val = bus_load(CPU.regs[rs1] + imm, 32);
            CPU.regs[rd] = (val & 0x0000000080000000) >> 31 ? (val | 0xffffffff00000000) : val;     
        }

        else if(funct3 == 4)
            // LBU
            CPU.regs[rd] = bus_load(CPU.regs[rs1] + imm, 8);

        else if(funct3 == 5)
            // LHU
            CPU.regs[rd] = bus_load(CPU.regs[rs1] + imm, 16);
        
        else if(funct3 == 6)
            // LWU
            CPU.regs[rd] = bus_load(CPU.regs[rs1] + imm, 32);

        else if(funct3 == 3)
            // LD
            CPU.regs[rd] = bus_load(CPU.regs[rs1] + imm, 64);

    }

    else if(opcode == 0x23){
        // SB SH SW (RV64I => SD)
        uint64_t imm = ((instruction >> 7) & 0x000000000000001f)
                        | ((instruction >> 20) & 0x0000000000000fe0);
        imm = (imm & 0x0000000000000800) >> 11 ? (imm | 0xfffffffffffff000) : imm;

        if(funct3 == 0)
            // SB
            bus_store(CPU.regs[rs1] + imm, 8, rs2);
        
        if(funct3 == 1)
            // SH
            bus_store(CPU.regs[rs1] + imm, 16, rs2);
        
        if(funct3 == 2)
            // SW
            bus_store(CPU.regs[rs1] + imm, 32, rs2);
        
        if(funct3 == 3)
            // SD
            bus_store(CPU.regs[rs1] + imm, 64, rs2);

    }

    else if(opcode == 0x13){
        // ADDI SLTI SLTIU XORI ORI ANDI
        // (RV64I => SLLI SRLI SRAI)
        uint64_t imm = (instruction >> 20) & 0x0000000000000fff;
        imm = (imm & 0x0000000000000800) >> 11 ? (imm | 0xfffffffffffff000) : imm;

        if(funct3 == 0)
            // ADDI
            CPU.regs[rd] = CPU.regs[rs1] + imm;

        else if(funct3 == 2)
            // SLTI
            CPU.regs[rd] = (int64_t)CPU.regs[rs1] < (int64_t)imm ? 1 : 0;

        else if(funct3 == 3)
            // SLTIU
            CPU.regs[rd] = (uint64_t)CPU.regs[rs1] < (uint64_t)imm ? 1 : 0;

        else if(funct3 == 4)
            // XORI
            CPU.regs[rd] = CPU.regs[rs1] ^ imm;

        else if(funct3 == 6)
            // ORI
            CPU.regs[rd] = CPU.regs[rs1] | imm;
        
        else if(funct3 == 7)
            // ANDI
            CPU.regs[rd] = CPU.regs[rs1] & imm;
        
        else if(funct3 == 1)
            //  SLLI
            CPU.regs[rd] = CPU.regs[rs1] << ((uint8_t)imm & 0x2f);

        else if(funct3 == 5){
            //  SRLI SRAI
            if((instruction >> 30) & 1)
                // SRAI
                CPU.regs[rd] = (CPU.regs[rs1] >> ((uint8_t)imm & 0x3f)) | (CPU.regs[rs1] & 0x8000000000000000);
            else
                // SRLI
                CPU.regs[rd] = (CPU.regs[rs1] >> ((uint8_t)imm & 0x3f));
        }
    }
    
    else if(opcode == 0x33){
        // ADD SUB SLL SLT SLTU XOR SRL SRA OR AND
        if(funct3 == 0)
            // ADD SUB
            CPU.regs[rd] = (instruction >> 30) & 1 ? CPU.regs[rs1] - CPU.regs[rs2] : CPU.regs[rs1] + CPU.regs[rs2];
        
        else if(funct3 == 1)
            // SLL
            CPU.regs[rd] = CPU.regs[rs1] << (CPU.regs[rs2] & 0x1f);
        
        else if(funct3 == 2)
            // SLT
            CPU.regs[rd] = (int64_t)CPU.regs[rs1] < (int64_t)CPU.regs[rs2] ? 1 : 0;
        
        else if(funct3 == 3)
            // SLTU
            CPU.regs[rd] = (uint64_t)CPU.regs[rs1] < (uint64_t)CPU.regs[rs2] ? 1 : 0;
        
        else if(funct3 == 4)
            // XOR
            CPU.regs[rd] = CPU.regs[rs1] ^ CPU.regs[rs2];
        
        else if(funct3 == 5){
            // SRA SRL
            uint64_t result = CPU.regs[rs1] >> (CPU.regs[rs2] & 0x1f);
            result |= (instruction >> 30) & 1 ? (CPU.regs[rs1] & 0x8000000000000000) : 0;
            CPU.regs[rd] = result;
        }

        else if(funct3 == 6)
            // AND
            CPU.regs[rd] = CPU.regs[rs1] & CPU.regs[rs2];

        else if(funct3 == 7)
            // OR
            CPU.regs[rd] = CPU.regs[rs1] | CPU.regs[rs2];
    }

    // else if(opcode == 0x0f){
    //     // FENCE
    // }

    // else if(opcode == 0x73){
    //     // ECALL EBREAK
    // }

    else if(opcode == 0x1b){
        // (RV64I => ADDIW SLLIW SRLIW SRAIW)
        uint64_t result;
        if(funct3 == 0){
            // ADDIW
            uint64_t imm = (instruction >> 20) & 0x0000000000000fff;
            imm |= (imm >> 11) & 1 ? 0xfffffffffffff000 : 0;
            result = (uint32_t)(CPU.regs[rs1] + imm);
            CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
        }

        else if(funct3 == 1){
            // SLLIW
            result = (uint32_t)(CPU.regs[rs1] << rs2);
            CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
        }

        else if(funct3 == 5){
            // SRLIW SRAIW
            result = (uint32_t)CPU.regs[rs1] >> rs2;
            result |= (instruction >> 30) & 1 ? (CPU.regs[rs1] & 0x0000000080000000) : 0;
            CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
        }
    }

    else if(opcode == 0x3b){
        // (RV64I => ADDW SUBW SLLW SRLW SRAW)
        uint64_t result;
        if(funct3 == 0){
            // ADDW SUBW
            result = (uint32_t)((instruction >> 30) & 1 ? CPU.regs[rs1] - CPU.regs[rs2] : CPU.regs[rs1] + CPU.regs[rs2]);
            CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
        }

        else if(funct3 == 1){
            // SLLW
            result = (uint32_t)(CPU.regs[rs1] << (CPU.regs[rs2] & 0x1f));
            CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
        }

        else if(funct3 == 5){
            // SRLW SRAW
            result = (uint32_t)CPU.regs[rs1] >> (CPU.regs[rs2] & 0x1f);
            result |= (instruction >> 30) & 1 ? CPU.regs[rs1] & 0x0000000080000000 : 0;
            CPU.regs[rd] = (result >> 31) & 1 ? (result | 0xffffffff00000000) : result;
        }

    }
    
    else{
        printf("INVALID INSTRUCTION          ");
        exit(0);
    }
    CPU.regs[0] = 0;
}

void cpu_regdump(){
    printf("PC: 0x%016lx\n\n", CPU.pc);

    for (int i = 0; i < 16; i++){
        printf("rs%02d: 0x%016lx                rs%02d: 0x%016lx\n", i, CPU.regs[i], i+16, CPU.regs[i+16]);
    }
}
// =======================================================================================================================


int main(int argc, char *argv[]){
    
    if(argc != 2){
        printf("GIVE INPUT FILE, main\n");
        exit(0);
    }
    
    //Set things up
    for(int i = 0; i < 32; i++)
        CPU.regs[i] = 0;
    CPU.regs[2] =  DRAM_END;
    CPU.pc = DRAM_BASE;
    CPU.dram = dram_init(argv[1]);
    
    // Start with the Fetch Decode Execute cycle
    while (1){
        uint32_t inst = cpu_fetch();
        printf("inst: 0x%08x\n", inst);
        cpu_execute(inst);
        cpu_regdump();
        sleep(5);
        CPU.pc += 4;
    }
    
    return 0;
}