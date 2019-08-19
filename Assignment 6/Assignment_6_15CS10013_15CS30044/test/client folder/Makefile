clientmake :
	cd ../lib && $(MAKE)
	g++ -Wall -g client.cpp -o client -L../lib -ltlp -lpthread -lcrypto -lssl
run : client
	./client 127.0.0.1 8111 SampleTextFile_1200kb.txt
