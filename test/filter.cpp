#include <iostream>
#include <fstream>
#include <sstream>

int main() {
    std::ifstream infile("../test/output.mc");
    std::ofstream outfile("../test/output1.mc");
    std::string line;

    if (!infile || !outfile) {
        std::cerr << "Error opening file!" << std::endl;
        return 1;
    }

    while (std::getline(infile, line)) {
        if (line.find("TERMINATED") != std::string::npos) {
            continue;
        }

        std::istringstream ss(line);
        std::string pc, ir;
        
        if (ss >> pc && ss.ignore() && std::getline(ss, ir, ',')) {
            outfile << pc << " " << ir << std::endl;
        }
    }

    infile.close();
    outfile.close();
    return 0;
}