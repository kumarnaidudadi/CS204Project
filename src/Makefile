all:
	clang++ -std=c++20 assembler.cpp -o ../bin/assembler
	../bin/assembler
	clang++ -std=c++20 myRISCVSim.cpp main.cpp -I../include -o ../bin/myRISCVSim
	clang++ -std=c++20 ../test/filter.cpp -o ../test/filter
	../test/filter
	../bin/myRISCVSim ../test/output1.mc

clean:
	rm -f *.o *~ *.bak ../bin/myRISCVSim ../test/filter
