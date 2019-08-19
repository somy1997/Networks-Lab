multiclient: tcpmulticlient.cpp
	g++ tcpmulticlient.cpp -lssl -lcrypto -o multiclient

clean: 
	rm *client
	
run: multiclient SampleTextFile_1200kb.txt
	./multiclient 10.117.5.144 8112 SampleTextFile_1200kb.txt 10
