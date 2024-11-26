CXXFLAGS=-std=c++17 -g -Wall

TARGET=client server 

normal: $(TARGET)

client:client.cpp
	g++ $(CXXFLAGS) client.cpp -o client

server:server.cpp
	g++ $(CXXFLAGS) server.cpp -o server

clean:
	rm -f $(TARGET) 