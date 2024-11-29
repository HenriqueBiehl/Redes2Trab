CXXFLAGS=-std=c++17 -g -Wall

TARGET=client server compare

OBJECTS=lib_network.o

normal: $(TARGET)

client:client.cpp lib_network.o 
	g++ $(CXXFLAGS) client.cpp lib_network.o -o client

server:server.cpp lib_network.o 
	g++ $(CXXFLAGS) server.cpp lib_network.o -o server

compare:compare.cpp lib_network.o
	g++ $(CXXFLAGS) compare.cpp lib_network.o -o compare

lib_network.o:lib_network.cpp 
	g++ $(CXXFLAGS) lib_network.cpp -c lib_network.o 

clean:
	rm -f $(TARGET) $(OBJECTS) 