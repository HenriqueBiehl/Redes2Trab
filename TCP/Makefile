CXXFLAGS=-std=c++17 -g -Wall

TARGET=client server 

OBJECTS=lib_network.o

normal: $(TARGET)

client:client.cpp lib_network.o
	g++ $(CXXFLAGS) client.cpp lib_network.o -o client

server:server.cpp lib_network.o
	g++ $(CXXFLAGS) server.cpp lib_network.o -o server

lib_network.o: lib_network.cpp 
	g++ $(CXXFLAGS) -c lib_network.cpp -o lib_network.o

clean:
	rm -f $(TARGET) $(OBJECTS)
