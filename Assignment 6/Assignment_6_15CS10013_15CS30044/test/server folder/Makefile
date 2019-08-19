servermake : server.cpp
	cd ../lib && $(MAKE)
	g++ -Wall -g server.cpp -o server -L../lib -ltlp -lpthread -lcrypto -lssl
run : server
	./server 8111
