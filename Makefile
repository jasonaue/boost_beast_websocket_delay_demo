all:
	g++ --std=c++1z main.cpp -lpthread -lssl -lcrypto -o main
