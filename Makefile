all: lpaq1_stream lpaq1 classify predictorcli

CXXFLAGS+=-std=c++11 -O3

lpaq1: lpaq1.cpp
	g++ -O3 lpaq1.cpp -o lpaq1

lpaq1_stream: lpaq1_stream.o bit_predictor.o
	g++ $^ -o $@

classify: classify.o bit_predictor.o
	g++ $^ -o $@
	
predictorcli: predictorcli.o bit_predictor.o
	g++ $^ -o $@
