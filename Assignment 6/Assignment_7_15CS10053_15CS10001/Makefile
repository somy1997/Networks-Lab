all:	sender receiver
sender:	wrapper.c
	gcc -o sender wrapper.c -lpthread
	
receiver:	recv.c
	gcc -o receiver recv.c -lpthread
	
clean:
	rm sender receiver
