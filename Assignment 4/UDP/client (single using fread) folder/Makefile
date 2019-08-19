client:	udpclient.cpp
	g++ udpclient.cpp -lssl -lcrypto -o client
	
clean: 
	rm *client
	
run: client SampleTextFile_1200kb.txt
	./client 10.117.5.144 8112 SampleTextFile_1200kb.txt
