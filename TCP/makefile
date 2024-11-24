
CXXFLAGS=-std=c++17 -g -Wall

CC=g++ 

OBJECTS=server.o client.o

#server: client server.o
#	$(CC) *.o -o server $(CXXFLAGS)

#server.o: server.cpp client 
	$(CC) -c server.cpp $(CXXFLAGS)

#client: client.o 
	$(CC) client.o -o client $(CXXFLAGS)

#client.o: client.cpp 
	$(CC) -c client.cpp $(CXXFLAGS)

clean:
	rm -f server client

