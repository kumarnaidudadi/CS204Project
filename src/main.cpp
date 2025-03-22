#include <fstream>
#include <sstream>
#include <bits/stdc++.h>
#include "myRISCVSim.h"  

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <input_file>" << endl;
        return 1;
    }

    string inputFilename = argv[1];

    std::ofstream outFile("../output.txt");
    std::streambuf *coutbuf = std::cout.rdbuf(); 
    std::cout.rdbuf(outFile.rdbuf()); 

    Simulator sim;
    sim.load_program_memory(inputFilename);  
    sim.run_RISCVSim();  

    return 0;
}