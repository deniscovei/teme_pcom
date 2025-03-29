all: server subscriber

server:
	g++ -g -std=c++20 server.cpp -o server

subscriber:
	g++ -g -std=c++20 tcp_client.cpp -o subscriber

PORT=12342

run_server:
	./server $(PORT)

run_subscriber:
	./subscriber C1 127.0.0.1 $(PORT)

run_udp:
	./start_udp.sh $(PORT)

kill:
	pkill server && pkill subscriber

clean:
	rm -f server subscriber
