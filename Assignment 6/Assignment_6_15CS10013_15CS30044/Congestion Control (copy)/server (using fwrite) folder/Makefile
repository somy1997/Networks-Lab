server: udpserver.cpp
	cd ../lib && $(MAKE)
	g++ -Wall -g udpserver.cpp -L../lib -ltlp -lssl -lcrypto -lpthread -o server
	
clean:
	rm server *Sample*
	
run: server
	./server 8112 0.5
