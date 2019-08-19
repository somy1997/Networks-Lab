#include <stdio.h>			//for printf
#include <unistd.h>			//for read,write,close
#include <stdlib.h>			//for exit,atoi
#include <string.h>			//for bzero,strtok,strcpy,strlen
#include <netdb.h>			//netinet/in.h,sys/socket.h,hostent,gethostbyaddress
//#include <sys/types.h> 
//#include <sys/socket.h>
//#include <netinet/in.h>
#include <arpa/inet.h>		//for inet_ntoa,netinet/in.h,inttypes.h
#include <fcntl.h>			//for open
#include <iostream>
#include <openssl/md5.h>	//for md5 checksum functions
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>          //for errno
#include <time.h>

#define HEAD1SIZE sizeof(unsigned int)
#define HEAD2SIZE sizeof(int)
#define HEADSIZE (HEAD1SIZE+HEAD2SIZE)
#define CHUNKSIZE 1024
#define BUFSIZE (HEADSIZE+CHUNKSIZE)
#define TIMEOUT_SEC 1
#define TIMEOUT_USEC 0
#define TIMEOUT_COUNT 10

using namespace std;

/*

refer types.h file @ http://www.doc.ic.ac.uk/~svb/oslab/Minix/usr/include/sys/types.h

struct sockaddr_in
{
	sa_family_t    sin_family; // address family: AF_INET 
	in_port_t      sin_port;   // port in network byte order, uint16_t 
	struct in_addr sin_addr;   // internet address 
};

struct sockaddr_in
{
        short   sin_family;
        unsigned short integer sin_port;
        struct  in_addr sin_addr;
        char    sin_zero[8];
};

Internet address
struct in_addr 
{
	uint32_t       s_addr;     // address in network byte order 
};

typedef uint32_t in_addr_t;

struct in_addr
{
	in_addr_t s_addr;
};

struct in_addr
{
        unsigned long integer s_addr;
};

struct sockaddr
{
    unsigned short integer sa_family;		// address family
    char sa_data[14];						//  up to 14 bytes of direct address
};

struct hostent
{
	char  *h_name;            // official name of host
	char **h_aliases;         // alias list
	int    h_addrtype;        // host address type
	int    h_length;          // length of address
	char **h_addr_list;       // list of addresses
}
#define h_addr h_addr_list[0] // for backward compatibility

int socket(int socket_family, int socket_type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len);
struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type);
char *inet_ntoa(struct in_addr in);
int connect(int socket, const struct sockaddr *address, socklen_t address_len);
struct hostent *gethostbyname(const char *name);
int inet_aton(const char *cp, struct in_addr *inp);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
int getsockopt(int socket, int level, int option_name, void *restrict option_value, socklen_t *restrict option_len);

void bzero(void *s, size_t n);
void bcopy(const void *src, void *dest, size_t n);
//the src and dest can overlap with bcopy
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
//difference between memcpy and memmove is that with memmove the src and dest can overlap but with memcpy they cannot

In UDP, bind is used to bind the server to a definite port.
In UDP, connect is used to set the default destination address after which the packets can be sent directly using send or write.
implement window to recieve out of order packets

*/

int parentfd;

void catchtermination(int signal)
{
	//while(wait(0)!=-1);
	close(parentfd);
	cout<<"\n\nServer Stopped"<<endl;
	exit(0);
}

void catchterminationchild(int signal)
{
	close(parentfd);
	//close(childfd);
	//close(fd);
	exit(0);
}

int encode(char* buf, unsigned int seq, int n)
{
	bcopy(&seq, buf, HEAD1SIZE);
	bcopy(&n, buf + HEAD1SIZE, HEAD2SIZE);
	return n + HEADSIZE;
}

int decode(char* buf, unsigned int &seq, int n)
{
	int n1;
	bcopy(buf, &seq, HEAD1SIZE);
	bcopy(buf + HEAD1SIZE, &n1, HEAD2SIZE);
	if(n1+HEADSIZE == n)
		return n1;
	else
		return -1;
}

int main(int argc, char **argv)
{
	signal(SIGINT,catchtermination);
	int n,n1,timeouts = 0;
	unsigned short portno;
	socklen_t clientlen;				//unsigned integer
	struct sockaddr_in serveraddr;
	struct sockaddr_in clientaddr;
	char buf[BUFSIZE],ack[BUFSIZE];
	char *hostaddrp,*chunk = buf + HEADSIZE;
	unsigned int seqno, ackno = 0;
	float dropprob;
	float randomfloat;
	bool dropped;
	
	if (argc != 3)
	{
		cerr<<"usage: "<<argv[0]<<" <port> <drop probability>\n";
		exit(0);
	}
	
	portno = (unsigned short)atoi(argv[1]);
	dropprob = (float)atof(argv[2]);
	srand(time(0));
	
	parentfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (parentfd < 0)
	{
		cerr<<"ERROR opening socket\n";
		exit(0);
	}
	
	//setsockopt(parentfd,SOL_SOCKET,SO_REUSEADDR,(const void *)&optval,sizeof(optval));
	
	bzero((char*)&serveraddr,sizeof(serveraddr));
	
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((unsigned short)portno);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(parentfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr)) < 0) 
	{
		cerr<<"ERROR on binding\n";
		exit(0);
	}
	
	cout<<"Server File Descriptor value "<<parentfd<<endl;
	printf("Server Running ....\n\n");
	
	/*
	struct timeval timeout;      
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = TIMEOUT_USEC;
	
	if (setsockopt (parentfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		cerr<<"ERROR : setsockopt failed\n";
		exit(0);
	}
	*/
	
	clientlen = sizeof(clientaddr);
	
	while (1)
	{
		ackno = 0;
		bzero(buf,BUFSIZE);
		n = recvfrom(parentfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
		clientlen = sizeof(clientaddr);
		if(n < 0)
		{
			cerr<<"ERROR reading from socket\n";
			exit(0);
		}
		n = decode(buf,seqno,n);
		if(n < 0)
		{
			cerr<<"ERROR in the data\n";
			continue;
		}
		if(seqno != ackno)
		{
			cout<<"ERROR: The packet doesn't have the starting sequence number 0.\n";
			continue;
		}
		
		hostaddrp = inet_ntoa(clientaddr.sin_addr);
		if (hostaddrp == NULL)
		{
			cerr<<"ERROR on inet_ntoa\n";
			exit(0);
		}
		printf("Server established connection with %s\n", hostaddrp);
		
		
		cout<<"Server received "<<chunk<<endl;
		
		char filename[100];
		strcpy(filename,strtok(chunk,"$"));
		cout<<"Name of the file to be recieved from the host : "<<filename<<endl;
		
		char* size = strtok(NULL,"$");
		long long int sizeoffile = atoi(size);
		cout<<"Size of the file to be recieved from the host : "<<sizeoffile<<endl;
		
		size = strtok(NULL,"$");
		long long int numberofchunks = atoi(size);
		cout<<"Number of chunks to be recieved from the host : "<<numberofchunks<<endl;
		
		//cout<<"yo"<<endl<<flush;
		//exit(0);
		//cout<<filename<<endl;
		
		FILE *fout = fopen(filename,"wb");
		if(fout == NULL)
		{
			cerr<<"Error creating file\n";
			exit(0);
		}
		
		//cout<<"yo"<<endl<<flush;
		//exit(0);
		
		bzero(ack, BUFSIZE);
		strcpy(ack + HEADSIZE,"Hello");
		n1 = encode(ack,ackno,strlen(ack + HEADSIZE));
		
		cout<<"Sending Hello to host.\n";
		
		//cout<<"yo\n"<<endl<<flush;
		//exit(0);
		
		dropped = false;
		while(sizeoffile)
		{
			ackno++;
			while(1)
			{
				if(!dropped)
				{
					n = sendto(parentfd, ack, n1, 0, (struct sockaddr *) &clientaddr, clientlen);
					if(n < 0)
					{
						cerr<<"ERROR writing to socket\n";
						exit(0);
					}
					cout<<"Acknowledgement sent with acknowledgement number "<<ackno-1<<".\n";
				}
				bzero(buf,BUFSIZE);
				n = recvfrom(parentfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
				clientlen = sizeof(clientaddr);
				randomfloat = (float)rand()/RAND_MAX;
				if(randomfloat < dropprob)
				{
					decode(buf,seqno,n);
					cout<<"Dropped packet with sequence number "<<seqno<<".\n";
					dropped = true;
					continue;
				}
				dropped = false;
				if(n < 0)
				{
					cerr<<"ERROR reading from socket\n";
					exit(0);
				}
				n = decode(buf,seqno,n);
				if(n < 0)
				{
					cerr<<"ERROR in the data\n";
					continue;
				}
				if(seqno == ackno)
				{
					break;
				}
				if(seqno < ackno)
					cout<<"Duplicate packet recieved with sequence number "<<seqno<<".\n";
				else
					cout<<"Out of order packet recieved with sequence number "<<seqno<<".\n";
			}
			sizeoffile-=n;
			fwrite(chunk,sizeof(char),n,fout);
			bzero(ack,BUFSIZE);
			n1 = encode(ack,ackno,0);
		}
		
		fclose(fout);
		
		cout<<"File recieved successfully from the host."<<endl;
		
		//cerr<<"yo\n";
		unsigned char c[MD5_DIGEST_LENGTH];
		char temphash[3];
		int i;
		//cout<<filename<<endl;
		FILE *inFile = fopen (filename, "r");
		if(inFile == NULL)
		{
			cerr<<"File could not be opened for calculating md5sum\n";
			exit(0);
		}
		
		MD5_CTX mdContext;
		int bytes;
		unsigned char data[1024];
		
		//cerr<<"yo\n";
		MD5_Init (&mdContext);
		while ((bytes = fread (data, 1, 1024, inFile)) != 0)
			MD5_Update (&mdContext, data, bytes);
		MD5_Final (c,&mdContext);
		
		fclose (inFile);
		//cerr<<"yo\n";
		
		bzero(ack + HEADSIZE,BUFSIZE);
		for(i = 0; i < MD5_DIGEST_LENGTH; i++)
		{ 
			sprintf(temphash,"%02x", c[i]);
			strcat(ack + HEADSIZE,temphash);
		}
		
		cout<<"md5sum calculated for "<<filename<<" : "<<ack + HEADSIZE<<endl;
		
		cout<<"Sending md5sum to host\n";
		n1 = encode(ack,ackno,strlen(ack + HEADSIZE));
		n = sendto(parentfd, ack, n1, 0, (struct sockaddr *) &clientaddr, clientlen);
		if (n < 0)
		{
			cerr<<"ERROR writing to socket\n";
			exit(0);
		}
		cout<<"Acknowledgement sent with acknowledgement number "<<ackno<<".\n";
		cout<<"md5sum successfully sent.\n\n";
	}
}
