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

/*

int and unsigned are gonna cause trouble
size_t is unsigned whereas ssize_t is signed
buffer for putting payload data by the app
seqno the starting sequence number of the next packet to be sent
ackno is the next expected seqno in the packet
seqno == ackno means all the packets are recieved
create packet picking data from the buffer starting from bufstart till bufend/MSS
bufstart incremented only when the data sent has been acknowledged
create_packet should get seqno and number of bytes to be put
it should take the given number of bytes from the senderbuffer and put it on the packet
it should get the starting address and handle the circular queue nature of the buffer
it also gets the address where to put the packet
its calling function would decide the number of bytes to be put by comparing windowsize, outstanding and filledsize
rate_control goes to sleep if there is no data in the buffer and the all the packets have been acked
timeout set in the main()
only appsend and rate_control run in different threads
use sigsuspend() instead of pause()
if(value) may create problem as this would be true in the case value is negative too
reference variable not needed in create_packet()
triple dup ack only checked for oldackno as oldackno is the latest one.
keeping initial oldackno as 0 doesn't cause problem as first ackno recieved should be greater than first seqno(=0)
need to send info whether packet is ack or data, use boolean
rwnd implementation
parse_ackpacket updates rwnd;
new functions create_ackpacket and parse_datapacket after using identify_packet

*/

char senderbuffer[SENDERBUFSIZE];
char packet[MAXPACKETSIZE];
unsigned seqno = 0, ackno = 0;
unsigned bufstart = 0, bufend = 0, filled = 0;
unsigned cwnd = INITIALWINDOWSIZE, rwnd = INITIALWINDOWSIZE, windowsize = INITIALWINDOWSIZE, ssthreshold = INITIALSSTHRESHOLD;
enum updatewindowconditions{ TIMEOUT, TRIPLEDUPACK, NEWACKBEFORECCTRIGGER, NEWACKAFTERCCTRIGGER};

int create_datapacket(unsigned payloadstart, unsigned int seqno, int n, char* packet)
{
	bool type = DATA;
	bzero(packet, MAXPACKETSIZE);
	bcopy(&type, packet, HEAD0SIZE);
	bcopy(&seqno, packet + HEAD0SIZE, HEAD1SIZE);
	bcopy(&n, packet + HEAD0SIZE + HEAD1SIZE, HEAD2SIZEDATA);
	int carryover = payloadstart + n - SENDERBUFSIZE;
	if(carryover <= 0)
	{
		bcopy(senderbuffer + payloadstart, packet + HEADSIZEDATA, n);
	}
	else
	{
		bcopy(senderbuffer + payloadstart, packet + HEADSIZEDATA, n - carryover);
		bcopy(senderbuffer, packet + HEADSIZEDATA + n - carryover, carryover);
	}
	return HEADSIZEDATA + n;
}

int create_ackpacket(unsigned int ackno, unsigned int rwnd, char* packet)
{
	bool type = ACK;
	bzero(packet, MAXPACKETSIZE);
	bcopy(&type, packet, HEAD0SIZE);
	bcopy(&ackno, packet + HEAD0SIZE, HEAD1SIZE);
	bcopy(&rwnd, packet + HEAD0SIZE + HEAD1SIZE, HEAD2SIZEACK);
	return HEADSIZEACK;
}

int parse_ackpacket(unsigned int &ackno, unsigned int &rwnd, int n, char* packet)
{
	if(n != HEADSIZEACK);
		return -1;
	bcopy(packet + HEAD0SIZE, &ackno, HEAD1SIZE);
	bcopy(packet + HEAD0SIZE + HEAD1SIZE, &rwnd, HEAD2SIZEACK);
	return 0;
}

int parse_datapacket(unsigned &bufend, unsigned &filled, unsigned &ooobufend, unsigned &ooocount, unsigned int &ackno, int n, char* packet)
{
	int n1;
	unsigned seqno;
	bcopy(packet + HEAD0SIZE, &seqno, HEAD1SIZE);
	bcopy(packet + HEAD0SIZE + HEAD1SIZE, &n1, HEAD2SIZEDATA);
	if(n1 + HEADSIZE != n)
	{
		return -1;
	}
	// returns the number of bytes by which to increase filled of reciever buffer
	if(seqno <= ackno)
	{
		n1 = seqno - ackno + n1;
		n1 = n1 < (RECVERBUFSIZE - filled)? n1: (RECVERBUFSIZE - filled);
		int carryover = bufend + n1 - RECVERBUFSIZE;
		if(carryover <= 0)
		{
			bcopy(packet + HEADSIZE, RECVERBUFSIZE + bufend, n1);
		}
		else
		{
			bcopy(packet + HEADSIZE, RECVERBUFSIZE + bufend, n1 - carryover);
			bcopy(packet + HEADSIZE + n1 - carryover, RECVERBUFSIZE, carryover);
		}
		bufend = (bufend + n1)%RECVERBUFSIZE);
		filled += n1;
		ackno += n1;
		int n2 = (ooobufend - bufend + RECVERBUFSIZE)%RECVERBUFSIZE;
		if(ooocount > 0 && n2 <= ooocount)
		{
			bufend = ooobufend;
			filled += n2;
			ackno += n2;
			n1 += n2;
		}
		ooocount = 0;
	}
	else
	{
		int n2 = n1;
		n1 = 0;
		unsigned ooobufload = (bufend + seqno - ackno)%RECVERBUFSIZE;
		if(ooocount > 0 && ooobufload == ooobufend)
		{
			unsigned bufstart = (bufend - filled + RECVERBUFSIZE)%RECVERBUFSIZE;
			int n3 = (bufstart - ooobufload + RECVERBUFSIZE)%RECVERBUFSIZE;
			n2 = n2 < n3? n2: n3;
			int carryover = ooobufload + n2 - RECVERBUFSIZE;
			if(carryover <= 0)
			{
				bcopy(packet + HEADSIZE, RECVERBUFSIZE + ooobufload, n2);
			}
			else
			{
				bcopy(packet + HEADSIZE, RECVERBUFSIZE + ooobufload, n2 - carryover);
				bcopy(packet + HEADSIZE + n2 - carryover, RECVERBUFSIZE, carryover);
			}
			ooobufcount += n2;
			ooobufend = (ooobufend + n2)%RECVERBUFSIZE;
		}
		else if(ooocount == 0)
		{
			unsigned lostbytes = (ooobufload - bufend + RECVERBUFSIZE)%RECVERBUFSIZE;
			if(lostbytes <= MAXCHUNKSIZE)
			{
				unsigned bufstart = (bufend - filled + RECVERBUFSIZE)%RECVERBUFSIZE;
				int n3 = (bufstart - ooobufload + RECVERBUFSIZE)%RECVERBUFSIZE;
				n2 = n2 < n3? n2: n3;
				int carryover = ooobufload + n2 - RECVERBUFSIZE;
				if(carryover <= 0)
				{
					bcopy(packet + HEADSIZE, RECVERBUFSIZE + ooobufload, n2);
				}
				else
				{
					bcopy(packet + HEADSIZE, RECVERBUFSIZE + ooobufload, n2 - carryover);
					bcopy(packet + HEADSIZE + n2 - carryover, RECVERBUFSIZE, carryover);
				}
				ooobufcount = n2;
				ooobufend = (ooobufload + n2)%RECVERBUFSIZE;
			}
		}
	}
	return n1;
}

void signalhandler(int signo)
{
	;
}

void sendbuffer_handle(char* chunk, int n)
{
	unsigned written = 0;
	signal(SIGUSR1, signalhandler);
	while(written < n)
	{
		if(filled == SENDERBUFSIZE)
			pause();
		unsigned space = SENDERBUFSIZE - filled;
		unsigned tobewritten = (n - written) < space? n - written: space;
		int carryover = bufend + tobewritten - SENDERBUFSIZE;
		if(carryover <= 0)
		{
			bcopy(chunk + written, senderbuf + bufend, tobewritten);
		}
		else
		{
			bcopy(chunk + written, senderbuf + bufend, tobewritten - carryover);
			bcopy(chunk + written + tobewritten - carryover, senderbuf, carryover);
		}
		filled += tobewritten;
		bufend = (bufend + tobewritten)%SENDERBUFSIZE;
		written += tobewritten;
	}
}

void appsend()
{
	//for multiple app, finds the corresponding buffer and calls sendbuffer_handle with it
}


int udpsend(int sockfd, char* packet, int n0, int flags, struct sockaddr* serveraddrptr, socklen_t serverlen)
{
	int n1 = sendto(sockfd , packet, n0, flags, serveraddrptr, serverlen);
	if (n1 < 0)
	{
		cerr<<"ERROR writing to socket\n";
		exit(0);
	}
	return n1;
}

int udprecieve(int sockfd, char* packet, int n0, int flags, struct sockaddr* serveraddrptr, socklen_t serverlen)
{
	n1 = recvfrom(sockfd, packet, MAXPACKETSIZE, flags, serveraddrptr, &serverlen);
	if(n1 < 0 && errno != EWOULDBLOCK)
	{
		cerr<<"ERROR reading from socket\n";
		exit(0);
	}
	return n1;
}

void rate_control()
{
	unsigned timeouts, dupackcount;
	oldackno = ackno;
	while(true)
	{
		int n, n0, n1;
		discard = ackno - oldackno;
		outstanding = seqno - ackno;
		bufstart = (bufstart + discard)%SENDERBUFSIZE;
		filled -= discard;
		oldackno = ackno;
		if(filled == 0 && ackno == seqno)
		{
			pause();
			continue; // continue because this thread may be interrupted by signal from some other thread
		}
		windowsize = cwnd < rwnd? cwnd: rwnd;
		while(outstanding < filled && outstanding < windowsize)
		{
			n = filled - outstanding;
			n = n < windowsize - outstanding? n: outstanding;
			n = n < MAXCHUNKSIZE? n: MAXCHUNKSIZE;
			n0 = create_packet(bufstart, seqno, n, packet);
			n1 = udpsend(sockfd, packet, n0, 0, (struct sockaddr *) &serveraddr, serverlen);
			cout<<"Packet sent with sequence number "<<seqno<<" and payload of size "<<n<<" bytes.\n";
			seqno += n;
			outstanding += n;
		}
		timeouts = 0;
		dupackcount = 0;
		while(1)
		{
			bzero(packet, MAXPACKETSIZE);
			n1 = udprecv(sockfd, packet, MAXPACKETSIZE, 0, (struct sockaddr *) &serveraddr, serverlen);
			if(n1 < 0)
			{
				timeouts++;
				dupackcount = 0;
				update_window(TIMEOUT);
				cout<<"No. of times timeout has occured : "<<timeouts<<endl;
				unsigned int dupsent = 0, dupseqno;
				while(dupsent < outstanding)
				{
					dupseqno = oldackno + dupsent;
					n = MAXCHUNKSIZE < outstanding - dupsent? MAXCHUNKSIZE: outstanding - dupsent;
					n0 = create_packet( (bufstart + dupsent)%SENDERBUFSIZE, dupseqno, n, packet);
					n1 = udpsend(sockfd , packet, n0, 0, (struct sockaddr *) &serveraddr, serverlen);
					cout<<"Duplicate packet sent with sequence number "<<dupseqno<<" and payload of size "<<n<<" bytes.\n";
					dupsent += n;
				}
				continue;
			}
			n1 = parse_ackpacket(ack,ackno,n1,packet);
			if(n1 < 0 || ackno > seqno)
			{
				cerr<<"ERROR in the acknowledgement\n";
				exit(0);
			}
			cout<<"Acknowledgement recieved with acknowledgement number "<<ackno<<".\n";
			if(seqno - ackno < outstanding)
			{
				if(dupackcount >= 3 || timeouts > 0)
				{
					update_window(NEWACKAFTERCCTRIGGER);
				}
				else
				{
					update_window(NEWACKBEFORECCTRIGGER);
				}
				break;
			}
			if(ackno == oldackno)
			{
				dupackcount++;
				if(dupackcount%3 == 0)
				{
					update_window(TRIPLEDUPACK);
					dupseqno = oldackno;
					n = MAXCHUNKSIZE < outstanding? MAXCHUNKSIZE: outstanding;
					n0 = create_packet( bufstart, dupseqno, n, packet);
					n1 = udpsend(sockfd , packet, n0, 0, (struct sockaddr *) &serveraddr, serverlen);
					cout<<"Duplicate packet sent with sequence number "<<dupseqno<<" and payload of size "<<n<<" bytes.\n";
				}
			}
		}
	}	
}

int main(int argc, char **argv)
{
		
	int sockfd,*n,n1,timeouts;
	unsigned short portno;
	struct sockaddr_in serveraddr;
	socklen_t serverlen = sizeof(serveraddr);
	char *hostname, *chunk;
	char *buf, ack[BUFSIZE];
	buf = (char *)malloc(sizeof(char)*BUFSIZE*INITIALwindowsize);
	n = (int *)malloc(sizeof(int)*INITIALwindowsize);
	unsigned int seqno = 0, oldackno, ackno, discard, outstanding, windowsize = INITIALwindowsize;
	
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
	timeout.tv_sec = TIMEOUT_SEC;
	timeout.tv_usec = TIMEOUT_USEC;
	
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		cerr<<"ERROR : setsockopt failed\n";
		exit(0);
	}
	
	char filename[100];
	sprintf(filename,"%s",argv[3]);
	long int sizeoffile,numberofchunks;
	char str[21];
	
	struct stat filestat;
	if(stat(filename,&filestat)<0)
	{
		cerr<<"ERROR finding stats of file : "<<filename<<endl;
		exit(0);
	}
	sizeoffile = filestat.st_size;
	cout<<sizeoffile<<endl;
	numberofchunks = sizeoffile%CHUNKSIZE? (sizeoffile/CHUNKSIZE+1): (sizeoffile/CHUNKSIZE);
	cout<<numberofchunks<<endl;
	
	printf("Sending file details : ");

	bzero(buf, windowsize*BUFSIZE);
	chunk = buf + HEADSIZE;
	strcpy(chunk,filename);
	strcat(chunk,"$");
	sprintf(str,"%ld",sizeoffile);
	strcat(chunk,str);
	strcat(chunk,"$");
	sprintf(str,"%ld",numberofchunks);
	strcat(chunk,str);
	
	cout<<chunk<<endl;
}
