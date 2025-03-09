#include <bits/stdc++.h>
#include <sstream>
#include <fstream>
#include <vector>
using namespace std;

// -------------------- Mappings --------------------
unordered_map<string, tuple<string, string, string>> r_format = {
    {"add", {"0110011", "000", "0000000"}}, {"and", {"0110011", "111", "0000000"}},
    {"or",  {"0110011", "110", "0000000"}}, {"sll", {"0110011", "001", "0000000"}},
    {"slt", {"0110011", "010", "0000000"}}, {"sra", {"0110011", "101", "0100000"}},
    {"srl", {"0110011", "101", "0000000"}}, {"sub", {"0110011", "000", "0100000"}},
    {"xor", {"0110011", "100", "0000000"}}, {"mul", {"0110011", "000", "0000001"}},
    {"div", {"0110011", "100", "0000001"}}, {"rem", {"0110011", "110", "0000001"}}
};

unordered_map<string, tuple<string, string>> i_format = {
    {"addi", {"0010011", "000"}}, {"andi", {"0010011", "111"}},
    {"ori",  {"0010011", "110"}}, {"lb",   {"0000011", "000"}},
    {"ld",   {"0000011", "011"}}, {"lh",   {"0000011", "001"}},
    {"lw",   {"0000011", "010"}}, {"jalr", {"1100111", "000"}}
};

unordered_map<string, pair<string, string>> s_format = {
    {"sb", {"0100011", "000"}}, {"sh", {"0100011", "001"}},
    {"sw", {"0100011", "010"}}, {"sd", {"0100011", "011"}}
};

unordered_map<string, pair<string, string>> sb_format = {
    {"beq", {"1100011", "000"}}, {"bne", {"1100011", "001"}},
    {"blt", {"1100011", "100"}}, {"bge", {"1100011", "101"}}
};

unordered_map<string, string> u_format = {
    {"auipc", "0010111"}, {"lui", "0110111"}
};

unordered_map<string, string> uj_format = {
    {"jal", "1101111"}
};

unordered_map<string, string> register_map = {
    {"x0", "00000"}, {"x1", "00001"}, {"x2", "00010"}, {"x3", "00011"},
    {"x4", "00100"}, {"x5", "00101"}, {"x6", "00110"}, {"x7", "00111"},
    {"x8", "01000"}, {"x9", "01001"}, {"x10", "01010"}, {"x11", "01011"},
    {"x12", "01100"}, {"x13", "01101"}, {"x14", "01110"}, {"x15", "01111"},
    {"x16", "10000"}, {"x17", "10001"}, {"x18", "10010"}, {"x19", "10011"},
    {"x20", "10100"}, {"x21", "10101"}, {"x22", "10110"}, {"x23", "10111"},
    {"x24", "11000"}, {"x25", "11001"}, {"x26", "11010"}, {"x27", "11011"},
    {"x28", "11100"}, {"x29", "11101"}, {"x30", "11110"}, {"x31", "11111"}
};

unordered_map<string, int> label_table;

// -------------------- Helper Functions --------------------
string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t");
    size_t end = s.find_last_not_of(" \t");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

vector<string> tokenize(const string &line) {
    vector<string> tokens;
    string token;
    bool in_quote = false;
    
    for (char c : line) {
        if (c == '"') in_quote = !in_quote;
        if ((c == ',' || isspace(c)) && !in_quote) {
            if (!token.empty()) {
                tokens.push_back(trim(token));
                token.clear();
            }
        } else {
            token += c;
        }
    }
    if (!token.empty()) tokens.push_back(trim(token));
    return tokens;
}

pair<string, string> split_offset_rs(const string &operand) {
    size_t lparen = operand.find('(');
    if (lparen == string::npos) return {"0", operand};
    string offset = operand.substr(0, lparen);
    string reg = operand.substr(lparen+1);
    reg = reg.substr(0, reg.find(')'));
    return {trim(offset), trim(reg)};
}

string bin_to_hex(const string &bin) {
    if (bin.length() != 32) return "INVALID_BIN";
    stringstream ss;
    ss << "0x" << hex << uppercase << setfill('0');
    for (int i=0; i<32; i+=4) {
        ss << bitset<4>(bin.substr(i,4)).to_ulong();
    }
    return ss.str();
}

int convertToDecimal(const string &num) {
    if (num.empty()) return 0;
    try {
        if (num.find("0x") == 0) return stoi(num.substr(2), nullptr, 16);
        if (num.find("0b") == 0) return stoi(num.substr(2), nullptr, 2);
        return stoi(num);
    } catch (...) {
        cerr << "Invalid number: " << num << endl;
        exit(1);
    }
}

// -------------------- Two-Pass Assembler --------------------
void first_pass(const string &input_file) {
    ifstream infile(input_file);
    string line;
    int text_addr = 0x00000000;
    int data_addr = 0x10000000;
    string current_section = "text";

    while (getline(infile, line)) {
        line = line.substr(0, line.find('#'));
        line = trim(line);
        if (line.empty()) continue;

        vector<string> tokens = tokenize(line);
        if (tokens.empty()) continue;

        // Handle labels - Modified to handle labels with/without spaces after colons
        if (!tokens.empty()) {
            size_t colon_pos = tokens[0].find(':');
            if (colon_pos != string::npos) {
                string label = tokens[0].substr(0, colon_pos);
                string remainder = tokens[0].substr(colon_pos + 1);
                label_table[label] = (current_section == "text") ? text_addr : data_addr;
                
                if (remainder.empty()) {
                    // Case: "label:"
                    tokens.erase(tokens.begin());
                } else {
                    // Case: "label:.word" -> replace with ".word"
                    tokens[0] = remainder;
                }
            }
        }

        if (tokens.empty()) continue;

        // Section handling
        if (tokens[0] == ".text") {
            current_section = "text";
        } else if (tokens[0] == ".data") {
            current_section = "data";
        } else if (current_section == "text") {
            text_addr += 4;
        } else if (current_section == "data") {
            if (tokens[0] == ".byte") data_addr += tokens.size()-1;
            else if (tokens[0] == ".half") data_addr += 2*(tokens.size()-1);
            else if (tokens[0] == ".word") data_addr += 4*(tokens.size()-1);
            else if (tokens[0] == ".dword") data_addr += 8*(tokens.size()-1);
            else if (tokens[0] == ".asciz") {
                string str = tokens[1].substr(1, tokens[1].size()-2);
                data_addr += str.size() + 1;
            }
        }
    }
    infile.close();
}

void second_pass(const string &input_file, const string &output_file) {
    ifstream infile(input_file);
    ofstream outfile(output_file);
    string line;
    int text_addr = 0x00000000;
    int data_addr = 0x10000000;
    string current_section = "text";

    while (getline(infile, line)) {
        string orig_line = line;
        line = line.substr(0, line.find('#'));
        line = trim(line);
        if (line.empty()) continue;

        vector<string> tokens = tokenize(line);

        // Handle labels - Modified to handle labels with/without spaces after colons
        if (!tokens.empty()) {
            size_t colon_pos = tokens[0].find(':');
            if (colon_pos != string::npos) {
                string label = tokens[0].substr(0, colon_pos);
                string remainder = tokens[0].substr(colon_pos + 1);
                
                if (remainder.empty()) {
                    // Case: "label:"
                    tokens.erase(tokens.begin());
                } else {
                    // Case: "label:.word" -> replace with ".word"
                    tokens[0] = remainder;
                }
            }
        }

        if (tokens.empty()) continue;

        // Section handling
        if (!tokens.empty() && tokens[0] == ".text") {
            current_section = "text";
            tokens.erase(tokens.begin());
        } else if (!tokens.empty() && tokens[0] == ".data") {
            current_section = "data";
            tokens.erase(tokens.begin());
        }

        if (tokens.empty()) continue;

        if (current_section == "text") {
            string op = tokens[0];
            string binary_code, instruction_format;
            string formatted_inst = op;

            try {
                // Reconstruct instruction with commas
                for (size_t i=1; i<tokens.size(); ++i) {
                    formatted_inst += (i == 1 ? " " : ",") + tokens[i];
                }

                if (r_format.find(op) != r_format.end()) {
                    // R-format instructions
                    auto [opcode, func3, func7] = r_format[op];
                    string rd = register_map.at(tokens[1]);
                    string rs1 = register_map.at(tokens[2]);
                    string rs2 = register_map.at(tokens[3]);
                    
                    binary_code = func7 + rs2 + rs1 + func3 + rd + opcode;
                    instruction_format = opcode + "-" + func3 + "-" + func7 + "-" +
                                       rd + "-" + rs1 + "-" + rs2 + "-NULL";
                }
                else if (i_format.find(op) != i_format.end()) {
                    // I-format instructions
                    auto [opcode, func3] = i_format[op];
                    string rd, rs1, imm_str;

                    if (op == "jalr" || op == "lb" || op == "ld" || op == "lh" || op == "lw") {
                        // Loads/JALR: op rd, offset(rs1)
                        rd = register_map.at(tokens[1]);
                        auto [offset, rs] = split_offset_rs(tokens[2]);
                        rs1 = register_map.at(rs);
                        int imm_value = convertToDecimal(offset);
                        imm_str = bitset<12>(imm_value).to_string();
                    } else {
                        // ALU immediates: addi/andi/ori
                        rd = register_map.at(tokens[1]);
                        rs1 = register_map.at(tokens[2]);
                        int imm_value = convertToDecimal(tokens[3]);
                        imm_str = bitset<12>(imm_value).to_string();
                    }

                    binary_code = imm_str + rs1 + func3 + rd + opcode;
                    instruction_format = opcode + "-" + func3 + "-NULL-" +
                                       rd + "-" + rs1 + "-" + imm_str;
                }
                else if (s_format.find(op) != s_format.end()) {
                    // S-format instructions
                    auto [opcode, func3] = s_format[op];
                    string rs2 = register_map.at(tokens[1]);
                    auto [offset, rs1] = split_offset_rs(tokens[2]);
                    bitset<12> imm(convertToDecimal(offset));
                    
                    string imm_hi = imm.to_string().substr(0, 7);
                    string imm_lo = imm.to_string().substr(7, 5);
                    binary_code = imm_hi + rs2 + register_map.at(rs1) + func3 + imm_lo + opcode;
                    instruction_format = opcode + "-" + func3 + "-NULL-" +
                                       rs1 + "-" + rs2 + "-" + imm.to_string();
                }
                else if (sb_format.find(op) != sb_format.end()) {
                    auto [opcode, func3] = sb_format[op];
                    string rs1 = register_map.at(tokens[1]);
                    string rs2 = register_map.at(tokens[2]);
                    
                    int offset;
                    // Check if operand is a label or immediate value
                    if (label_table.find(tokens[3]) != label_table.end()) {
                        int target_addr = label_table[tokens[3]];
                        offset = target_addr - text_addr;
                    } else {
                        offset = convertToDecimal(tokens[3]);
                    }
                
                    // Branch offsets must be divided by 2 for word alignment
                    bitset<13> imm(offset);  // 13 bits to handle signed values
                    
                    // Extract bits according to SB-format encoding
                    string imm_12   = imm.to_string().substr(0, 1);   // bit 12 (sign bit)
                    string imm_10_5 = imm.to_string().substr(2, 6);   // bits 10-5
                    string imm_4_1  = imm.to_string().substr(8, 4);   // bits 4-1
                    string imm_11   = imm.to_string().substr(1, 1);   // bit 11
                    
                    // Assemble binary according to RISC-V encoding rules
                    binary_code = imm_12 + imm_10_5 + rs2 + rs1 + func3 + imm_4_1 + imm_11 + opcode;
                
                    instruction_format = opcode + "-" + func3 + "-NULL-" +
                                        rs1 + "-" + rs2 + "-" +
                                        imm.to_string();
                }
                else if (u_format.find(op) != u_format.end()) {
                    // U-format instructions
                    string opcode = u_format[op];
                    string rd = register_map.at(tokens[1]);
                    bitset<20> imm(convertToDecimal(tokens[2]) & 0xFFFFF);
                    
                    binary_code = imm.to_string() + rd + opcode;
                    instruction_format = opcode + "-NULL-NULL-" +
                                       rd + "-NULL-" + imm.to_string();
                }
                else if (uj_format.find(op) != uj_format.end()) {
                    string opcode = uj_format[op];
                    string rd = register_map.at(tokens[1]);
                    
                    int offset;
                    // Check if operand is a label or immediate value
                    if (label_table.find(tokens[2]) != label_table.end()) {
                        int target_addr = label_table[tokens[2]];
                        offset = target_addr - text_addr;
                    } else {
                        offset = convertToDecimal(tokens[2]);
                    }
                    
                    // JAL offsets use 21 bits to handle signed values
                    bitset<21> imm(offset);
                    string imm_str = imm.to_string();
                    
                    // Extract bits according to UJ-format encoding
                    string imm_20   = imm_str.substr(0, 1);    // bit 20 (sign bit)
                    string imm_19_12 = imm_str.substr(1, 8);   // bits 19-12
                    string imm_11   = imm_str.substr(9, 1);    // bit 11
                    string imm_10_1 = imm_str.substr(10, 10);  // bits 10-1
                    
                    // Assemble binary according to RISC-V encoding rules
                    binary_code = imm_20 + imm_10_1 + imm_11 + imm_19_12 + rd + opcode;
                    
                    instruction_format = opcode + "-NULL-NULL-" +
                                        rd + "-NULL-" +
                                        imm_str;
                }

                outfile << "0x" << hex << text_addr << " " << bin_to_hex(binary_code)
                        << " , " << formatted_inst << " # " << instruction_format << endl;
                text_addr += 4;
            } catch (const out_of_range& e) {
                cerr << "Invalid operand in line: " << orig_line << endl;
                exit(1);
            }
        }
        else if (current_section == "data") {
            string directive = tokens[0];
            vector<string> operands(tokens.begin()+1, tokens.end());

            if (directive == ".byte") {
                for (string op : operands) {
                    int val = convertToDecimal(op);
                    outfile << "0x" << hex << data_addr 
                            << " 0x" << setw(2) << setfill('0') << (val & 0xFF) << endl;
                    data_addr += 1;
                }
            }
            else if (directive == ".half") {
                for (string op : operands) {
                    int val = convertToDecimal(op);
                    outfile << "0x" << hex << data_addr 
                            << " 0x" << setw(4) <<  setfill('0') <<(val & 0xFFFF) << endl;
                    data_addr += 2;
                }
            }
            else if (directive == ".word") {
                for (string op : operands) {
                    int val = convertToDecimal(op);
                    outfile << "0x" << hex << data_addr 
                            << " 0x" << setw(8) << setfill('0') << val << endl;
                    data_addr += 4;
                }
            }
            else if (directive == ".dword") {
                for (string op : operands) {
                    int val = convertToDecimal(op);
                    outfile << "0x" << hex << data_addr 
                            << " 0x" << setw(16) << setfill('0') << val << endl;
                    data_addr += 8;
                }
            }
            else if (directive == ".asciz") {
                string str = operands[0].substr(1, operands[0].size()-2);
                for (char c : str) {
                    outfile << "0x" << hex << data_addr 
                            << " 0x" << setw(2) << setfill('0') << (int)c << endl;
                    data_addr += 1;
                }
                outfile << "0x" << hex << data_addr << " 0x00" << endl;
                data_addr += 1;
            }
        }
    }
    outfile<<"TERMINATED"<<endl;
    infile.close();
    outfile.close();
}

int main() {
    first_pass("input.asm");
    second_pass("input.asm", "output.mc");
    cout << "Assembly successfully converted to machine code." << endl;
    return 0;
}
