all: lpaq1_stream lpaq1

lpaq1: lpaq1.cpp
	g++ -O3 lpaq1.cpp -o lpaq1

lpaq1_stream: lpaq1_stream.cpp
	g++ -O3 lpaq1_stream.cpp -o lpaq1_stream

