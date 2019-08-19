#include <stdio.h>		//for stderr,fprintf,stderr,printf,sprintf
#include <stdlib.h>		//for atoi,exit
#include <string.h>		//for bzero,bcopy,strcpy,strcat,strlen,strcmp
#include <unistd.h>		//for read,close,write
#include <arpa/inet.h>		//for inet_aton,netinet/in.h,inttypes.h
#include <netdb.h>		//for sockaddr_in,AF_INET,SOCK_STREAM,socket,gethostbyname,hostent,htons,connect
#include <fcntl.h>		//for open,file permissions,opening modes
#include <iostream>
#include <openssl/md5.h>	//for md5 checksum functions
#include <sys/wait.h>
#include <sys/stat.h>        // for stat
#include <errno.h>           //for errno
#include <signal.h>

#define SLEEP_VAL 5
static int alarm_fired = 0;
void mysig(int sig)
{
	pid_t pid;
	printf("PARENT : Received signal %d \n", sig);
	if (sig == SIGALRM)
	{
		alarm_fired = 1;
	}
}

#define HEAD0SIZE sizeof(bool)
#define HEAD1SIZE sizeof(unsigned int)
#define HEAD2SIZEDATA sizeof(int)
#define HEAD2SIZEACK sizeof(unsigned int)
#define HEADSIZEDATA (HEAD0SIZE+HEAD1SIZE+HEAD2SIZEDATA)
#define HEADSIZEACK (HEAD0SIZE+HEAD1SIZE+HEAD2SIZEACK)
#define MAXCHUNKSIZE 1024    // MSS
#define MAXPACKETSIZE (HEADSIZEDATA+MAXCHUNKSIZE) // data packet size always > ack packet size
#define SENDERBUFSIZE MAXCHUNKSIZE*50
#define TIMEOUT_SEC 1
#define TIMEOUT_USEC 0
#define TIMEOUT_COUNT 10
#define INITIALWINDOWSIZE 3*MAXCHUNKSIZE
#define INITIALSSTHRESHOLD SENDERBUFSIZE
#define DATA true
#define ACK false
	
using namespace std;

char senderbuffer[SENDERBUFSIZE];
char packet[MAXPACKETSIZE];
unsigned seqno = 0, ackno = 0;
unsigned bufstart = 0, bufend = 0, filled = 0;
unsigned cwnd = INITIALWINDOWSIZE, rwnd = INITIALWINDOWSIZE, windowsize = INITIALWINDOWSIZE, ssthreshold = INITIALSSTHRESHOLD;
enum updatewindowconditions{ TIMEOUT, TRIPLEDUPACK, NEWACKBEFORECCTRIGGER, NEWACKAFTERCCTRIGGER};


int main(int argc, char* argv[])
{	
	int sockfd,timeouts;
	unsigned short portno;
	struct sockaddr_in serveraddr;
	socklen_t serverlen = sizeof(serveraddr);
	char *hostname, *chunk;
	
	if (argc != 4)
	{
		 fprintf(stderr,"usage: %s <hostaddr(IPv4)> <port> <filename>\n", argv[0]);
		 exit(0);
	}
	
	hostname = argv[1];
	portno = (unsigned short)atoi(argv[2]);
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{ 
		cerr<<"ERROR opening socket\n";
		exit(0);
	}
	
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(portno);
	inet_aton(hostname,&serveraddr.sin_addr);
	
	struct timeval timeout;      
	socklen_t timeout_len;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	
	//getsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, &timeout_len);
	
	cout<<"Default timeout sec  = "<<timeout.tv_sec<<endl;
	cout<<"Default timeout usec = "<<timeout.tv_usec<<endl;
	
	cout<<sizeof(timeout)<<endl;
	cout<<timeout_len<<endl;
	
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	
	
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		cerr<<"ERROR : setsockopt failed\n";
		exit(0);
	}
	
	alarm(SLEEP_VAL);
	
	(void) signal(SIGALRM, mysig);
	
	int n1 = recvfrom(sockfd, packet, MAXPACKETSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
	//int n1 = recv(sockfd, packet, MAXPACKETSIZE, 0);
	if(n1 < 0 && errno != EWOULDBLOCK)
	{
		cerr<<"ERROR reading from socket with errno = "<<errno<<endl;
		//exit(0);
	}
	
	cout<<"Now came out of recvfrom\n";
	
}
