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
#include <time.h>

#define HEAD0SIZE sizeof(bool)	// packet type
#define HEAD1SIZE sizeof(unsigned int)	// seqno / ackno
#define HEAD2SIZEDATA sizeof(int)	// size of payload
#define HEAD2SIZEACK sizeof(int)	// rwnd
#define HEADSIZEDATA (HEAD0SIZE+HEAD1SIZE+HEAD2SIZEDATA)
#define HEADSIZEACK (HEAD0SIZE+HEAD1SIZE+HEAD2SIZEACK)
#define MAXPAYLOADSIZE 1024    // MSS
#define MAXACKSIZE HEADSIZEACK
#define MAXPACKETSIZE (HEADSIZEDATA+MAXPAYLOADSIZE) // data packet size always > ack packet size
#define MAXRBUFSIZE MAXPAYLOADSIZE*50
#define MAXSBUFSIZE MAXPAYLOADSIZE*50
#define TIMEOUT_SEC 1
#define TIMEOUT_USEC 0
#define TIMEOUT_COUNT 10
#define INITIALWINDOWSIZE (MAXPAYLOADSIZE*3)
#define INITIALSSTHRESHOLD (MAXPAYLOADSIZE*25)
#define DATA true
#define ACK false
#define CONREQMSG "REQUEST"
#define CONACCMSG "ACCEPT"
#define FINMSG "FINISH"
#define FINACK "FINISHACK"
#define CONMSGLEN 200
#define SRC true
#define DEST false
	
using namespace std;

// init() starts all the threads, sets the destination as the other host

// finish() cancels the threads, closes the connection, must be called to clear the buffers in case the other end closes first

// appsend() puts the data onto the send buffer

// apprecv() pulls the data from the recv buffer

// senddata() sends data from send buffer, and congestion control

// recvdata() sends ack after putting recieved data on recv buffer

// mainrecv() recvs the packet and puts it in the correct queue ack or data

// see if needed to maintain 2 condition var for each buffer

// can make a class with functions init() appsend() apprecv() finish() in it

// accept() for accepting connection

class ooocomparision
{
	public:

	bool operator()(const pair<unsigned int,unsigned int> & a, const pair<unsigned int,unsigned int> & b) const
	{
		if(a.first == b.first)
			return a.second<b.second;
		else
			return a.first>b.first;
	}

};

queue<pair<char *, int> > aq,dq;
pthread_mutex_t maq = PTHREAD_MUTEX_INITIALIZER, mdq = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cmaq = PTHREAD_COND_INITIALIZER, cmdq = PTHREAD_COND_INITIALIZER;

pair<char *, int> sbuf,rbuf;
pthread_mutex_t msbuf = PTHREAD_MUTEX_INITIALIZER, mrbuf = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cmsbuf = PTHREAD_COND_INITIALIZER, cmrbuf = PTHREAD_COND_INITIALIZER;

int sockfd;
struct sockaddr_in pa;
//socklen_t palen;
pthread_t tid[3];
bool conn = false;

enum cccond{ TIMEOUT, TRIPLEDUPACK, GOODNETWORK};

int min(int a, int b)
{
	return a < b? a: b;
}

int max(int a, int b)
{
	return a > b? a: b;
}

bool identify_packet(char* packet)
{
	if(packet == NULL)
	{
		cerr<<"function identify_packet called with NULL value\n";
		//exit(0);
	}
	bool type;
	bcopy(packet, &type, HEAD0SIZE);
	cerr<<"identify_packet: type"<<type<<endl;
	return type;
}

int create_datapacket(char *payload, unsigned int seqno, int n, char* packet)
{
	bool type = DATA;
	bzero(packet, MAXPACKETSIZE);
	bcopy(&type, packet, HEAD0SIZE);
	bcopy(&seqno, packet + HEAD0SIZE, HEAD1SIZE);
	bcopy(&n, packet + HEAD0SIZE + HEAD1SIZE, HEAD2SIZEDATA);
	bcopy(payload, packet + HEADSIZEDATA, n);
	return HEADSIZEDATA + n;
}

int parse_datapacket(char* payload, unsigned int &seqno, int n, char* packet)
{
	int n1;
	bcopy(packet + HEAD0SIZE, &seqno, HEAD1SIZE);
	bcopy(packet + HEAD0SIZE + HEAD1SIZE, &n1, HEAD2SIZEDATA);
	if(n1 + (int)HEADSIZEDATA != n)
	{
		exit(-1);
	}
	bzero(payload, MAXPAYLOADSIZE);
	bcopy(packet + HEADSIZEDATA, payload, n1);
	payload[n1] = '\0';
	return n1;
}

int create_ackpacket(unsigned int ackno, int rwnd, char* packet)
{
	cerr<<"create_ackpacket:"<<endl;
	bool type = ACK;
	bzero(packet, MAXPACKETSIZE);
	cerr<<type<<'\t';
	bcopy(&type, packet, HEAD0SIZE);
	cerr<<ackno<<'\t'<<&ackno<<'\t';
	bcopy(&ackno, packet + HEAD0SIZE, HEAD1SIZE);
	cerr<<rwnd<<endl;
	bcopy(&rwnd, packet + HEAD0SIZE + HEAD1SIZE, HEAD2SIZEACK);
	packet[HEADSIZEACK] = '\0';
	//cerr<<packet+1<<endl;
	cerr<<ackno<<'\t'<<&ackno<<endl;
	return HEADSIZEACK;
}

int parse_ackpacket(unsigned int &ackno, int &rwnd, int n, char* packet)
{
	
	if(n != HEADSIZEACK)
		exit(-1);
	bcopy(packet + HEAD0SIZE, &ackno, HEAD1SIZE);
	bcopy(packet + HEAD0SIZE + HEAD1SIZE, &rwnd, HEAD2SIZEACK);
	return 0;
}

void* mainrecv(void *ptr);
void* senddata(void *ptr);
void* recvdata(void *ptr);


int makeconnection()
{
	int timeouts = 0, n;
	char packet[CONMSGLEN];
	struct sockaddr_in src_addr;
	socklen_t addrlen;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		cerr<<"ERROR : setsockopt failed\n";
		exit(0);
	}
	while(timeouts < 12)
	{
		bzero(packet, CONMSGLEN);
		strcpy(packet, CONREQMSG);
		n = send(sockfd, packet, strlen(packet), 0);
		if (n < 0)
		{
			cerr<<"ERROR writing to socket\n";
			//exit(0);
		}
		cerr<<"makeconnection trying to recieve\n";
		bzero(packet, CONMSGLEN);
		n = recvfrom(sockfd, packet, MAXPACKETSIZE, 0, (struct sockaddr *)&src_addr, &addrlen);
		if(n < 0 && errno == EWOULDBLOCK)
		{
			timeouts++;
			continue;
		}
		if(n < 0)
		{
			cerr<<"ERROR reading from socket with errno = "<<errno<<endl;
			//exit(0);
		}
		if(src_addr.sin_addr.s_addr != pa.sin_addr.s_addr || src_addr.sin_port != pa.sin_port)
		{
			//continue;
		}
		if(!strcmp(packet, CONACCMSG))
		{
			return 0;
		}
	}
	exit(-1);
}

bool isconnalive()
{
	return conn;
}

int init(char *hostname, unsigned short portno)
{
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		cerr<<"ERROR opening socket\n";
		exit(-1);
	}
	
	cerr<<hostname<<endl;
	
	pa.sin_family = AF_INET;
	pa.sin_port = htons(portno);
	inet_aton(hostname,&pa.sin_addr);
	//palen = sizeof(pa);
	
	//set destination as the other host for sockfd using connect
	if(connect(sockfd, (struct sockaddr *) &pa, sizeof(pa)) < 0)
	{ 
		cerr<<"ERROR setting destination\n";
		exit(-1);
	}
	if(makeconnection() < 0)
	{
		close(sockfd);
		exit(-1);
	}
	sbuf.first = (char *)malloc(sizeof(char)*MAXSBUFSIZE);
	if(sbuf.first == NULL)
	{
		cerr<<"ERROR can't allocate space for sender buffer\n";
		exit(-1);
	}
	bzero(sbuf.first, MAXSBUFSIZE);
	sbuf.second = 0;
	rbuf.first = (char *)malloc(sizeof(char)*MAXRBUFSIZE);
	if(rbuf.first == NULL)
	{
		cerr<<"ERROR can't allocate space for reciever buffer\n";
		free(sbuf.first);
		exit(-1);
	}
	bzero(rbuf.first, MAXRBUFSIZE);
	rbuf.second = 0;
	//create the 3 threads
	conn = true;
	pthread_create( &tid[1], NULL, &senddata, NULL);
	pthread_create( &tid[2], NULL, &recvdata, NULL);
	pthread_create( &tid[0], NULL, &mainrecv, NULL);
	return 0;
}

int bindnaccept(unsigned short portno)
{
	char packet[CONMSGLEN];
	socklen_t palen = sizeof(pa);
	int n;
	
	cerr<<portno<<endl;
	
	struct sockaddr_in serveraddr;
	
	bzero((char*)&serveraddr,sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((unsigned short)portno);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		cerr<<"ERROR opening socket\n";
		exit(-1);
	}
	
	if (bind(sockfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr)) < 0) 
	{
		cerr<<"ERROR on binding\n";
		exit(-1);
	}
	
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if(setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		cerr<<"ERROR : setsockopt failed\n";
		exit(-1);
	}
	do
	{
		bzero(packet, CONMSGLEN);
		//cerr<<pa.sin_port<<endl;
		cerr<<"bindnaccept recieving\n";
		cerr<<palen<<endl;
		n = recvfrom(sockfd, packet, CONMSGLEN, 0, (struct sockaddr *)&pa, &palen);
		cerr<<palen<<endl;

		palen = sizeof(pa);
		if(n < 0)
		{
			cerr<<"ERROR reading from socket with errno = "<<errno<<endl;
			exit(-1);
		}
	}while(strcmp(packet, CONREQMSG));
	
	if(connect(sockfd, (struct sockaddr *) &pa, sizeof(pa)) < 0)
	{
		cerr<<"ERROR setting destination\n";
		exit(-1);
	}
	cerr<<"bindnaccept: portno"<<pa.sin_port<<endl;	
	bzero(packet, CONMSGLEN);
	strcpy(packet, CONACCMSG);
	cerr<<"bindnaccept: sending acknowledgement\n";
	n = send(sockfd, packet, strlen(packet), 0);
	if (n < 0)
	{
		cerr<<"ERROR writing to socket\n";
		exit(-1);
	}
	
	sbuf.first = (char *)malloc(sizeof(char)*MAXSBUFSIZE);
	if(sbuf.first == NULL)
	{
		cerr<<"ERROR can't allocate space for sender buffer\n";
		exit(-1);
	}
	bzero(sbuf.first, MAXSBUFSIZE);
	sbuf.second = 0;
	rbuf.first = (char *)malloc(sizeof(char)*MAXRBUFSIZE);
	if(rbuf.first == NULL)
	{
		cerr<<"ERROR can't allocate space for reciever buffer\n";
		exit(-1);
	}
	bzero(rbuf.first, MAXRBUFSIZE);
	rbuf.second = 0;
	//create the 3 threads
	conn = true;
	cerr<<"bindnaccept: creating threads\n";
	pthread_create( &tid[1], NULL, &senddata, NULL);
	pthread_create( &tid[2], NULL, &recvdata, NULL);
	pthread_create( &tid[0], NULL, &mainrecv, NULL);
	return 0;
}

void closeconnection(bool gen)
{
	int timeouts = 0, n;
	char packet[CONMSGLEN];
	struct sockaddr_in src_addr;
	socklen_t addrlen;
	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		cerr<<"ERROR : setsockopt failed\n";
		//exit(0);
	}
	while(timeouts < 12)
	{
		bzero(packet, CONMSGLEN);
		if(gen == SRC)
			strcpy(packet, FINMSG);
		else
			strcpy(packet, FINACK);
		n = send(sockfd, packet, strlen(packet), 0);
		if (n < 0)
		{
			cerr<<"ERROR writing to socket\n";
			//exit(0);
		}
		bzero(packet, CONMSGLEN);
		n = recvfrom(sockfd, packet, MAXPACKETSIZE, 0, (struct sockaddr *)&src_addr, &addrlen);
		if(n < 0 && errno == EWOULDBLOCK)
		{
			timeouts++;
			continue;
		}
		if(n < 0)
		{
			cerr<<"ERROR reading from socket with errno = "<<errno<<endl;
			//exit(0);
		}
		if(src_addr.sin_addr.s_addr != pa.sin_addr.s_addr || src_addr.sin_port != pa.sin_port)
		{
			continue;
		}
		if(!strcmp(packet, FINACK))
		{
			if(gen == SRC)
			{
				bzero(packet, CONMSGLEN);
				strcpy(packet, FINACK);
				n = send(sockfd, packet, strlen(packet), 0);
				if (n < 0)
				{
					cerr<<"ERROR writing to socket\n";
					//exit(0);
				}
			}
			return;
		}
	}
}

void finish()
{
	
	//pthread_cancel(tid[0]);
	pthread_join(tid[0], NULL);
	//pthread_cancel(tid[1]);
	pthread_join(tid[1], NULL);
	//pthread_cancel(tid[2]);
	pthread_join(tid[2], NULL);
	free(sbuf.first);
	free(rbuf.first);
	while(!aq.empty())
	{
		free(aq.front().first);
		aq.pop();
	}
	while(!dq.empty())
	{
		free(dq.front().first);
		dq.pop();
	}
	closeconnection(SRC);
	close(sockfd);
	conn = false;
}

void* mainrecv(void *ptr)
{
	cerr<<"mainrecv started \n";
	char* packet;
	int n;
	bool type;
	struct sockaddr_in src_addr;
	socklen_t addrlen = sizeof(src_addr);
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		cerr<<"ERROR : setsockopt failed\n";
		exit(0);
	}
	//put a very high timeout and create finish if it occurs
	//appsend() and apprecv() should check if the mainrecv() thread is present, probably can change some global variable
	while(true)
	{
		do
		{
			packet = (char*) malloc(sizeof(char)*MAXPACKETSIZE);
		}while(packet == NULL);
		bzero(packet, MAXPACKETSIZE);
		cerr<<"mainrecv :: recieving\n";
		n = recvfrom(sockfd, packet, MAXPACKETSIZE, 0, (struct sockaddr *) &src_addr, &addrlen);
		cerr<<"mainrecv :: outofrevcieving"<<endl;
		if(n == HEADSIZEACK)
		{
			cerr<<"mainrecv:: recieved an acknowledgement"<<endl;
			bool type = identify_packet(packet);
			int rwnd;
			unsigned int ackno;
			cerr<<"mainrecv::"<<n<<endl;
			parse_ackpacket(ackno,rwnd,n,packet);
			cerr<<"mainrecv::"<<type<<" "<<ackno<<" "<<rwnd<<" "<<packet<<endl;
		}
		else{
			cerr<<"mainrecv:: recieved a data"<<endl;
			char data[1024];
			bool type = identify_packet(packet);
			int rwnd;
			unsigned int ackno;
			cerr<<"mainrecv::"<<n<<endl;
			parse_datapacket(data,ackno,n,packet);
			cerr<<"mainrecv::"<<type<<" "<<ackno<<" "<<data<<" "<<packet<<endl;
		}
		addrlen = sizeof(src_addr);
		if(n < 0 && errno == EWOULDBLOCK)
		{
			//close connection since timeout
			free(packet);
			cerr<<"mainrecv error1 EWOULDBLOCK\n";
			continue;
			// pthread_cancel(tid[1]);
			// pthread_cancel(tid[2]);
			// conn = false;
			// pthread_exit(NULL);
		}
		if(n < 0)
		{
			cerr<<"ERROR reading from socket\n";
			//exit(0);
		}
		if(src_addr.sin_addr.s_addr != pa.sin_addr.s_addr || src_addr.sin_port != pa.sin_port)
		{
			free(packet);
			cerr<<"mainrecv: befcontinue "<<src_addr.sin_addr.s_addr<<" "<<pa.sin_addr.s_addr<<" "<<(src_addr.sin_port != pa.sin_port)<<endl;
			continue;
		}
		if(!strcmp(packet, FINMSG))
		{
			free(packet);
			cerr<<"mainrecv error2 EWOULDBLOCK\n";

			continue;
			// closeconnection(DEST);
			// // pthread_cancel(tid[1]);
			// pthread_cancel(tid[2]);
			// conn = false;
			// pthread_exit(NULL);
		}
		// cerr<<"mainrecv: type:"<<type<<endl;
		type = identify_packet(packet);
		if(type == ACK)
		{
			//acquirelock
			//put packet on aq
			//signal
			//releaselock
			cerr<<"mainrecv: locking maq\n";
			pthread_mutex_lock(&maq);
			if(aq.empty()){
				pthread_cond_signal(&cmaq);
				cerr<<"mainrecv: signalling cmaq\n";
			}
			cerr<<"mainrecv: unlocking maq\n";
			aq.push(make_pair(packet,n));
			pthread_mutex_unlock(&maq);
		}
		else if(type == DATA)
		{
			//acquirelock
			//put packet on dq
			//signal
			//release lock
			pthread_mutex_lock(&mdq);
			if(dq.empty())
				pthread_cond_signal(&cmdq);
			dq.push(make_pair(packet,n));
			pthread_mutex_unlock(&mdq);
		}
	}
}

void* recvdata(void *ptr)
{
	//acquire lock
	//check cond
	//cond_wait(), can be timed wait() to send ack regularly
	//if nothing recieved go to cond_wait()
	//release lock
	//check what is recved
	//if any new data put in buffer//another lock handling//, handle out of order
	//send ack
	cerr<<"recvdata started\n";
	unsigned int ackno=0,oldackno, seqno, start;
	int n, n1, n2, rwnd;
	pair< char *, int > dp;
	priority_queue< pair<unsigned int,unsigned int>,vector<pair<unsigned int,unsigned int> >, ooocomparision > q;
	char payload[MAXPAYLOADSIZE],packet[MAXACKSIZE];
	while(true)
	{
		//flag = 0;
		sleep(1);
		cerr<<"recvdata locking mdq\n";
		pthread_mutex_lock(&mdq);
		//pick one packet
		//release lock
		//send ack
		//if no packet then wait
		while(dq.empty())
		{
			cerr<<"recvdata waiting for cmdq to be filled\n";
			pthread_cond_wait(&cmdq, &mdq);
		}		//can be timed wait if want to send regular ack
		cerr<<"recvdata dq is not empty\n";
		dp = dq.front();
		dq.pop();
		cerr<<"recvdata unlocking mdq\n";
		pthread_mutex_unlock(&mdq);
		n = parse_datapacket(payload, seqno, dp.second, dp.first);
		cerr<<"recvdata: n"<<n<<endl;
		free(dp.first);
		
		cerr<<"recvdata locking mrbuf\n";
		pthread_mutex_lock(&mrbuf);
		if(n > 0 && seqno <= ackno && seqno + n > ackno)
		{
			//if rbuf empty send a signal but dont wait if the buf is full
			//correct
			//add to buffer
			//check for ooo
			//accordingly send ack
			if(rbuf.second == 0)
			{
				cerr<<"recvdata signalling cmrbuf\n";
				pthread_cond_signal(&cmrbuf);
			}
			oldackno = ackno;
			n1 = min(seqno + n - ackno, MAXRBUFSIZE - rbuf.second);
			bcopy(payload, rbuf.first + rbuf.second, n1);
			ackno += n1;
			while(!q.empty() && q.top().first <= ackno)
			{
				ackno = max(ackno, q.top().second);
				q.pop();
			}
			rbuf.second += ackno - oldackno;
			cout<<"recvdata 1st cond ackno "<<ackno<<endl;
		}
		else if(n < 0)
		{
			cerr<<"ERROR : Incorrect Packet"<<endl;
			cout<<"recvdata 2nd cond ackno "<<ackno<<endl;
		}
		else if(seqno > ackno)
		{
			//ooo
			//add to buffer if possible accordingly put the seqno in the q
			start = seqno - ackno + rbuf.second;
			n1 = min( n, MAXRBUFSIZE - start);
			bcopy(payload, rbuf.first + start, n1);
			q.push(make_pair(seqno, seqno + n1));
			cout<<"recvdata 3rd cond ackno "<<ackno<<endl;
		}
		//else  n = 0 || seqno + n <= ackno 
		//handle packet
		//check if out of order if space in buff put in buff
		//if correct then check merge with out of order
		//put the data at the right pos
		//free the packet
		//put the data if new in buffer
		//if buffer empty send signal
		//send ack with recver window
		rwnd = MAXRBUFSIZE - rbuf.second;
		cerr<<"recvdata unlocking mrbuf ackno "<<ackno<<"\n";
		pthread_mutex_unlock(&mrbuf);
		cerr<<"recvdata ackno addr "<<ackno<<endl;
		n2 = create_ackpacket(ackno,rwnd,packet);
		cerr<<"recvdata: sending ack with ackno "<<ackno<<"\n";
		cerr<<"recvdata: portno"<<pa.sin_port<<endl;
		cerr<<"recvdata: n2: "<<n2<<endl;
		int n = sendto(sockfd , packet, n2, 0,(struct sockaddr*)&pa,sizeof(pa));
		if (n < 0)
		{
			cerr<<"ERROR writing to socket\n";
			//exit(0);
		}
	}
}

int apprecv(char *data, int n)
{
	//acquire lock
	//cond_wait()
	//check if buffer has something otherwise goto cond_wait()
	//pull data from recv buffer copy it to given pointer
	//release lock
	int n1;
	cerr<<"apprecv locking mrbuf\n";
	pthread_mutex_lock(&mrbuf);
	/*
	if(rbuf.second == MAXRBUFSIZE)
		pthread_cond_signal(&cmrbuf, &mrbuf);
	won't be needed as recv won't be waiting, as soon as it recieves data, it would send the ack
	*/
	while(rbuf.second == 0)
	{
		cerr<<"apprecv waiting on cmrbuf\n";
		pthread_cond_wait(&cmrbuf, &mrbuf);
	}
	n1 = min(n, rbuf.second);
	bcopy(rbuf.first, data, n1);
	//copy the whole right segment because they may contain ooo data
	bcopy(rbuf.first + n1, rbuf.first, MAXRBUFSIZE - n1);
	bzero(rbuf.first + MAXRBUFSIZE - n1, n1);
	rbuf.second -= n1;
	cerr<<"apprecv unlocking mrbuf\n";
	pthread_mutex_unlock(&mrbuf);
	data[n1] = '\0';
	return n1;
}

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

void senddatapackets( int &sent, int limit, unsigned int &seqno)
{
	// sbuf locked already
	int n,n1,n2;
	char payload[MAXPAYLOADSIZE],packet[MAXPACKETSIZE];
	while(sent < limit)
	{
		n1 = min(MAXPAYLOADSIZE, limit - sent);
		bcopy(sbuf.first + sent, payload, n1);
		n2 = create_datapacket(payload,seqno+sent,n1,packet);
		n = send(sockfd , packet, n2, 0);
		if (n < 0)
		{ 
			cerr<<"ERROR writing to socket\n";
			//exit(0);
		}
		cerr<<"Packet sent with sequence number "<<seqno<<".\n";
		sent += n1;
	}
	seqno += sent;
	// sbuf unlocked after this
}				

void* senddata(void *ptr)
{
	//copy congestion control
	//acquire lock
	//copy bufsize
	//release lock
	int wnd,rwnd,cwnd, n1,limit,ret,timeouts,dupacks,ssthres = INITIALSSTHRESHOLD,discard,outstanding,resent;
	wnd = rwnd = cwnd = INITIALWINDOWSIZE;
	unsigned int seqno,ackno,oldackno,dupseqno;
	seqno = ackno = oldackno = 0;
	pair< char *, int > ap;
	struct timespec t;
	while(true)
	{	
		discard = ackno - oldackno;
		outstanding = seqno - ackno;
		//acquire lock
		cerr<<"senddata locking msbuf\n";
		pthread_mutex_lock(&msbuf);
		//if buffer was full signal
		bcopy(sbuf.first + discard, sbuf.first , outstanding);
		sbuf.second-=discard;
		if(outstanding == 0)
		{
			while(sbuf.second == 0)
			{
				cerr<<sbuf.second<<endl;
				cerr<<"senddata waiting on cmsbuf\n";
				pthread_cond_wait(&cmsbuf, &msbuf);
				cerr<<"value of sbuf.second "<<sbuf.second<<endl;
			}
		}
		//change bufsize
		//store bufsize
		wnd = min(rwnd, cwnd);
		limit = min(sbuf.second, wnd);
		senddatapackets(outstanding, limit, seqno);
		cerr<<"senddata unlocking msbuf\n";
		pthread_mutex_unlock(&msbuf);
		oldackno = ackno;
		timeouts = 0;
		dupacks = 0;
		while(ackno <= oldackno)
		{
			ret = 0;
			cerr<<"senddata locking maq\n";
			pthread_mutex_lock(&maq);
			while(ret==0 && aq.empty())
			{
				clock_gettime(CLOCK_REALTIME, &t);
				t.tv_sec+=10;
				cerr<<"senddata: timedwaiting on cmaq\n";
				ret = pthread_cond_timedwait(&cmaq, &maq, &t);
			}
			if(ret == ETIMEDOUT && aq.empty())
			{
				//send all packets
				cerr<<"senddata: recv ack timeout"<<endl;
				cerr<<"senddata unlocking maq\n";
				pthread_mutex_unlock(&maq);
				timeouts++;
				update_window(cwnd,ssthres,TIMEOUT);
				wnd = min(rwnd, cwnd);
				limit = min(outstanding, wnd);
				resent = 0;
				dupseqno = oldackno;
				cerr<<"senddata locking msbuf\n";
				pthread_mutex_lock(&msbuf);
				senddatapackets(resent,limit,dupseqno);
				cerr<<"senddata unlocking msbuf\n";
				pthread_mutex_unlock(&msbuf);
				continue;
			}
			if(ret == 0 && !aq.empty())
			{
				ap = aq.front();
				aq.pop();
				cerr<<"senddata unlocking maq\n";
				pthread_mutex_unlock(&maq);
				n1 = parse_ackpacket(ackno, rwnd, ap.second, ap.first);
				free(ap.first);
				if(n1 < 0)
				{
					cerr<<"ERROR : Incorrect acknowledgement\n";
				}
				else
				{
					cerr<<"Acknowledgement recieved with acknowledgement number "<<ackno<<".\n";
					if(ackno == oldackno)
					{
						dupacks++;
						if(dupacks % 3 == 0)
						{
							update_window(cwnd,ssthres,TRIPLEDUPACK);
							wnd = min(rwnd, cwnd);
							limit = min(wnd, outstanding);
							limit = min(MAXPAYLOADSIZE, limit);
							resent = 0;
							dupseqno = oldackno;
							pthread_mutex_lock(&msbuf);
							senddatapackets(resent, limit, dupseqno);
							pthread_mutex_unlock(&msbuf);
						}
					}
				}
				continue;
			}
			cerr<<"senddata unlocking maq\n";
			pthread_mutex_unlock(&maq);
		}
		if(timeouts == 0 && dupacks == 0)
		{
			update_window(cwnd, ssthres, GOODNETWORK);
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
	int n1, w = 0;
	while(w < n)
	{
		cerr<<"appsend locking msbuf\n";
		pthread_mutex_lock(&msbuf);
		if(sbuf.second == 0)
		{
			cerr<<"appsend signalling cmsbuf\n";
			pthread_cond_signal(&cmsbuf);
		}
		while( sbuf.second == MAXSBUFSIZE )
		{
			cerr<<"appsend waiting on cmsbuf\n";
			pthread_cond_wait(&cmsbuf, &msbuf);
		}
		n1 = min(n - w,MAXSBUFSIZE - sbuf.second);
		bcopy(data + w, sbuf.first + sbuf.second, n1);
		sbuf.second+=n1;
		cerr<<"appsend unlocking msbuf\n";
		pthread_mutex_unlock(&msbuf);
		w += n1;
	}
}
