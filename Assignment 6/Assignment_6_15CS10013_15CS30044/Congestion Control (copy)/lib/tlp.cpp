#include <stdio.h>		//for stderr,fprintf,stderr,printf,sprintf
#include <stdlib.h>		//for atoi,exit
#include <string.h>		//for bzero,bcopy,strcpy,strcat,strlen,strcmp
#include <unistd.h>		//for read,close,write
#include <arpa/inet.h>		//for inet_aton,netinet/in.h,inttypes.h
#include <netdb.h>		//for sockaddr_in,AF_INET,SOCK_STREAM,socket,gethostbyname,hostent,htons,connect
#include <fcntl.h>		//for open,file permissions,opening modes
#include <iostream>
// #include <openssl/md5.h>	//for md5 checksum functions
#include <sys/wait.h>
#include <sys/stat.h>        // for stat
#include <errno.h>           //for errno
#include <queue>
#include <utility>
#include <pthread.h>
#include <vector>
#include <sys/time.h>

#define HEAD0SIZE sizeof(bool)
#define HEAD1SIZE sizeof(unsigned int)
#define HEAD2SIZE sizeof(int)
#define HEADSIZE (HEAD0SIZE+HEAD1SIZE+HEAD2SIZE)
#define CHUNKSIZE 1024
#define BUFSIZE (HEADSIZE+CHUNKSIZE)
#define TIMEOUT_SEC 0
#define TIMEOUT_USEC 5000
#define TIMEOUT_COUNT 10
//#define WINDOWSIZE 3
#define INITIALWINDOWSIZE 3
#define MAXWINDOWSIZE 10
#define MAXWINDOWSIZE2 10*CHUNKSIZE

using namespace std;


unsigned int ackno = 0, sn = 0, rn = 0;
char s[MAXWINDOWSIZE2],r[MAXWINDOWSIZE2]; 
pthread_mutex_t sm = PTHREAD_MUTEX_INITIALIZER, rm = PTHREAD_MUTEX_INITIALIZER, wm = PTHREAD_MUTEX_INITIALIZER;
int sockfd;
float dropprob;

//socklen_t palen;
pthread_t tid[2];

enum cccond{ TIMEOUT, TRIPLEDUPACK, GOODNETWORK};

void* recvbuffer_handle(void *ptr);
void* sendbuffer_handle(void *ptr);

void createthreads()
{
	pthread_t tid1,tid2;
 	pthread_attr_t a;
 	pthread_attr_init(&a);
    pthread_create(&tid1, &a, &recvbuffer_handle, NULL);
    pthread_attr_destroy(&a);
    pthread_attr_init(&a);
    pthread_create(&tid2, &a, &sendbuffer_handle, NULL);
    pthread_attr_destroy(&a);
}

int min(int a, int b)
{
	return a < b? a: b;
}

int max(int a, int b)
{
	return a > b? a: b;
}

void setsockfd(int fd)
{
	sockfd = fd;
}

void setdropprob(float dp)
{
	dropprob = dp;
}

bool identify_packet(char* packet)
{
	if(packet == NULL)
	{
		cerr<<"function identify_packet called with NULL value\n";
		exit(0);
	}
	bool type;
	bcopy(packet, &type, HEAD0SIZE);
	cerr<<"identify_packet: type"<<type<<endl;
	return type;
}

int encode(char* buf, unsigned int seq, int n, bool type)
{
	bcopy(&type, buf, HEAD0SIZE);
	bcopy(&seq, buf + HEAD0SIZE, HEAD1SIZE);
	bcopy(&n, buf + HEAD0SIZE + HEAD1SIZE, HEAD2SIZE);
	return n + HEADSIZE;
}

int decode(char* buf, unsigned int &seq, int n)
{
	int n1;
	buf = buf + HEAD0SIZE;
	bcopy(buf, &seq, HEAD1SIZE);
	bcopy(buf + HEAD1SIZE, &n1, HEAD2SIZE);
	if(n1+ (int)HEADSIZE == n)
		return n1;
	else
		return -1;
}

void rate_control(char* ack, int n1)
{
	unsigned int tackno;
	n1 = decode(ack,tackno,n1);
	if(n1 < 0)
	{
		cerr<<"ERROR in the acknowledgement\n";
		exit(0);
	}
	cout<<"Acknowledgement recieved with acknowledgement number "<<tackno<<".\n";
	pthread_mutex_lock(&wm);
	if(tackno > ackno)
		ackno = tackno;
	pthread_mutex_unlock(&wm);
}

void* recvbuffer_handle(void *ptr)
{
	int n,n1;
	unsigned int rackno = 0, rseqno;
	struct sockaddr_in clientaddr;
	socklen_t clientlen;
	float randomfloat;
	char buf[BUFSIZE], ack[BUFSIZE];
	char *chunk = buf + HEADSIZE;
	while(true)
	{
		//ackno++;
		while(1)
		{
			bzero(buf,BUFSIZE);
			n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
			if(n < 0)
			{
				cerr<<"ERROR reading from socket\n";
				exit(0);
			}
			clientlen = sizeof(clientaddr);
			randomfloat = (float)rand()/RAND_MAX;
			cout<<randomfloat<<'\t'<<dropprob<<endl;
			if(randomfloat < dropprob)
			{
				unsigned int tseqno;
				decode(buf,tseqno,n);
				cout<<"Dropped packet with sequence number "<<tseqno<<".\n";
				continue;
			}
		}
		if(identify_packet(buf) == 0)		//ack
		{
			rate_control(buf,n);
		}
		else								//data
		{
			n = decode(buf,rseqno,n);
			if(n < 0)
			{
				cerr<<"ERROR in the data\n";
				continue;
			}
			if(rseqno == rackno)
			{
				//sizeoffile-=n;
				//fwrite(chunk,sizeof(char),n,fout);
				if(rn >= MAXWINDOWSIZE2)
				{
					cout<<"Reciever buffer full\n";
					continue;
				}
				pthread_mutex_lock(&rm);
				//n1 = min(n, MAXWINDOWSIZE2 - rn);
				bcopy(chunk, r + rn, n);
				rn+=n;
				pthread_mutex_unlock(&rm);
				//cout<<chunk<<endl;
				bzero(ack,BUFSIZE);
				n1 = encode(ack,rackno,0,0);
				n = sendto(sockfd, ack, n1, 0, (struct sockaddr *) &clientaddr, clientlen);
				if(n < 0)
				{
					cerr<<"ERROR writing to socket\n";
					exit(0);
				}
				cout<<"Acknowledgement sent with acknowledgement number "<<rackno<<".\n";
				rackno++;
			}
			else if(rseqno < rackno)
				cout<<"Duplicate packet recieved with sequence number "<<rseqno<<".\n";
			else
				cout<<"Out of order packet recieved with sequence number "<<rseqno<<".\n";
		}
	}
}

int apprecv(char *data, int n)
{
	int n1;
	while(rn == 0)
	{
		cout<<"Recieve buffer empty";
		sleep(1);
	}
	pthread_mutex_lock(&rm);
	n1 = min(n, rn);
	bcopy(r, data, n1);
	bcopy(r + n1, r, MAXWINDOWSIZE2 - n1);
	rn -= n1;
	pthread_mutex_unlock(&rm);
	data[n1] = '\0';
	return n1;
}

/*
void update_window(int &cwnd, int &ssthres, cccond cond)
{
	if(cond == TIMEOUT || cond == TRIPLEDUPACK)
	{
		ssthres = cwnd/2;
		if(ssthres < MAXPAYLOADSIZE)
			ssthres = MAXPAYLOADSIZE;
	}
	if(cond == TIMEOUT)
	{
		cwnd = MAXPAYLOADSIZE;
	}
	else if(cond == TRIPLEDUPACK)
	{
		cwnd = ssthres;
	}
	else if(cond == GOODNETWORK)
	{
		if(cwnd + MAXPAYLOADSIZE <= ssthres)
			cwnd += MAXPAYLOADSIZE;
		else
		{
			cwnd += MAXPAYLOADSIZE/cwnd;
			if(cwnd > ssthres)
				ssthres = cwnd;
		}
	}
}
//*/

void* sendbuffer_handle(void *ptr)
{
	struct timeval sendt, difft, now;
	char *chunk;
    char *buf = (char *)malloc(sizeof(char)*BUFSIZE*INITIALWINDOWSIZE);
	int *n = (int *)malloc(sizeof(int)*INITIALWINDOWSIZE);
	int n1;
	unsigned int seqno = 0, sackno, oldackno, discard, outstanding, WINDOWSIZE = INITIALWINDOWSIZE, timeouts;
	pthread_mutex_lock(&wm);
	sackno = ackno;
	pthread_mutex_lock(&wm);
	oldackno = sackno;
	while(true)
	{
		while(sn >= 0 || sackno < seqno)
		{
			discard = sackno - oldackno;
			outstanding = seqno - sackno;
			bcopy(buf + discard*BUFSIZE, buf, outstanding*BUFSIZE);
			bcopy(n + discard, n, outstanding*sizeof(int));
			while(sn >= 0 && outstanding < WINDOWSIZE)
			{
				seqno++;
				bzero(buf + outstanding*BUFSIZE, BUFSIZE);
				chunk = buf + outstanding*BUFSIZE + HEADSIZE;
				pthread_mutex_lock(&sm);
				n[outstanding] = min(sn, CHUNKSIZE);
				bcopy(s, chunk, n[outstanding]);
				sn -= n[outstanding];
				bcopy(s + n[outstanding], s, sn);
				pthread_mutex_unlock(&sm);
				n[outstanding] = encode(buf + outstanding*BUFSIZE, seqno, n[outstanding],1);
				n1 = send(sockfd , buf + outstanding*BUFSIZE, n[outstanding], 0);
				if (n1 < 0)
				{
					cerr<<"ERROR writing to socket\n";
					exit(0);
				}
				cout<<"Packet sent with sequence number "<<seqno<<".\n";
				outstanding++;
			}
			oldackno = sackno;
			timeouts = 0;
			gettimeofday(&sendt, NULL);
			while(1)
			{
				while(ackno == sackno)
				{
					usleep(5000);
					gettimeofday(&now, NULL);
					timersub(&sendt, &now, &difft);
					if(difft.tv_sec > 0)
					{
						timeouts++;
						cout<<"No. of times timeout has occured : "<<timeouts<<endl;
						for(unsigned int i=0; i<outstanding; i++)
						{
							//char temp[BUFSIZE];
							//bcopy(buf + i*BUFSIZE, temp, BUFSIZE);
							//cout<<temp+4<<endl;
							//cout<<*((int *)temp)<<endl;
							//cout<<n[i]<<endl;
							n1 = send(sockfd , buf + i*BUFSIZE, n[i], 0);
							if (n1 < 0)
							{ 
								cerr<<"ERROR writing to socket\n";
								exit(0);
							}
							cout<<"Duplicate packet sent with sequence number "<<oldackno+i+1<<".\n";
						}
						WINDOWSIZE /= 2;
						if(WINDOWSIZE == 0)
							WINDOWSIZE = 1;
						continue;
					}
				}
				pthread_mutex_lock(&wm);
				sackno = ackno;
				pthread_mutex_unlock(&wm);
				cout<<"Acknowledgement recieved with acknowledgement number "<<sackno<<".\n";
			}
			if(timeouts == 0)
				WINDOWSIZE += 1;
			if(WINDOWSIZE > outstanding)
			{
				char* newbuf = (char *)realloc(buf, sizeof(char)*BUFSIZE*WINDOWSIZE);
				int* newn = (int *)realloc(n, sizeof(int)*WINDOWSIZE);
				if(newbuf == NULL || newn == NULL)
				{
					cout<<"ERROR : No space to increase the window size.\n";
					exit(0);
					WINDOWSIZE -= 1;
				}
				else
				{
					buf = newbuf;
					n = newn;
				}
			}
		}
	}
}

void appsend(char *data, int n)
{
	//acquire lock
	//cond_wait()
	//check if buffer free otherwise goto cond_wait()
	//put data on send buffer if data not put completely goto cond_wait()
	//release lock
	//int n1, w = 0;
	while(sn >= MAXWINDOWSIZE2)
	{
		cout<<"Sender buffer full\n";
		sleep(1);
	}
	pthread_mutex_lock(&sm);
	//n1 = min(n - w,MAXSBUFSIZE - sbuf.second);
	bcopy(data, s + sn, n);
	sn += n;
	pthread_mutex_unlock(&sm);
}
