#include <fstream>
#include <sstream>
#include <bits/stdc++.h>
#include "myRISCVSim.h"
using namespace std;

// Constructor to initialize default values

Simulator::Simulator() : pcMuxPc(""), pcTemp(""), ir(""), rd(nullopt), rs1(""), rs2(""), func3(""), func7(""),
                immMuxB(""), immMuxInr(""), aluOp(""), muxPc(false), muxInr(false), muxMa(false),
                regWrite(false), ra(""), rb(""), rm(""), rz(""), mdr(""), mar(""), condition(false) {

// Optionals are default-initialized to nullopt
    clock = nullopt;
    muxY = nullopt;
    branch = nullopt;
    memRead = nullopt;
    memWrite = nullopt;
    muxB = nullopt;
}

void Simulator:: load_program_memory(const string& filename) {
    ifstream inputFile(filename);
    if (!inputFile) {
        cerr << "Error: Could not open file " << filename << endl;
        exit(1);
    }

    string line;
    while (getline(inputFile, line)) {
        stringstream ss(line);
        string pc, ir;
        ss >> pc >> ir;

        // Normalize PC to 8-character hex format
        stringstream formattedPC;
        formattedPC << "0x" << uppercase << setfill('0') << setw(8) << hex << stoul(pc, nullptr, 16);
        
        // Store instruction
        textSegment[formattedPC.str()] = ir;
    }
    inputFile.close();
    pcMuxPc = "0x00000000";
}



void Simulator:: run_RISCVSim() {
    while (textSegment.find(pcMuxPc) != textSegment.end()) {
        fetch();
        decode();
        execute();
        memoryAccess();
        writeBack();
    }
}

void Simulator:: reset() {
    pcTemp = "";
    ir = "";
    rd = nullopt;
    rs1 = "";
    rs2 = "";
    func3 = "";
    func7 = "";
    immMuxB = "";
    immMuxInr = "";
    aluOp = "";
    muxPc = false;
    muxInr = false;
    muxMa = false;
    regWrite = false;
    ra = "";
    rb = "";
    rm = "";
    rz = "";
    mdr = nullopt;  // Make sure optional variables are properly reset
    mar = nullopt;
    condition = false;
    
    // Optionals
    muxY = nullopt;
    branch = nullopt;
    memRead = nullopt;
    memWrite = nullopt;
    muxB = nullopt;
}

string Simulator:: regGet(const string &r){
    stringstream regKey;
    regKey << "x" << dec << stoi(r, nullptr, 2);
    
    if (registerFile.find(regKey.str()) != registerFile.end()){
        return registerFile[regKey.str()];
    } 
    return "0x00000000";
}

void Simulator:: fetch() {
    reset();
    cout<<pcMuxPc<<endl;
    if (textSegment.empty()) {
        cout << "Error: Text Segment is not initialized!" << endl;
        return;
    }

    // Ensure pcMuxPc is valid
    if (pcMuxPc.length() < 3 || pcMuxPc.find("0x") != 0) {
        cout << "Error: Invalid PC value format!" << endl;
        return;
    }

    // Fetch the instruction from memory using PC
    string pcHex = pcMuxPc;
    ir = (textSegment.find(pcHex) != textSegment.end()) ? textSegment[pcHex] : "0x00000013";

    // Convert PC from hex to integer safely
    int pcValue;
    try {
        pcValue = stoi(pcHex.substr(2), nullptr, 16);
    } catch (const invalid_argument&) {
        cout << "Error: PC contains non-hex characters!" << endl;
        return;
    } catch (const out_of_range&) {
        cout << "Error: PC value out of range!" << endl;
        return;
    }

    // Increment PC by 4 (32-bit instruction size)
    pcValue += 4;

    // Convert back to hex string
    stringstream ss;
    ss << "0x" << uppercase << setw(8) << setfill('0') << hex << noshowbase << pcValue;
    pcTemp = ss.str();
    pcMuxPc = pcTemp; // Update PC

    // Update clock cycle safely
    clock = (clock ? *clock + 1 : 1);

    // Debugging output
    cout << "-------------------\n";
    cout << "Fetch Stage:\n";
    cout << "PC: " << pcHex << "\n";
    cout << "Instruction Register (IR): " << ir << "\n";
    cout << "Updated PC: " << pcTemp << "\n";
    cout << "Clock Cycle: " << clock.value() << "\n";
    cout << "-------------------\n";
}   

void Simulator:: decode() {
    if (ir == "" || ir.length() != 10) { // Ensure IR is valid (0xXXXXXXXX format)
        cout << "Error: Invalid instruction in IR" << endl;
        return;
    }
    func3 = "";
    func7 = "";
    // Convert hex instruction to binary (excluding "0x" prefix)
    unsigned int instVal = stoul(ir.substr(2), nullptr, 16);
    string binary = bitset<32>(instVal).to_string();
    // Extract opcode (bits 0-6)
    string opcode = binary.substr(25, 7);
    // Decode based on opcode
    if (opcode == "0110011") { // R-Type (add, sub, and, or, sll, slt, sra, srl, xor, mul, div, rem)
        rd  = binary.substr(20, 5);
        rs1 = binary.substr(12, 5);
        rs2 = binary.substr(7, 5);
        func3 = binary.substr(17, 3);
        func7 = binary.substr(0, 7);
        ra = regGet(rs1);
        rb = regGet(rs2);
        // ALU operation lookup
        if (func3 == "000") {
            if (func7 == "0000000") {
                aluOp = "ADD";
            } else if (func7 == "0100000") {
                aluOp = "SUB";
            } else if (func7 == "0000001") {
                aluOp = "MUL";
            } else {
                aluOp = "INVALID";
            }
        } else if (func3 == "111") {
            aluOp = "AND";
        } else if (func3 == "110") {
            if (func7 == "0000001") {
                aluOp = "REM";
            } else {
                aluOp = "OR";
            }
        } else if (func3 == "001") {
            aluOp = "SLL";
        } else if (func3 == "010") {
            aluOp = "SLT";
        } else if (func3 == "101") {
            if (func7 == "0000000") {
                aluOp = "SRL";
            } else if (func7 == "0100000") {
                aluOp = "SRA";
            } else {
                aluOp = "INVALID";
            }
        } else if (func3 == "100") {
            if (func7 == "0000000") {
                aluOp = "XOR";
            } else if (func7 == "0000001") {
                aluOp = "DIV";
            } else {
                aluOp = "INVALID";
            }
        } else {
            throw invalid_argument("Invalid R-type instruction");
        }
        //select appropriate controls for R type instruction
        muxPc = false;
        muxInr = false;
        muxMa = false; // selecting pc
        muxY = 0;
        branch = false;
        memRead = false;
        memWrite = false;
        regWrite = true;
        muxB = false;
    }
    else if (opcode == "0010011") { // I-Type (addi, andi, ori)
        rd  = binary.substr(20, 5);
        rs1 = binary.substr(12, 5);
        func3 = binary.substr(17, 3);
        immMuxB = binary.substr(0, 12);
        
        ra = regGet(rs1);
        
        // Sign-extend immMuxB to 32-bit
        int immValue = stoi(immMuxB, nullptr, 2);
        if (immMuxB[0] == '1') { // Check if negative (MSB is 1)
            immValue -= (1 << 12); // Proper sign-extension for 12-bit values
        }
        immMuxB = bitset<32>(immValue).to_string();
        
        // ALU operation lookup
        if (func3 == "000") {
            aluOp = "ADD";
        } else if (func3 == "111") {
            aluOp = "AND";
        } else if (func3 == "110") {
            aluOp = "OR";
        } else {
            throw invalid_argument("Invalid I-type instruction");
        }
        muxPc = false;     
        muxInr = false;  
        muxMa = false; // selecting pc
        muxY = 0;      //select output from alu which is rz
        branch = false;
        memRead = false;
        memWrite = false;
        regWrite = true;
        muxB = true;
    }
    else if (opcode == "0000011") { // Load (lb, lh, lw, ld)
        rd  = binary.substr(20, 5);
        rs1 = binary.substr(12, 5);
        func3 = binary.substr(17, 3);
        string immMuxB = binary.substr(0, 12); // Extract first 12 bits


        cout<<binary<<" "<<rd.value()<<" "<<rs1<<endl;
        
        ra = regGet(rs1);
        // Extract and sign-extend the 12-bit immediate
        int immValue = stoi(immMuxB, nullptr, 2);
        if (immMuxB[0] == '1') { 
            immValue -= (1 << 12); 
        }
        immMuxB = bitset<32>(immValue).to_string();
        
        // Determine memory access size based on func3
        string size;
        if (func3 == "000") {
            size = "BYTE"; // lb (load byte)
        } else if (func3 == "001") {
            size = "HALF"; // lh (load half-word)
        } else if (func3 == "010") {
            size = "WORD"; // lw (load word)
        } else if (func3 == "011") {
            size = "DOUBLE"; // ld (load double-word)
        } else {
            throw invalid_argument("Invalid load instruction");
        }
        
        // Set ALU operation to perform address computation (ra + imm)
        aluOp = "LOAD";
        
        // Control signals
        muxPc = false;
        muxInr = false;  //no change in pc
        muxMa = true; // Selecting rz
        muxY = 1;     //select data from mdr for loading into rd
        branch = false;  
        memRead = true;
        memWrite = false;
        regWrite = true;
        muxB = true;   
    }
    else if (opcode == "1100111") { // JALR
        rd  = binary.substr(20, 5);
        rs1 = binary.substr(12, 5);
        func3 = binary.substr(17, 3);
        immMuxInr = binary.substr(0, 12); //offset to pc
        ra = regGet(rs1);
        // Sign-extend immMuxInr to 32-bit
        int jalrImm = stoi(immMuxInr, nullptr, 2);
        if (immMuxInr[0] == '1') { 
            jalrImm -= (1 << 12);           //for neg numbers sign extension
        }
        immMuxInr = bitset<32>(jalrImm).to_string();  //this imm value is then passed to MuxInr to inc in pc
        
        aluOp = "JALR";
        muxPc = true; // selecting ra
        muxInr = false;
        muxMa = false; // selecting pc
        muxY = 2;
        branch = true;
        memRead = false;
        memWrite = false;
        regWrite = true;
        muxB = nullopt;
    }
    else if (opcode == "0100011") { // S-Type (sb, sw, sd, sh)
        rs1 = binary.substr(12, 5);
        rs2 = binary.substr(7, 5);
        func3 = binary.substr(17, 3);
        immMuxB = binary.substr(0, 7) + binary.substr(20, 5); // Immediate field
        ra = regGet(rs1);
        rb = regGet(rs2);
        rm = rb;   //this will be forwaded to mdr in execute stage
        
        // Sign-extend immMuxB to 32-bit
        int immValue = bitset<12>(immMuxB).to_ulong(); // Extract as unsigned
        if (immMuxB[0] == '1') { // Sign extend if negative
            immValue |= 0xFFFFF000;
        }
        immMuxB = bitset<32>(immValue).to_string();
        
        // Determine size based on func3
        string size;
        if (func3 == "000") {
            size = "BYTE"; // sb
        } else if (func3 == "001") {
            size = "HALF"; // sh
        } else if (func3 == "010") {
            size = "WORD"; // sw
        } else if (func3 == "011") {
            size = "DOUBLE"; // sd
        } else {
            throw invalid_argument("Invalid S-type instruction");
        }
        
        aluOp = "STORE";
        // Control signals
        muxPc = false;
        muxInr = false;
        muxMa = true; // selecting rz
        muxY = nullopt;
        branch = false;
        memRead = false;
        memWrite = true;
        regWrite = false;
        muxB = true;
    }
    else if (opcode == "1100011") { // SB-Type (beq, bne, bge, blt)
        //cout<<binary<<endl;
        rs1 = binary.substr(12, 5);
        rs2 = binary.substr(7, 5);
        func3 = binary.substr(17, 3);
        ra = regGet(rs1);
        rb = regGet(rs2);
    
        string imm12 = binary.substr(0, 1);       // Bit 31
        string imm10_5 = binary.substr(1, 6);     // Bits 30-25
        string imm4 = binary.substr(24, 1);       // Bit 11
        string imm11 = binary.substr(7, 1);       // Bit 7
        string imm9_8 = binary.substr(8, 2);      // Bits 10-8

        // Combine the bits to form the immediate
        string imm = imm12 + imm11 + imm10_5 + imm9_8 + imm4 + "0"; // Add a trailing 0 (<<1 shift)
        cout<<"this is imm in branch"<<imm<<endl; // Final SB-type immediate

        // Convert to integer and sign-extend
        int immValue = stoi(imm, nullptr, 2);
        if (imm12 == "1") {
            immValue -= (1 << 13);
        }
        immMuxInr = bitset<32>(immValue).to_string();
        
        // Set ALU operation based on func3
        if (func3 == "000") {
            aluOp = "BEQ";
        } else if (func3 == "001") {
            aluOp = "BNE";
        } else if (func3 == "100") {
            aluOp = "BLT";
        } else if (func3 == "101") {
            aluOp = "BGE";
        } else {
            throw invalid_argument("Invalid SB-type instruction");
        }
        
        // Control signals
        muxPc = false;
        muxInr = false;//it initially is set to false , if branch is taken then set to true.
        muxMa = false; // selecting pc
        muxY = nullopt;
        branch = true;
        memRead = false;
        memWrite = false;
        regWrite = false;
        muxB = false;
    }
    else if (opcode == "0110111") { // U-Type (LUI)
        rd = binary.substr(20, 5);
        // Extract 20-bit immediate and shift left by 12 (LUI semantics)
        int immValue = stoi(binary.substr(0, 20), nullptr, 2);
        if (binary[0] == '1') { // If MSB is 1 (negative)
            immValue -= (1 << 20); // Proper two's complement sign extension
        }
        immValue <<= 12; // Shift left to match instruction format
        immMuxB = bitset<32>(immValue).to_string();
        // Set ALU operation
        aluOp = "LUI";
        // Control signals
        muxPc = false;
        muxInr = false;  
        muxMa = false; // selecting PC
        muxY = 0;
        branch = false;
        memRead = false;
        memWrite = false;
        regWrite = true;
        muxB = true;
    }
    else if (opcode == "0010111") { // U-Type (AUIPC)
        rd = binary.substr(20, 5);
        ra = pcMuxPc;    //select pc to ra and then enter ALU
        // Extract 20-bit immediate and shift left by 12 (AUIPC semantics)
        int immValue = stoi(binary.substr(0, 20), nullptr, 2) << 12;
        // Sign-extend to 32-bit
        if (immMuxB[0] == '1') { // Check if negative (MSB is 1)
            immValue -= (1 << 12); // Proper sign-extension for 12-bit values
        }
        immMuxB = bitset<32>(immValue).to_string();
        // Set ALU operation
        aluOp = "AUIPC";
        // Control signals
        muxPc = false;
        muxInr = false;
        muxMa = false; // selecting PC
        muxY = 0;
        branch = false;
        memRead = false;
        memWrite = false;
        regWrite = true;
        muxB = true;
    }
    else if (opcode == "1101111") { // UJ-Type (JAL)
        rd = binary.substr(20, 5);
        // Extract 20-bit JAL immediate and rearrange the bits correctly
        string immBinary = binary.substr(0, 1) + binary.substr(12, 8) + binary.substr(11, 1) + binary.substr(1, 10) + "0";
        // Convert to signed integer
        int immValue = stoi(immBinary, nullptr, 2);
        // Sign-extend to 32-bit
        if (binary[0] == '1') {
            immValue -= (1 << 21); // Correct sign-extension
        }
        immMuxInr = bitset<32>(immValue).to_string();
        // Set ALU operation for JAL
        aluOp = "JAL";
        // Control signals
        muxPc = false;
        muxInr = false;
        muxMa = false; // selecting PC
        muxY = 2;
        branch = true;
        memRead = false;
        memWrite = false;
        regWrite = true;
        muxB = nullopt;    //dontcare value
    }
    else {
        cout << "Error: Unsupported opcode " << opcode << endl;
        return;
    }
    
    // Debug output
    cout << "Decode Stage:" << endl;
    cout << "Opcode: " << opcode << endl;
    cout << "rd: " << (rd.has_value() ? *rd : "Dont Care") << endl; //might be dontcare
    cout << "rs1: " << rs1 << endl;
    cout << "rs2: " << rs2 << endl;
    cout << "func3: " << func3 << endl;
    cout << "func7: " << (!func7.empty() ? func7 : "Not supported" )<< endl;
    if (!immMuxB.empty()) {
        cout << "Immediate: " << immMuxB << endl;
    } else if (!immMuxInr.empty()) {
        cout << "Immediate: " << immMuxInr << endl;
    }    
}  

void Simulator:: execute() {
    if (aluOp == "") {
        cout << "Error: ALU operation not set." << endl;
        return;
    }
    
    // Convert ra and rb from hex to int, handling null cases
    unsigned int op1 = (ra != "") ? stoul(ra.substr(2), nullptr, 16) : 0;
    unsigned int op2 = 0;
    
    //check for op2 among rs2 and immediate 12-bit ,based on dontcare and muxB control value
    if (muxB.has_value() && muxB.value()) {
        op2 = (immMuxB != "") ? stoul(immMuxB, nullptr, 2) : 0; // Immediate value for I-type & S-type
    } else if (rb != "") {
        op2 = stoul(rb.substr(2), nullptr, 16); // Register value for R-type & SB-type
    }
    //else{
    //     cout<<"Error accesing second operand"<<endl;
    //     return;
    // }
    
    int result = 0;
    
    // Execute ALU operation
    if (aluOp == "ADD") {
        result = op1 + op2;
    } else if (aluOp == "SUB") {
        result = op1 - op2;
    } else if (aluOp == "MUL") {
        result = op1 * op2;
    } else if (aluOp == "DIV") {
        result = (op2 != 0) ? op1 / op2 : 0;
    } else if (aluOp == "REM") {
        result = (op2 != 0) ? op1 % op2 : 0;
    } else if (aluOp == "AND") {
        result = op1 & op2;
    } else if (aluOp == "OR") {
        result = op1 | op2;
    } else if (aluOp == "XOR") {
        result = op1 ^ op2;
    } else if (aluOp == "SLL") {
        result = op1 << (op2 & 0x1F); // Masking to 5 bits for shift amount
    } else if (aluOp == "SRL") {
        result = static_cast<int>(static_cast<unsigned int>(op1) >> (op2 & 0x1F));
    } else if (aluOp == "SRA") {
        result = op1 >> (op2 & 0x1F);
    } else if (aluOp == "SLT") {
        result = (op1 < op2) ? 1 : 0;
    } else if (aluOp == "LUI") {
        result = op2; // LUI directly loads the immediate
    } else if (aluOp == "AUIPC") {
        result = op1 + op2; // AUIPC adds immediate to PC
    } else if (aluOp == "JAL") {
        condition = true; // JAL stores return address
    } else if (aluOp == "JALR") {
        condition = true; // JALR clears LSB to ensure even address
    } else if (aluOp == "BEQ") {
        condition = (op1 == op2);
    } else if (aluOp == "BNE") {
        condition = (op1 != op2);
    } else if (aluOp == "BLT") {
        condition = (op1 < op2);
    } else if (aluOp == "BGE") {
        condition = (op1 >= op2);
    } else if (aluOp == "LOAD") {
        result = op1 + op2; // Compute effective address for load
    } else if (aluOp == "STORE") {
        result = op1 + op2; // Compute effective address for store
    } else {
        cout << "Error: Unsupported ALU operation " << aluOp << endl;
        return;
    }
    
    // Store result in rz unless it's a branch instruction
    if (aluOp[0] != 'B') {
        stringstream ss;
        ss << "0x" << uppercase << setw(8) << setfill('0') << hex << result;
        rz = ss.str();
    }
    
    // Handle branch condition (update PC if branch is taken)
    if (branch.has_value() && branch.value()) {
        if (condition) {
            muxInr = true;
        }
    }
    
    if (aluOp == "LOAD" || aluOp == "STORE") {
        mdr = rm;
        mar = rz;
    }
    else {
        mar = "";
    }
    // Debug output
    
    cout << "Execute Stage:" << endl;
    cout << "ALU Operation: " << aluOp << endl;
    cout << "Operand 1 (ra): " << ((ra != "") ? ra : "NULL") << endl;
    if (muxB.has_value() && muxB.value()) {
        cout << "Operand 2 (rb/imm): " << (immMuxB != "" ? immMuxB : "NULL") << endl;
    } else {
        cout << "Operand 2 (rb/imm): " << (rb != "" ? rb : "NULL") << endl;
    }
    cout << "Result (rz): " << (!rz.empty()? rz : "Dont Care" )<< endl;
    if (branch.has_value() && branch.value()) {
        cout << "Branch Taken: " << (condition ? "true" : "false") << endl;
    }
}

void Simulator:: memoryAccess() {
    if (!mar.has_value() || mar.value().length() == 0 ) {
        cout<<"mar - null , inr - null"<<endl;
        if (muxInr) {
            cout<<"mar - null , inr - true"<<endl;
            int pcValue;
            if (muxPc) {
                if (ra.length() < 2) {
                    cout << "Error: ra is too short." << endl;
                    return;
                }
                pcValue = stoi(ra.substr(2), nullptr, 16);
            } else {
                if (pcMuxPc.length() < 2) {
                    cout << "Error: pcMuxPc is too short." << endl;
                    return;
                }
                cout<<"this is pc : "<<pcMuxPc<<endl;
                pcValue = stoi(pcMuxPc.substr(2), nullptr, 16);
            }

            //since pc is alaready updated to pc+4 we need to subtract 4
            cout<<"this is imminr value"<<immMuxInr<<endl;

            pcValue += stoi(immMuxInr, nullptr, 2) - 4; 
            cout<<"this is pc value"<<pcValue<<endl;

            stringstream ss;
            ss << uppercase << setfill('0') << setw(8) << hex << pcValue;
            pcMuxPc = "0x" + ss.str();
            cout<<"this is pc value"<<pcValue<<endl;
            cout<<"this is pc : "<<pcMuxPc<<endl;

            
        }
    } else {
        cout<<"mar has value"<<endl;
        if (mar.value().length() < 2) {
            cout << "Error: MAR is too short. MAR Content: " << mar.value() << endl;
            return;
        }
        unsigned int address = stoul(mar.value().substr(2), nullptr, 16);

        if (memRead.has_value() && memRead.value()) {
            string loadedValue;
            if (size == "BYTE") {
                char buf[16];
                snprintf(buf, sizeof(buf), "0x%08X", address);
                loadedValue = (memory.find(buf) != memory.end()) ? memory[buf] : "00";
            } else if (size == "HALF") {
                for (int i = 0; i < 2; i++) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "0x%08X", address + i);
                    loadedValue = ((memory.find(buf) != memory.end()) ? memory[buf] : "00") + loadedValue;
                }
            } else if (size == "WORD") {
                for (int i = 0; i < 4; i++) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "0x%08X", address + i);
                    loadedValue = ((memory.find(buf) != memory.end()) ? memory[buf] : "00") + loadedValue;
                }
            } else if (size == "DOUBLE") {
                for (int i = 0; i < 8; i++) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "0x%08X", address + i);
                    loadedValue = ((memory.find(buf) != memory.end()) ? memory[buf] : "00") + loadedValue;
                }
            } else {
                cout << "Error: Invalid memory size for load." << endl;
                return;
            }
            for (auto &ch : loadedValue) ch = toupper(ch);
            mdr = "0x" + loadedValue;
            cout << "Loaded Value (MDR): " << mdr.value() << " from Address (MAR): " << mar.value() << endl;
        }

        if (memWrite.has_value() && memWrite.value()) {
            if (!mdr.has_value() || mdr.value().length() < 2) {
                cout << "Error: MDR is empty or too short." << endl;
                return;
            }
            string value = mdr.value().substr(2);
            if (size == "BYTE" && value.size() < 2) {
                cout << "Error: Insufficient data for BYTE write." << endl;
                return;
            } else if (size == "HALF" && value.size() < 4) {
                cout << "Error: Insufficient data for HALF write." << endl;
                return;
            } else if (size == "WORD" && value.size() < 8) {
                cout << "Error: Insufficient data for WORD write." << endl;
                return;
            } else if (size == "DOUBLE" && value.size() < 16) {
                cout << "Error: Insufficient data for DOUBLE write." << endl;
                return;
            }

            if (size == "BYTE") {
                char buf[16];
                snprintf(buf, sizeof(buf), "0x%08X", address);
                memory[buf] = value.substr(value.size() - 2);
            } else if (size == "HALF") {
                for (int i = 0; i < 2; i++) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "0x%08X", address + i);
                    memory[buf] = value.substr(value.size() - (2 * (i + 1)), 2);
                }
            } else if (size == "WORD") {
                for (int i = 0; i < 4; i++) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "0x%08X", address + i);
                    memory[buf] = value.substr(value.size() - (2 * (i + 1)), 2);
                }
            } else if (size == "DOUBLE") {
                for (int i = 0; i < 8; i++) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "0x%08X", address + i);
                    memory[buf] = value.substr(value.size() - (2 * (i + 1)), 2);
                }
            } else {
                cout << "Error: Invalid memory size for store." << endl;
                return;
            }
            cout << "Stored Value (MDR): " << mdr.value() << " to Address (MAR): " << mar.value() << endl;
        }
    }
}

void Simulator:: writeBack() {

    if (!regWrite.has_value() || regWrite.value() == false) {
        cout << "Skipping WriteBack: regWrite is disabled." << endl;
        return;
    }
    if (!rd.has_value() || rd.value() == "00000") { // x0 should not be modified
        cout << "Skipping WriteBack: Destination register is x0." << endl;
        return;
    }
    if (muxY == 0) {
        ry = rz;
    } else if (muxY == 1) {
        ry = mdr.has_value() ? mdr.value() : "";
    } else if (muxY == 2) {
        ry = pcTemp;   //for jal/jalr 
    }
    int regNum = stoi(rd.value(), nullptr, 2);   //hex to bin
    string registerName = "x" + to_string(regNum);
    registerFile[registerName] = ry;             //load then in reg file
    cout << "WriteBack: Register " << registerName << " updated with " << ry << endl;
}

