all: lpaq1_stream lpaq1 classify

lpaq1: lpaq1.cpp
	g++ -O3 lpaq1.cpp -o lpaq1

lpaq1_stream: lpaq1_stream.cpp bit_predictor.cpp
	g++ -std=c++11 -O3 lpaq1_stream.cpp bit_predictor.cpp -o lpaq1_stream
	
classify: bit_predictor.cpp classify.cpp
	g++ -std=c++11 -O3 classify.cpp bit_predictor.cpp -o classify
