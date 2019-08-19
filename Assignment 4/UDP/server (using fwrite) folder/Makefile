server: udpserver.cpp
	g++ udpserver.cpp -lssl -lcrypto -o server
	
clean:
	rm server *Sample*
	
run: server
	./server 8112
