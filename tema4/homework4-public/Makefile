CC=g++
CFLAGS=-I. -g

client: client.cpp requests.cpp helpers.cpp buffer.cpp
	$(CC) -o client -g client.cpp requests.cpp helpers.cpp buffer.cpp -Wall

run: client
	./client

clean:
	rm -f *.o client
