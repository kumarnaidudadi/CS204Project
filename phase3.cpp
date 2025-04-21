#include <iostream>     
#include <fstream>      
#include <sstream>      
#include <iomanip>      
#include <string>       
#include <map>          
#include <unordered_map>
#include <vector>       
#include <cstdint>      
#include <cstdlib>      
#include <stdexcept>     
#include <algorithm>    
#include <cstdio>       
#include <sstream> 
#include <bits/stdc++.h>
using namespace std;

static const string NOP_INSTRUCTION = "NOP"; 
static const uint64_t INITIAL_SP = 0x0;             
        

static string formatHex(uint64_t value) {
    ostringstream oss;
    oss << "0x" << uppercase << hex << setw(8) << setfill('0') << value;
    return oss.str();
}

uint64_t parseHex(const string &hex) {
    if (hex.size() < 3 || hex.rfind("0x", 0) != 0) {
        cerr << "Warning: Invalid hex string format: " << hex << endl;
        return 0;
    }
    return stoull(hex.substr(2), nullptr, 16);
}

int64_t signExtend(const string &binary, int bitWidth) {
    uint64_t value = stoull(binary, nullptr, 2);
    uint64_t mask = 1ULL << (bitWidth - 1);
    if (value & mask) {
        uint64_t signBits = ~0ULL << bitWidth;
        value |= signBits;
    }
    return static_cast<int64_t>(value);
}

class PipelinedCPU {
    public:
        struct IFIDRegister{
            uint64_t instructionPC = 0;
            string instruction = NOP_INSTRUCTION;
            uint64_t nextPC = 0;
            uint64_t instructionNumber = 0;
            bool valid = false;
            bool predictedTaken = false;
            uint64_t predictedTarget = 0;
        
            string toString() const {
                ostringstream oss;
                oss << boolalpha;
                oss << "IF/ID [Valid:" << valid << "]: PC=" << formatHex(instructionPC)
                    << ", IR=" << instruction
                    << ", NextPC=" << formatHex(nextPC)
                    << ", Inst#:" << instructionNumber
                    << ", PredTaken:" << predictedTaken
                    << ", PredTarget=" << formatHex(predictedTarget);
                return oss.str();
            }
        
            void clear() {
                instruction = NOP_INSTRUCTION;
                valid = false;
                instructionNumber = 0;
                predictedTaken = false;
                predictedTarget = 0;
            }
        };
        struct IDEXRegister{
            // Control Signals
            string aluOp = "NOP";
            bool regWrite = false;
            bool memRead = false;
            bool memWrite = false;
            bool branch = false;
            bool jump = false;
            bool useImm = false;
            int writeBackMux = 0;
            string memSize = "WORD";
        
            // Data
            uint64_t instructionPC = 0;
            uint64_t nextPC = 0;
            uint64_t readData1 = 0;
            uint64_t readData2 = 0;
            uint64_t immediate = 0;
            int rs1 = 0;
            int rs2 = 0;
            int rd = 0;
            string debugInstruction = NOP_INSTRUCTION;
            uint64_t instructionNumber = 0;
            bool valid = false;
        
            string toString() const {
                ostringstream oss;
                oss << boolalpha;
                oss << "ID/EX [Valid:" << valid << "]: PC=" << formatHex(instructionPC)
                    << ", Inst#:" << instructionNumber
                    << ", Ctrl:[" << aluOp << ",RW:" << regWrite
                    << ",MR:" << memRead << ",MW:" << memWrite
                    << ",Br:" << branch << ",Jmp:" << jump
                    << ",Imm:" << useImm << ",WBMux:" << writeBackMux
                    << ",Sz:" << memSize << "]"
                    << ", RVal1=" << formatHex(readData1)
                    << ", RVal2=" << formatHex(readData2)
                    << ", Imm=" << formatHex(immediate)
                    << ", rs1=x" << rs1 << ", rs2=x" << rs2
                    << ", rd=x" << rd
                    << ", IR=" << debugInstruction;
                return oss.str();
            }
        
            void clear() {
                aluOp = "NOP";
                regWrite = memRead = memWrite = branch = jump = useImm = false;
                writeBackMux = 0;
                memSize = "WORD";
                valid = false;
                instructionNumber = 0;
                debugInstruction = NOP_INSTRUCTION;
            }
        };
        struct EXMEMRegister{
            bool regWrite = false;
            bool memRead = false;
            bool memWrite = false;
            bool branchTaken = false;
            string debugInstruction = NOP_INSTRUCTION;
            int writeBackMux = 0;
            string memSize = "WORD";
        
            uint64_t instructionPC = 0;
            uint64_t aluResult = 0;
            uint64_t writeData = 0;
            uint64_t branchTarget = 0;
            int rd = 0;
            bool valid = false;
            uint64_t instructionNumber = 0;
        
            string toString() const {
                ostringstream oss;
                oss << boolalpha;
                oss << "EX/MEM [Valid:" << valid << "]: Inst#:" << instructionNumber
                    << ", Ctrl:[RW:" << regWrite << ",MR:" << memRead
                    << ",MW:" << memWrite << ",BrTaken:" << branchTaken
                    << ",WBMux:" << writeBackMux << ",Sz:" << memSize << "]"
                    << ", ALURes=" << formatHex(aluResult)
                    << ", WriteData=" << formatHex(writeData)
                    << ", BrTarget=" << formatHex(branchTarget)
                    << ", rd=x" << rd;
                return oss.str();
            }
        
            void clear() {
                regWrite = memRead = memWrite = branchTaken = false;
                valid = false;
                instructionNumber = 0;
            }
        };
        struct MEMWBRegister{
            bool regWrite = false;
            int writeBackMux = 0;
        
            uint64_t instructionPC = 0;
            uint64_t aluResult = 0;
            uint64_t readData = 0;
            int rd = 0;
            string debugInstruction = NOP_INSTRUCTION;
            bool valid = false;
            uint64_t instructionNumber = 0;
        
            string toString() const {
                ostringstream oss;
                oss << boolalpha;
                oss << "MEM/WB [Valid:" << valid << "]: Inst#:" << instructionNumber
                    << ", Ctrl:[RW:" << regWrite << ",WBMux:" << writeBackMux << "]"
                    << ", ALURes=" << formatHex(aluResult)
                    << ", ReadData=" << formatHex(readData)
                    << ", rd=x" << rd;
                return oss.str();
            }
        
            void clear() {
                regWrite = false;
                valid = false;
                instructionNumber = 0;
            }
        };
        class BranchPredictor{
            public:
                bool predictTaken(uint64_t pc) {
                    ++predictions;
                    return historyTable[pc] == 1;
                }
            
                bool isInBTB(uint64_t pc) const {
                    return targetBuffer.find(pc) != targetBuffer.end();
                }
            
                uint64_t getPredictedTarget(uint64_t pc) {
                    if (isInBTB(pc)) {
                        ++btbHits;
                        return targetBuffer[pc];
                    } else {
                        ++btbMisses;
                        return pc + 4;
                    }
                }
            
                void update(uint64_t pc, bool actuallyTaken, uint64_t actualTarget) {
                    bool wasTaken = (historyTable[pc] == 1);
                    if (wasTaken != actuallyTaken) {
                        ++mispredictions;
                    }
                    historyTable[pc] = actuallyTaken ? 1 : 0;
                    if (actuallyTaken) {
                        targetBuffer[pc] = actualTarget;
                    }
                }
            
                string toString() const {
                    ostringstream oss;
                    oss << "BPU State: Predictions=" << predictions
                        << ", Mispredictions=" << mispredictions
                        << ", BTB Hits=" << btbHits
                        << ", BTB Misses=" << btbMisses << "\n";
                    oss << " History Table (PC -> State[0=NT,1=T]):\n";
                    if (historyTable.empty()) {
                        oss << "  <Empty>\n";
                    } else {
                        map<uint64_t,int> sorted(historyTable.begin(), historyTable.end());
                        for (auto &e : sorted) {
                            oss << "  " << formatHex(e.first) << " -> " << e.second << "\n";
                        }
                    }
                    oss << " Branch Target Buffer (PC -> Target):\n";
                    if (targetBuffer.empty()) {
                        oss << "  <Empty>\n";
                    } else {
                        map<uint64_t,uint64_t> sortedT(targetBuffer.begin(), targetBuffer.end());
                        for (auto &e : sortedT) {
                            oss << "  " << formatHex(e.first) << " -> " << formatHex(e.second) << "\n";
                        }
                    }
                    return oss.str();
                }
            
            private:
                unordered_map<uint64_t,int> historyTable;
                unordered_map<uint64_t,uint64_t> targetBuffer;
                uint64_t predictions = 0;
                uint64_t mispredictions = 0;
                uint64_t btbHits = 0;
                uint64_t btbMisses = 0;
            };
        
        unordered_map<string, string> registerFile;
        map<uint64_t, string> textSegment;
        map<uint64_t, uint8_t> dataMemory;
    
        BranchPredictor bpu;
    
        IFIDRegister if_id_reg;
        IDEXRegister id_ex_reg;
        EXMEMRegister ex_mem_reg, ex_debug;
        MEMWBRegister mem_wb_reg;
    
        uint64_t pc;
        uint64_t clockCycle;
        uint64_t instructionCount;
        bool hazardStall;
        bool dataForwardingStall;
        bool branchMispredictFlush;
    
        bool pipeliningEnabled;
        bool dataForwardingEnabled;
        bool printRegistersEnabled;
        bool printPipelineRegsEnabled;
        bool printBPUEnabled;
        int traceInstructionNum;
        
        static const uint64_t INITIAL_SP = 0x7FFFFFDC;


        PipelinedCPU(){
            // --- Configuration Knobs ---
            pipeliningEnabled = true;
            dataForwardingEnabled = true;
            printRegistersEnabled = false;
            printPipelineRegsEnabled = true;
            printBPUEnabled = false;
            traceInstructionNum = -1;
        
            // --- Core Components ---
            for (int i = 0; i < 32; ++i) {
                registerFile["x" + to_string(i)] = formatHex(0);
            }
            registerFile["x2"] = formatHex(INITIAL_SP); // Initialize Stack Pointer (sp)
        
            pc = 0x0;
            clockCycle = 0;
            instructionCount = 0;
            hazardStall = false;
            dataForwardingStall = false;
            branchMispredictFlush = false;
        
            // Pipeline Registers
            if_id_reg = {};
            id_ex_reg = {};
            ex_mem_reg = {};
            mem_wb_reg = {};
        
            // Branch Predictor
            bpu = BranchPredictor();
        
            // Optional: initialize memory maps if needed
            textSegment.clear();
            dataMemory.clear();
        };
        
        void instructionFetch(){
            if (hazardStall || dataForwardingStall) {
                // If stalled, do not fetch a new instruction
                return;
            }
        
            if (branchMispredictFlush) {
                // If flushing due to misprediction, the PC has already been updated
                // Just clear the flush flag and continue with the new PC
                branchMispredictFlush = false;
            }
        
            // Fetch next instruction (or NOP)
            string instructionHex = textSegment.count(pc) ? textSegment[pc] : NOP_INSTRUCTION;
            uint64_t currentPC   = pc;
            uint64_t pcPlus4     = pc + 4;
        
            // Decode opcode to see if this might be a branch/jump
            bool mightBeBranch = false;
            uint64_t instructionVal = parseHex(instructionHex);
            int opcode = static_cast<int>(instructionVal & 0x7F);  // bits [6:0]
        
            if (opcode == 0b1100011  // B‑Type (branches)
             || opcode == 0b1101111  // J‑Type (JAL)
             || opcode == 0b1100111) // I‑Type (JALR)
            {
                mightBeBranch = true;
            }
        
            // Branch prediction defaults
            uint64_t predictedNextPC = pcPlus4;
            bool predictedTaken      = false;
        
            // If in BTB, use BTB + 1‑bit predictor
            if (bpu.isInBTB(currentPC)) {
                predictedTaken = bpu.predictTaken(currentPC);
                if (predictedTaken) {
                    predictedNextPC = bpu.getPredictedTarget(currentPC);
                    if (printBPUEnabled) {
                        std::cout << "BPU: Predicting branch at 0x"
                                  << std::hex << std::uppercase << currentPC
                                  << " as TAKEN to 0x" << predictedNextPC << " (BTB hit)" << std::endl;
                    }
                }
                else if (printBPUEnabled) {
                    std::cout << "BPU: Predicting branch at 0x"
                              << std::uppercase << std::setfill('0') << std::setw(8)
                              << std::hex << currentPC
                              << " as NOT TAKEN (BTB hit but not taken)" << std::endl;
                }
        
            // If looks like a branch but not in BTB, still predict
            } else if (mightBeBranch) {
                predictedTaken = bpu.predictTaken(currentPC);
                if (predictedTaken) {
                    if (printBPUEnabled) {
                        std::cout << "BPU: Predicting branch at 0x"
                                  << std::uppercase << std::setfill('0') << std::setw(8)
                                  << std::hex << currentPC
                                  << " as TAKEN but target unknown (BTB miss)" << std::endl;
                    }
                    predictedNextPC = pcPlus4;  // will be corrected in EX stage
                }
                else if (printBPUEnabled) {
                    std::cout << "BPU: Predicting branch at 0x"
                              << std::uppercase << std::setfill('0') << std::setw(8)
                              << std::hex << currentPC
                              << " as NOT TAKEN" << std::endl;
                }
            }
        
            // Fill IF/ID register
            if_id_reg.instructionPC    = currentPC;
            if_id_reg.instruction      = instructionHex;
            if_id_reg.nextPC          = pcPlus4;              // for link-address
            if_id_reg.valid           = true;
            if_id_reg.instructionNumber = instructionCount++;
            if_id_reg.predictedTaken  = predictedTaken;
            if_id_reg.predictedTarget = predictedNextPC;
        
            // Update PC based on prediction
            pc = predictedTaken ? predictedNextPC : pcPlus4;
        }        

        void instructionDecode() {
            if (!if_id_reg.valid) {
                id_ex_reg.clear();  // Pass NOP downstream
                return;
            }

            // Take values from IF/ID register
            string instruction  = if_id_reg.instruction;
            uint64_t   instructionPC = if_id_reg.instructionPC;
            uint64_t   nextPC        = if_id_reg.nextPC;

            // Clear for new instruction
            id_ex_reg.clear();
            id_ex_reg.instructionPC    = instructionPC;
            id_ex_reg.nextPC          = nextPC;
            id_ex_reg.debugInstruction = instruction;
            id_ex_reg.instructionNumber = if_id_reg.instructionNumber;
            id_ex_reg.valid           = true;

            // NOP?
            if (instruction == NOP_INSTRUCTION) {
                id_ex_reg.aluOp = "NOP";
                return;
            }

            // Decode fields
            uint64_t instructionVal = parseHex(instruction);
            int      opcode         = static_cast<int>(instructionVal & 0x7F);
            id_ex_reg.rd  = static_cast<int>((instructionVal >>  7) & 0x1F);
            int funct3    = static_cast<int>((instructionVal >> 12) & 0x7);
            id_ex_reg.rs1 = static_cast<int>((instructionVal >> 15) & 0x1F);
            id_ex_reg.rs2 = static_cast<int>((instructionVal >> 20) & 0x1F);
            int funct7    = static_cast<int>((instructionVal >> 25) & 0x7F);

            // Read register operands
            string rs1Name = "x" + to_string(id_ex_reg.rs1);
            string rs2Name = "x" + to_string(id_ex_reg.rs2);
            id_ex_reg.readData1 = parseHex(
                registerFile.count(rs1Name) ? registerFile[rs1Name] : formatHex(0));
            id_ex_reg.readData2 = parseHex(
                registerFile.count(rs2Name) ? registerFile[rs2Name] : formatHex(0));

            // Default control signals
            id_ex_reg.regWrite   = false;
            id_ex_reg.memRead    = false;
            id_ex_reg.memWrite   = false;
            id_ex_reg.branch     = false;
            id_ex_reg.jump       = false;
            id_ex_reg.useImm     = false;
            id_ex_reg.writeBackMux = 0;
            id_ex_reg.memSize    = "WORD";

            switch (opcode) {
            // R‑Type
            case 0b0110011:
                id_ex_reg.aluOp   = decodeRType(funct3, funct7);
                id_ex_reg.regWrite = true;
                break;

            // I‑Type arithmetic
            case 0b0010011:
                id_ex_reg.immediate = signExtend(
                    bitset<32>(instructionVal >> 20).to_string(), 12);
                id_ex_reg.aluOp   = decodeITypeArith(funct3, funct7);
                id_ex_reg.useImm = true;
                id_ex_reg.regWrite = true;
                break;

            // I‑Type load
            case 0b0000011: {
                id_ex_reg.immediate = signExtend(
                    bitset<32>(instructionVal >> 20).to_string(), 12);
                id_ex_reg.aluOp    = "ADD";
                id_ex_reg.memRead  = true;
                id_ex_reg.useImm   = true;
                id_ex_reg.regWrite = true;
                id_ex_reg.writeBackMux = 1;
                switch (funct3) {
                case 0b000: id_ex_reg.memSize = "BYTE"; break;
                case 0b001: id_ex_reg.memSize = "HALF"; break;
                case 0b010: id_ex_reg.memSize = "WORD"; break;
                default:    id_ex_reg.aluOp   = "INVALID"; break;
                }
                break;
            }

            // I‑Type JALR
            case 0b1100111:
                id_ex_reg.immediate   = signExtend(
                    bitset<32>(instructionVal >> 20).to_string(), 12);
                id_ex_reg.aluOp       = "JALR";
                id_ex_reg.useImm      = true;
                id_ex_reg.regWrite    = true;
                id_ex_reg.jump        = true;
                id_ex_reg.writeBackMux = 2;
                break;

            // S‑Type store
            case 0b0100011: {
                uint64_t imm4_0  = (instructionVal >>  7) & 0x1F;
                uint64_t imm11_5 = (instructionVal >> 25) & 0x7F;
                uint64_t immSVal = (imm11_5 << 5) | imm4_0;
                id_ex_reg.immediate = signExtend(
                    bitset<32>(immSVal).to_string(), 12);
                id_ex_reg.aluOp     = "ADD";
                id_ex_reg.memWrite  = true;
                id_ex_reg.useImm    = true;
                switch (funct3) {
                case 0b000: id_ex_reg.memSize = "BYTE"; break;
                case 0b001: id_ex_reg.memSize = "HALF"; break;
                case 0b010: id_ex_reg.memSize = "WORD"; break;
                default:    id_ex_reg.aluOp   = "INVALID"; break;
                }
                break;
            }

            // B‑Type branch
            case 0b1100011: {
                uint64_t imm11   = (instructionVal >>  7) & 0x1;
                uint64_t imm4_1  = (instructionVal >>  8) & 0xF;
                uint64_t imm10_5 = (instructionVal >> 25) & 0x3F;
                uint64_t imm12   = (instructionVal >> 31) & 0x1;
                uint64_t immBVal = (imm12 << 12) | (imm11 << 11)
                                | (imm10_5 << 5) | (imm4_1 << 1);
                id_ex_reg.immediate = signExtend(
                    bitset<13>(immBVal).to_string(), 13);
                id_ex_reg.aluOp    = decodeBType(funct3);
                id_ex_reg.branch   = true;
                break;
            }

            // U‑Type LUI
            case 0b0110111:
                id_ex_reg.immediate = signExtend(
                    bitset<32>(instructionVal & 0xFFFFF000).to_string(), 32);
                id_ex_reg.aluOp   = "LUI";
                id_ex_reg.useImm  = true;
                id_ex_reg.regWrite = true;
                break;

            // U‑Type AUIPC
            case 0b0010111:
                id_ex_reg.immediate = signExtend(
                    bitset<32>(instructionVal & 0xFFFFF000).to_string(), 32);
                id_ex_reg.aluOp   = "AUIPC";
                id_ex_reg.useImm  = true;
                id_ex_reg.regWrite = true;
                break;

            // J‑Type JAL
            case 0b1101111: {
                uint64_t imm20   = (instructionVal >> 31) & 0x1;
                uint64_t imm10_1 = (instructionVal >> 21) & 0x3FF;
                uint64_t imm11   = (instructionVal >> 20) & 0x1;
                uint64_t imm19_12= (instructionVal >> 12) & 0xFF;
                uint64_t immJVal = (imm20 << 20) | (imm19_12 << 12)
                                | (imm11 << 11) | (imm10_1 << 1);
                id_ex_reg.immediate = signExtend(
                    bitset<21>(immJVal).to_string(), 21);
                id_ex_reg.aluOp     = "JAL";
                id_ex_reg.regWrite  = true;
                id_ex_reg.jump      = true;
                id_ex_reg.writeBackMux = 2;
                break;
            }

            default:
                cerr << "Error: Unsupported opcode "
                        << bitset<7>(opcode)
                        << " at PC " << formatHex(instructionPC)
                        << endl;
                id_ex_reg.aluOp = "INVALID";
                id_ex_reg.valid = false;
                break;
            }

            // Hazard detection
            if (pipeliningEnabled) {
                detectAndHandleHazards();
            }

            // Data‑forwarding stall check
            dataForwardingStall = false;
            if (!dataForwardingEnabled && id_ex_reg.aluOp != "NOP") {
                int rs1 = id_ex_reg.rs1, rs2 = id_ex_reg.rs2;
                bool rs1Needed = true;
                bool rs2Needed = !id_ex_reg.useImm;
                bool storeNeedsRs2 = id_ex_reg.memWrite;

                if (ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd != 0) {
                    if ((rs1Needed && ex_mem_reg.rd == rs1) ||
                        ((rs2Needed || storeNeedsRs2) && ex_mem_reg.rd == rs2)) {
                        dataForwardingStall = true;
                    }
                }
                if (mem_wb_reg.valid && mem_wb_reg.regWrite && mem_wb_reg.rd != 0) {
                    if ((rs1Needed && mem_wb_reg.rd == rs1 &&
                        !(ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd == rs1)) ||
                        ((rs2Needed || storeNeedsRs2) && mem_wb_reg.rd == rs2 &&
                        !(ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd == rs2))) {
                        dataForwardingStall = true;
                    }
                }
            }

            // Stall => convert to NOP; else consume IF/ID
            if (hazardStall || dataForwardingStall) {
                id_ex_reg.clear();
                id_ex_reg.valid = true;
            } else {
                if_id_reg.valid = false;
            }
        }
        string decodeRType(int funct3, int funct7) {
            switch (funct3) {
                case 0b000:
                    return (funct7 == 0b0000000) ? "ADD"
                        : (funct7 == 0b0100000) ? "SUB"
                        : (funct7 == 0b0000001) ? "MUL"
                        : "INVALID";
                case 0b001:
                    return (funct7 == 0b0000000) ? "SLL"
                        : (funct7 == 0b0000001) ? "MULH"
                        : "INVALID";
                case 0b010:
                    return (funct7 == 0b0000000) ? "SLT"
                        : (funct7 == 0b0000001) ? "MULHSU"
                        : "INVALID";
                case 0b011:
                    return (funct7 == 0b0000000) ? "SLTU"
                        : (funct7 == 0b0000001) ? "MULHU"
                        : "INVALID";
                case 0b100:
                    return (funct7 == 0b0000000) ? "XOR"
                        : (funct7 == 0b0000001) ? "DIV"
                        : "INVALID";
                case 0b101:
                    return (funct7 == 0b0000000) ? "SRL"
                        : (funct7 == 0b0100000) ? "SRA"
                        : (funct7 == 0b0000001) ? "DIVU"
                        : "INVALID";
                case 0b110:
                    return (funct7 == 0b0000000) ? "OR"
                        : (funct7 == 0b0000001) ? "REM"
                        : "INVALID";
                case 0b111:
                    return (funct7 == 0b0000000) ? "AND"
                        : (funct7 == 0b0000001) ? "REMU"
                        : "INVALID";
                default:
                    return "INVALID";
            }
        }
        string decodeITypeArith(int funct3, int funct7) {
            switch (funct3) {
                case 0b000:
                    return "ADDI";
                case 0b010:
                    return "SLTI";
                case 0b011:
                    return "SLTIU";
                case 0b100:
                    return "XORI";
                case 0b110:
                    return "ORI";
                case 0b111:
                    return "ANDI";
                case 0b001:
                    return "SLLI";
                case 0b101:
                    return (funct7 == 0b0000000) ? "SRLI"
                        : (funct7 == 0b0100000) ? "SRAI"
                        : "INVALID";
                default:
                    return "INVALID";
            }
        }
        string decodeBType(int funct3) {
            switch (funct3) {
                case 0b000: return "BEQ";
                case 0b001: return "BNE";
                case 0b100: return "BLT";
                case 0b101: return "BGE";
                case 0b110: return "BLTU";
                case 0b111: return "BGEU";
                default:    return "INVALID";
            }
        }

        void execute() {
            // Backup previous EX/MEM for debugging
            ex_debug = ex_mem_reg;
        
            if (!id_ex_reg.valid) {
                ex_mem_reg.clear();
                return;
            }
        
            // --- Forwarding Logic ---
            uint64_t operand1   = id_ex_reg.readData1;
            uint64_t operand2   = id_ex_reg.useImm ? id_ex_reg.immediate : id_ex_reg.readData2;
            int      sourceReg1 = id_ex_reg.rs1;
            int      sourceReg2 = id_ex_reg.rs2;
        
            if (pipeliningEnabled && dataForwardingEnabled) {
                // EX/MEM hazard
                if (ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd != 0) {
                    if (ex_mem_reg.rd == sourceReg1) {
                        operand1 = ex_mem_reg.aluResult;
                    }
                    if (!id_ex_reg.useImm && !id_ex_reg.memWrite && ex_mem_reg.rd == sourceReg2) {
                        operand2 = ex_mem_reg.aluResult;
                    }
                    if (id_ex_reg.memWrite && ex_mem_reg.rd == sourceReg2) {
                        id_ex_reg.readData2 = ex_mem_reg.aluResult;
                    }
                }
                // MEM/WB hazard
                if (mem_wb_reg.valid && mem_wb_reg.regWrite && mem_wb_reg.rd != 0) {
                    uint64_t wbData = (mem_wb_reg.writeBackMux == 1)
                                      ? mem_wb_reg.readData
                                      : mem_wb_reg.aluResult;
                    if (mem_wb_reg.rd == sourceReg1
                        && !(ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd == sourceReg1)) {
                        operand1 = wbData;
                    }
                    if (!id_ex_reg.useImm && !id_ex_reg.memWrite && mem_wb_reg.rd == sourceReg2
                        && !(ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd == sourceReg2)) {
                        operand2 = wbData;
                    }
                    if (id_ex_reg.memWrite && mem_wb_reg.rd == sourceReg2
                        && !(ex_mem_reg.valid && ex_mem_reg.regWrite && ex_mem_reg.rd == sourceReg2)) {
                        id_ex_reg.readData2 = wbData;
                    }
                }
            }
        
            // Init EX/MEM
            ex_mem_reg.clear();
            ex_mem_reg.valid             = true;
            ex_mem_reg.instructionNumber = id_ex_reg.instructionNumber;
            ex_mem_reg.instructionPC     = id_ex_reg.instructionPC;
        
            // --- ALU Execution ---
            uint64_t aluResult         = 0;
            bool     branchConditionMet = false;
            uint64_t branchTarget      = 0;
            uint64_t linkAddress       = id_ex_reg.nextPC;  // PC+4
        
            if      (id_ex_reg.aluOp == "ADDI") {   aluResult = operand1 + operand2; }
            else if (id_ex_reg.aluOp == "SUB") {    aluResult = operand1 - operand2; }
            else if (id_ex_reg.aluOp == "MUL") {    aluResult = operand1 * operand2; }
            else if (id_ex_reg.aluOp == "XOR" ||
                     id_ex_reg.aluOp == "XORI") {   aluResult = operand1 ^ operand2; }
            else if (id_ex_reg.aluOp == "OR"  ||
                     id_ex_reg.aluOp == "ORI") {    aluResult = operand1 | operand2; }
            else if (id_ex_reg.aluOp == "AND" ||
                     id_ex_reg.aluOp == "ANDI") {   aluResult = operand1 & operand2; }
            else if (id_ex_reg.aluOp == "SLL" ||
                     id_ex_reg.aluOp == "SLLI") {   aluResult = operand1 << (operand2 & 0x1F); }
            else if (id_ex_reg.aluOp == "SRL" ||
                     id_ex_reg.aluOp == "SRLI") {   aluResult = operand1 >> (operand2 & 0x1F); }
            else if (id_ex_reg.aluOp == "SRA" ||
                     id_ex_reg.aluOp == "SRAI") {   aluResult = static_cast<int64_t>(operand1) >> (operand2 & 0x1F); }
            else if (id_ex_reg.aluOp == "SLT" ||
                     id_ex_reg.aluOp == "SLTI") {   aluResult = (static_cast<int64_t>(operand1) < static_cast<int64_t>(operand2)) ? 1 : 0; }
            else if (id_ex_reg.aluOp == "SLTU" ||
                     id_ex_reg.aluOp == "SLTIU") {  aluResult = (operand1 < operand2) ? 1 : 0; }
            else if (id_ex_reg.aluOp == "LOAD_ADDR" ||
                     id_ex_reg.aluOp == "STORE_ADDR" ||
                     id_ex_reg.aluOp == "ADD") {     aluResult = operand1 + operand2; }
            else if (id_ex_reg.aluOp == "BEQ") {
                branchConditionMet = (operand1 == operand2);
            }
            else if (id_ex_reg.aluOp == "BNE") {
                branchConditionMet = (operand1 != operand2);
            }
            else if (id_ex_reg.aluOp == "BLT") {
                branchConditionMet = (static_cast<int64_t>(operand1) < static_cast<int64_t>(operand2));
            }
            else if (id_ex_reg.aluOp == "BGE") {
                branchConditionMet = (static_cast<int64_t>(operand1) >= static_cast<int64_t>(operand2));
            }
            else if (id_ex_reg.aluOp == "BLTU") {
                branchConditionMet = (operand1 < operand2);
            }
            else if (id_ex_reg.aluOp == "BGEU") {
                branchConditionMet = (operand1 >= operand2);
            }
            else if (id_ex_reg.aluOp == "JAL") {
                aluResult          = linkAddress;
                branchTarget       = id_ex_reg.instructionPC + id_ex_reg.immediate;
                branchConditionMet = true;
            }
            else if (id_ex_reg.aluOp == "JALR") {
                aluResult          = linkAddress;
                branchTarget       = (operand1 + id_ex_reg.immediate) & ~1ULL;
                branchConditionMet = true;
            }
            else if (id_ex_reg.aluOp == "LUI") {
                aluResult = id_ex_reg.immediate;
            }
            else if (id_ex_reg.aluOp == "AUIPC") {
                aluResult = id_ex_reg.instructionPC + id_ex_reg.immediate;
            }
            else if (id_ex_reg.aluOp == "NOP") {
                // no-op
            }
            else if (id_ex_reg.aluOp == "INVALID") {
                cerr << "Executing INVALID operation!\n";
            }
            else {
                cerr << "Unknown ALU operation in EX: " << id_ex_reg.aluOp << "\n";
            }
        
            // --- Branch / Jump Handling ---
            if (id_ex_reg.branch || id_ex_reg.jump) {
                if (id_ex_reg.branch) {
                    branchTarget = id_ex_reg.instructionPC + id_ex_reg.immediate;
                    ex_mem_reg.branchTaken = branchConditionMet;
                    bool predictedTaken = pipeliningEnabled
                                          ? bpu.predictTaken(id_ex_reg.instructionPC)
                                          : if_id_reg.predictedTaken;
                    uint64_t predictedTarget = pipeliningEnabled
                                               ? bpu.getPredictedTarget(id_ex_reg.instructionPC)
                                               : if_id_reg.predictedTarget;
                    bpu.update(id_ex_reg.instructionPC, branchConditionMet, branchTarget);
        
                    bool targetMismatch = branchConditionMet && (predictedTarget != branchTarget);
                    if (predictedTaken != branchConditionMet || targetMismatch) {
                        branchMispredictFlush = true;
                        pc = branchConditionMet ? branchTarget : id_ex_reg.nextPC;
                        if (printBPUEnabled || printPipelineRegsEnabled) {
                            if (targetMismatch) {
                                std::cout << "BRANCH TARGET MISPREDICT at 0x"
                                          << std::uppercase << std::setfill('0') << std::setw(8)
                                          << std::hex << id_ex_reg.instructionPC
                                          << ": Predicted 0x" << std::setw(8) << predictedTarget
                                          << ", Actual 0x" << std::setw(8) << branchTarget
                                          << ". Correcting PC." << std::endl;
                            } else {
                                std::cout << "BRANCH DIRECTION MISPREDICT at 0x"
                                          << std::uppercase << std::setfill('0') << std::setw(8)
                                          << std::hex << id_ex_reg.instructionPC
                                          << ": Predicted " << (predictedTaken ? "TAKEN" : "NOT TAKEN")
                                          << ", Actual " << (branchConditionMet ? "TAKEN" : "NOT TAKEN")
                                          << ". Correcting PC to 0x" << std::setw(8) << pc << std::endl;
                            }
                        }
                    }
                } else {
                    // JAL or JALR
                    if (id_ex_reg.aluOp == "JAL") {
                        branchTarget = id_ex_reg.instructionPC + id_ex_reg.immediate;
                    } else {
                        branchTarget = (operand1 + id_ex_reg.immediate) & ~1ULL;
                    }
                    ex_mem_reg.branchTaken = true;
                    uint64_t predictedTarget = pipeliningEnabled
                                               ? bpu.getPredictedTarget(id_ex_reg.instructionPC)
                                               : if_id_reg.predictedTarget;
                    bpu.update(id_ex_reg.instructionPC, true, branchTarget);
        
                    if (predictedTarget != branchTarget) {
                        branchMispredictFlush = true;
                        pc = branchTarget;
                        if (printBPUEnabled || printPipelineRegsEnabled) {
                            std::cout << "JUMP TARGET MISPREDICT at 0x"
                                      << std::uppercase << std::setfill('0') << std::setw(8)
                                      << std::hex << id_ex_reg.instructionPC
                                      << ": Predicted 0x" << std::setw(8) << predictedTarget
                                      << ", Actual 0x" << std::setw(8) << branchTarget
                                      << ". Correcting PC." << std::endl;
                        }
                    }
                }
                ex_mem_reg.branchTarget = branchTarget;
            }
        
            // --- Write EX/MEM Register ---
            ex_mem_reg.aluResult   = aluResult;
            ex_mem_reg.writeData   = id_ex_reg.memWrite ? id_ex_reg.readData2 : operand2;
            ex_mem_reg.rd          = id_ex_reg.rd;
            ex_mem_reg.regWrite    = id_ex_reg.regWrite;
            ex_mem_reg.memRead     = id_ex_reg.memRead;
            ex_mem_reg.memWrite    = id_ex_reg.memWrite;
            ex_mem_reg.debugInstruction = id_ex_reg.debugInstruction;
            ex_mem_reg.writeBackMux     = id_ex_reg.writeBackMux;
            ex_mem_reg.memSize          = id_ex_reg.memSize;
        
            // Consume ID/EX
            id_ex_reg.valid = false;
        }
        
        void memoryAccess() {
            if (!ex_mem_reg.valid) {
                mem_wb_reg.clear();
                return;
            }
            mem_wb_reg.clear();
            mem_wb_reg.valid             = true;
            mem_wb_reg.debugInstruction  = ex_mem_reg.debugInstruction;
            mem_wb_reg.instructionNumber = ex_mem_reg.instructionNumber;
            mem_wb_reg.instructionPC     = ex_mem_reg.instructionPC;
        
            uint64_t addr      = ex_mem_reg.aluResult;
            uint64_t writeData = ex_mem_reg.writeData;
            uint64_t readDataResult = 0;
        
            if (ex_mem_reg.memRead) {
                readDataResult = readMemory(addr, ex_mem_reg.memSize);
                if (printPipelineRegsEnabled ||
                    (traceInstructionNum != -1 && traceInstructionNum == mem_wb_reg.instructionNumber)) {
                    std::cout << "      MEM: Read " << ex_mem_reg.memSize
                              << " from 0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << addr
                              << ", Value=0x" << std::setw(8) << readDataResult << std::endl;
                }
            } 
            else if (ex_mem_reg.memWrite) {
                writeMemory(addr, writeData, ex_mem_reg.memSize);
                if (printPipelineRegsEnabled ||
                    (traceInstructionNum != -1 && traceInstructionNum == mem_wb_reg.instructionNumber)) {
                    std::cout << "      MEM: Wrote " << ex_mem_reg.memSize
                              << " to 0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << addr
                              << ", Value=0x" << std::setw(8) << writeData << std::endl;
                }
            }
        
            // Prepare MEM/WB register
            mem_wb_reg.aluResult   = ex_mem_reg.aluResult;
            mem_wb_reg.readData    = readDataResult;
            mem_wb_reg.rd          = ex_mem_reg.rd;
            mem_wb_reg.regWrite    = ex_mem_reg.regWrite;
            mem_wb_reg.writeBackMux= ex_mem_reg.writeBackMux;
        }
        
        void writeBack() {
            if (!mem_wb_reg.valid || !mem_wb_reg.regWrite || mem_wb_reg.rd == 0) {
                mem_wb_reg.valid = false;
                return;
            }
        
            uint64_t writeData;
            switch (mem_wb_reg.writeBackMux) {
                case 0:  // ALU result
                case 2:  // Link address
                    writeData = mem_wb_reg.aluResult;
                    break;
                case 1:  // Load data
                    writeData = mem_wb_reg.readData;
                    break;
                default:
                    cerr << "Error: Invalid writeBackMux value in WB: "
                              << mem_wb_reg.writeBackMux << endl;
                    writeData = 0;
                    break;
            }
        
            string rdName = "x" + to_string(mem_wb_reg.rd);
            registerFile[rdName] = formatHex(writeData);
        
            if (printPipelineRegsEnabled || printRegistersEnabled ||
                (traceInstructionNum != -1 && traceInstructionNum == mem_wb_reg.instructionNumber)) {
                std::cout << "      WB: Write 0x"
                          << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << writeData
                          << " to " << rdName << " (Inst# "
                          << std::dec << mem_wb_reg.instructionNumber << ")" << std::endl;
            }
        }
        
        void detectAndHandleHazards() {
            hazardStall = false;
        
            // --- Load-Use Hazard Detection (Stall) ---
            if (id_ex_reg.valid && ex_mem_reg.valid
                && ex_mem_reg.memRead && ex_mem_reg.regWrite
                && ex_mem_reg.rd != 0) {
                
                bool rs1Match = (id_ex_reg.rs1 == ex_mem_reg.rd);
                bool idUsesRs2 = (id_ex_reg.aluOp == "ADD" && id_ex_reg.memWrite)
                              || (!id_ex_reg.useImm && !id_ex_reg.jump
                                  && id_ex_reg.aluOp != "LUI"
                                  && id_ex_reg.aluOp != "AUIPC");
                bool rs2Match = idUsesRs2 && (id_ex_reg.rs2 == ex_mem_reg.rd);
        
                if (rs1Match || rs2Match) {
                    hazardStall = true;
                    if (printPipelineRegsEnabled || traceInstructionNum != -1) {
                        printf(">>> Load-Use Hazard Detected! Stalling pipeline. <<<\n");
                        printf("    ID wants r%d/r%d, EX is LW to r%d\n",
                                    id_ex_reg.rs1, id_ex_reg.rs2, ex_mem_reg.rd);
                    }
                }
            }
        }
        
        void handleFlush() {
            if (branchMispredictFlush) {
                if (printPipelineRegsEnabled) {
                    printf(">>> Branch/Jump Misprediction! Flushing pipeline. <<<\n");
                }
                // Insert NOPs in IF/ID and ID/EX
                if_id_reg.clear();
                id_ex_reg.clear();
                // PC already corrected in EX stage
                branchMispredictFlush = false;
                hazardStall = false;
            }
        }
        
        void run() {
            if (pipeliningEnabled) {
                runPipeline();
            } else {
                runSingleCycle();
            }
            printFinalState();
        }
        
        void runPipeline() {
            cout << "--- Starting Pipelined Simulation ---\n";
            cout << "Knobs: Forwarding=" << boolalpha << dataForwardingEnabled
                      << ", RegPrint=" << printRegistersEnabled
                      << ", PipePrint=" << printPipelineRegsEnabled
                      << ", BPPrint=" << printBPUEnabled
                      << ", TraceInst#=" << traceInstructionNum << "\n";
        
            while (mem_wb_reg.debugInstruction != "0xDEADBEEF") {
                ++clockCycle;
                cout << "\n--- Cycle: " << std::dec<<clockCycle << " ---\n";
        
                // Execute and stage banking for correct within‑cycle data flow
                execute();
                EXMEMRegister temp = ex_mem_reg;
                ex_mem_reg = ex_debug;
                writeBack();
                memoryAccess();
                ex_mem_reg = temp;
        
                // Handle any branch/jump mispredict flush before decode/fetch
                handleFlush();
        
                instructionDecode();
                instructionFetch();
        
                // Print pipeline registers if enabled or tracing a specific inst
                if (printPipelineRegsEnabled) {
                    cout << if_id_reg.toString()    << "\n"
                              << id_ex_reg.toString()    << "\n"
                              << ex_mem_reg.toString()   << "\n"
                              << mem_wb_reg.toString()   << "\n";
                } else if (traceInstructionNum != -1) {
                    if (if_id_reg.valid && if_id_reg.instructionNumber == traceInstructionNum)
                        cout << if_id_reg.toString() << "\n";
                    if (id_ex_reg.valid && id_ex_reg.instructionNumber == traceInstructionNum)
                        cout << id_ex_reg.toString() << "\n";
                    if (ex_mem_reg.valid && ex_mem_reg.instructionNumber == traceInstructionNum)
                        cout << ex_mem_reg.toString() << "\n";
                    if (mem_wb_reg.valid && mem_wb_reg.instructionNumber == traceInstructionNum)
                        cout << mem_wb_reg.toString() << "\n";
                }
        
                if (printRegistersEnabled) {
                    printRegisterFileState();
                }
                if (printBPUEnabled) {
                    cout << bpu.toString();
                }
        
                bool pipelineEmpty = !if_id_reg.valid &&
                                     !id_ex_reg.valid &&
                                     !ex_mem_reg.valid &&
                                     !mem_wb_reg.valid;
                bool noMoreInstructions =
                    (textSegment.count(pc) ? textSegment[pc] : NOP_INSTRUCTION)
                    == NOP_INSTRUCTION;
        
                if (pipelineEmpty && noMoreInstructions) {
                    cout << "\n--- Pipeline Empty and PC points to NOP. Simulation finished. ---\n";
                    break;
                }
            }
        }
        
        void runSingleCycle() {
            cout << "--- Starting Single-Cycle Simulation (Pipelining Disabled) ---\n";
            string ir;
            uint64_t pcTemp     = 0;
            uint64_t currentPC  = pc;
        
            if_id_reg.clear();
            id_ex_reg.clear();
            ex_mem_reg.clear();
            mem_wb_reg.clear();
        
            while (true) {
                ++clockCycle;
                cout << "\n--- Cycle: " <<std::dec<< clockCycle
                          << " (PC=" << formatHex(currentPC) << ") ---\n";
        
                // 1. Fetch
                ir = textSegment.count(currentPC) ? textSegment[currentPC] : NOP_INSTRUCTION;
                cout << "Fetch: IR = " << ir << "\n";
                if (ir == NOP_INSTRUCTION || ir == "0xDEADBEEF") {
                    cout << "Termination: " << ir << "\n";
                    break;
                }
                pcTemp = currentPC + 4;
        
                // Reset pipeline registers for new instruction
                id_ex_reg.clear();
                ex_mem_reg.clear();
                mem_wb_reg.clear();
        
                // 2. Decode
                if_id_reg.instruction    = ir;
                if_id_reg.instructionPC  = currentPC;
                if_id_reg.nextPC        = pcTemp;
                if_id_reg.valid         = true;
                instructionDecode();
                cout << "Decode: " << id_ex_reg.toString() << "\n";
                if (!id_ex_reg.valid || id_ex_reg.aluOp == "INVALID") {
                    cerr << "Decode Error. Halting.\n";
                    break;
                }
        
                // 3. Execute
                pc = pcTemp;  // default PC+4
                execute();
                cout << "Execute: " << ex_mem_reg.toString() << "\n";
                if (!ex_mem_reg.valid) {
                    cerr << "Execute Error. Halting.\n";
                    break;
                }
        
                // 4. Memory
                memoryAccess();
                cout << "Memory: " << mem_wb_reg.toString() << "\n";
                if (!mem_wb_reg.valid) {
                    cerr << "Memory Stage Error. Halting.\n";
                    break;
                }
        
                // 5. Write Back
                writeBack();
        
                // Update PC for next fetch
                currentPC = pc;
        
                if (printRegistersEnabled) {
                    printRegisterFileState();
                }
        
                if (clockCycle > 5000) {
                    cerr << "Error: Single-cycle execution exceeded 5000 cycles.\n";
                    break;
                }
            }
        }
        
        void parseMachineCodeFromFile(const string &filePath) {
            textSegment.clear();
            dataMemory.clear();
            int64_t basePC = -1;
        
            ifstream file(filePath);
            if (!file) {
                cerr << "Error opening machine code file: " << filePath << endl;
                return;
            }
        
            string line;
            while (getline(file, line)) {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                auto end = line.find_last_not_of(" \t\r\n");
                if (end != string::npos) line.erase(end + 1);
                if (line.empty() || line[0] == '#') continue;
        
                istringstream iss(line);
                vector<string> parts;
                string part;
                while (iss >> part) parts.push_back(part);
                if (parts.size() < 2) {
                    cerr << "Skipping invalid line (not enough parts): " << line << endl;
                    continue;
                }
        
                string addressStr = parts[0];
                string valueStr   = parts[1];
                if (addressStr.rfind("0x", 0) != 0 || valueStr.rfind("0x", 0) != 0) {
                    cerr << "Skipping invalid line (hex format error): " << line << endl;
                    continue;
                }
                uint64_t address = stoull(addressStr.substr(2), nullptr, 16);
                uint64_t value   = stoull(valueStr.substr(2), nullptr, 16);
        
                if (line.find(',') != string::npos) {
                    // Instruction line
                    if (basePC == -1) basePC = address;
                    uint64_t instruction = value;
                    textSegment[address] = formatHex(instruction);
                    // Load bytes into dataMemory
                    for (int i = 0; i < 4; ++i) {
                        uint8_t byteVal = (instruction >> (i * 8)) & 0xFF;
                        dataMemory[address + i] = byteVal;
                    }
                } else {
                    // Data line
                    if (value > 0xFF) {
                        cerr << "Warning: Data value '" << valueStr
                                  << "' larger than a byte found on data line: "
                                  << line << ". Storing truncated byte." << endl;
                        value &= 0xFF;
                    }
                    dataMemory[address] = static_cast<uint8_t>(value);
                }
            }
        
            if (basePC != -1) {
                pc = basePC;
                cout << "Set initial PC to: " << formatHex(pc) << endl;
            } else {
                cout << "No instructions found in text segment. PC remains at 0x0." << endl;
            }
            instructionCount = 0;
            cout << "Parsing done. Loaded " << textSegment.size() << " instructions." << endl;
        }
        
        void printRegisterFileState() {
            cout << "Register File State:" << endl;
            for (int i = 0; i < 32; ++i) {
                string name = "x" + to_string(i);
                string val  = registerFile.count(name) ? registerFile[name] : formatHex(0);
                cout << "  " << name << ": " << val << " ";
                if ((i + 1) % 4 == 0) cout << endl;
            }
            if (32 % 4 != 0) cout << endl;
        }
        
        void printDataMemoryState(){
            cout << "\nData Memory State (Non-zero Bytes):" << endl;
            map<uint64_t, string> sortedMemory;
        
            for(const auto &entry : dataMemory){
                sortedMemory[entry.first] = entry.second;
            }
        
            if(sortedMemory.empty()){
                cout << "  <Empty or All Zeroes>" << endl;
            } else{
                int bytesInLine = 0;
                int64_t currentWordAddr = -1;
                ostringstream wordLine;
        
                for(const auto &entry : sortedMemory){
                    uint64_t addr = entry.first;
                    string byteVal = entry.second;
                    uint64_t wordAddr = addr & ~3ULL;
        
                    // Convert raw byte to hex string
                    unsigned char byte = static_cast<unsigned char>(byteVal[0]);
                    stringstream hexByte;
                    hexByte << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)byte;
        
                    if(wordAddr != currentWordAddr){
                        if(bytesInLine > 0){
                            cout << "  0x"
                                 << std::uppercase << std::hex << std::setfill('0') << std::setw(8)
                                 << currentWordAddr << ": " << wordLine.str() << endl;
                        }
        
                        currentWordAddr = wordAddr;
                        wordLine.str("");
                        wordLine.clear();
                        bytesInLine = 0;
        
                        for(uint64_t padAddr = wordAddr; padAddr < addr; padAddr++){
                            wordLine << "   ";
                            bytesInLine++;
                        }
        
                        wordLine << hexByte.str() << " ";
                        bytesInLine++;
                    } else{
                        for(uint64_t padAddr = (wordAddr + bytesInLine); padAddr < addr; padAddr++){
                            wordLine << "   ";
                            bytesInLine++;
                        }
        
                        wordLine << hexByte.str() << " ";
                        bytesInLine++;
                    }
        
                    if(bytesInLine == 4){
                        cout << "  0x"
                             << std::uppercase << std::hex << std::setfill('0') << std::setw(8)
                             << currentWordAddr << ": " << wordLine.str() << endl;
        
                        wordLine.str("");
                        wordLine.clear();
                        bytesInLine = 0;
                        currentWordAddr = -1;
                    }
                }
        
                if(bytesInLine > 0){
                    cout << "  0x"
                         << std::uppercase << std::hex << std::setfill('0') << std::setw(8)
                         << currentWordAddr << ": " << wordLine.str() << endl;
                }
            }
        }

        void printFinalState() {
            cout << "\n--- Simulation Complete ---" << endl;
            cout << "Total Clock Cycles: " << std::dec<< clockCycle - 1 << endl;
        
            // Uncomment and implement executedInstructions if needed:
            // cout << "Executed Instructions: " << executedInstructions << endl;
            // printf("CPI: %.2f\n", static_cast<double>(clockCycle) / executedInstructions);
        
            if (pipeliningEnabled && printBPUEnabled) {
                cout << bpu.toString() << endl;
            }
        
            printRegisterFileState();
            printDataMemoryState();
        }

        uint64_t readMemory(uint64_t address, const string &size) {
            uint64_t value = 0;
            int bytes = (size == "BYTE") ? 1 : (size == "HALF") ? 2 : 4;
            for (int i = 0; i < bytes; ++i) {
                uint64_t addr = address + i;
                uint8_t byteVal = static_cast<uint8_t>(dataMemory[addr] & 0xFF);
                value |= (static_cast<uint64_t>(byteVal) << (i * 8));
            }
            // Sign-extension for BYTE/HALF
            if (size == "BYTE" && (value & 0x80)) {
                value |= 0xFFFFFFFFFFFFFF00ULL;
            } else if (size == "HALF" && (value & 0x8000)) {
                value |= 0xFFFFFFFFFFFF0000ULL;
            }
            return value;
        }
        
        void writeMemory(uint64_t address, uint64_t data, const string &size) {
            int bytes = (size == "BYTE") ? 1 : (size == "HALF") ? 2 : 4;
            for (int i = 0; i < bytes; ++i) {
                uint64_t addr = address + i;
                uint8_t byteVal = static_cast<uint8_t>((data >> (i * 8)) & 0xFF);
                dataMemory[addr] = byteVal;
            }
        }
        
};

#include <fstream>
#include <iostream>
using namespace std;

int main(int argc, char* argv[]) {
    // Redirect cout to output.txt
    ofstream out("output.txt");
    streambuf* coutBuf = cout.rdbuf(); // Save old buffer
    cout.rdbuf(out.rdbuf());           // Redirect cout to file

    string filePath = "output.mc";

    if(argc > 1){
        filePath = argv[1];
        cout << "Using machine code file: " << filePath << endl;
    }
    else{
        cout << "Using default machine code file: " << filePath << endl;
    }

    PipelinedCPU cpu;

    cpu.pipeliningEnabled = true;
    cpu.dataForwardingEnabled = true;
    cpu.traceInstructionNum = 4;
    // cpu.printRegistersEnabled = false;
    // cpu.printPipelineRegsEnabled = true;
    // cpu.traceInstructionNum = 5;
    // cpu.printBPUEnabled = true;

    cpu.parseMachineCodeFromFile(filePath);
    cpu.run();

    cout.rdbuf(coutBuf); // Restore original buffer (optional)
    out.close();         // Close the file stream

    return 0;
}
