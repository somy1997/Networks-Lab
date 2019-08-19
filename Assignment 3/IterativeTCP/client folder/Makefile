client:	tcpclient.cpp
	g++ tcpclient.cpp -lssl -lcrypto -o client

clean: 
	rm client
	
run: client SampleTextFile_1200kb.txt
	./client 127.0.0.1 8112 SampleTextFile_1200kb.txt
