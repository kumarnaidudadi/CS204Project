#ifndef MYRISCVSIM_H
#define MYRISCVSIM_H

#include <bits/stdc++.h>
using namespace std;

class Simulator {
    public:
        // Data members (instance variables)
        unordered_map<string, string> textSegment;  //indexed based on pc
        unordered_map<string, string> registerFile; //indexed based on 5 bit - registor number 
        unordered_map<string, string> memory;       //byte addressible 
        string size; 
        
        string pcMuxPc; 
        string pcTemp;
        string ir;
        optional<int> clock; // using optional to mimic possible null
        optional<string> rd; // to mimic dont care use optional                        
        string rs1, rs2, func3, func7; 
        string immMuxB;      //for imm type instructions
        string immMuxInr;    //the imm offset extracted for control instructions 
        string aluOp;        //opcode 
        bool muxPc;          //part of IAG
        bool muxInr;         //part of IAG
        bool muxMa;            
        optional<int> muxY; // can be 0,1,2 or null
        optional<bool> branch; // can be true, false, or null
        optional<bool> memRead; // can be true, false, or null
        optional<bool> memWrite; // can be true, false, or null
        optional<bool> regWrite;
        optional<bool> muxB; 
        
        string ra, rb, rm, rz, ry;  //buffers
    
        optional<string> mdr;                       // Memory Data Register (MDR)
        optional<string> mar;                       // Memory Address Register (MAR)
        bool condition;

    // Constructor
    Simulator();
    void load_program_memory(const string& filename); // Method to load program memory
    void run_RISCVSim(); // Run simulation
    void reset();
    string regGet(const string& r);
    void fetch();
    void decode();
    void execute();
    void memoryAccess();
    void writeBack();
};

#endif // MYRISCVSIM_H