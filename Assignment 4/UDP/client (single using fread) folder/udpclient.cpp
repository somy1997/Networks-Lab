#include <stdio.h>		//for stderr,fprintf,stderr,printf,sprintf
#include <stdlib.h>		//for atoi,exit
#include <string.h>		//for bzero,bcopy,strcpy,strcat,strlen,strcmp
#include <unistd.h>		//for read,close,write
//#include <sys/types.h>	//for mode_t
//#include <sys/socket.h>
//#include <netinet/in.h>
#include <arpa/inet.h>		//for inet_aton,netinet/in.h,inttypes.h
#include <netdb.h>		//for sockaddr_in,AF_INET,SOCK_STREAM,socket,gethostbyname,hostent,htons,connect
#include <fcntl.h>		//for open,file permissions,opening modes
#include <iostream>
#include <openssl/md5.h>	//for md5 checksum functions
#include <sys/wait.h>
#include <sys/stat.h>        // for stat
#include <errno.h>           //for errno

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

struct stat {
    dev_t     st_dev;     // ID of device containing file
    ino_t     st_ino;     // inode number
    mode_t    st_mode;    // protection
    nlink_t   st_nlink;   // number of hard links
    uid_t     st_uid;     // user ID of owner
    gid_t     st_gid;     // group ID of owner
    dev_t     st_rdev;    // device ID (if special file)
    off_t     st_size;    // total size, in bytes
    blksize_t st_blksize; // blocksize for file system I/O
    blkcnt_t  st_blocks;  // number of 512B blocks allocated
    time_t    st_atime;   // time of last access
    time_t    st_mtime;   // time of last modification
    time_t    st_ctime;   // time of last status change
};

int stat(const char *path, struct stat *buf);

The acknowledgement to 1st packet acts as a hello message.
The acknowledgement to last packet contains checksum.
Multiple clients can be implemented by using recvfrom by comparing the client address and sending the message to the corresponding client handler.
encode and decode need to return the no. of bytes as encode may encode the bytes and the bytes might be having 0 value, strlen won't work.
can use byte sequence numbers
need to  make sure that clientaddr hasn't changed
need to send number of chunks info
need to add number of bytes to the application header
shifting the whole buf to add application header very inefficient
add application header separate from the chunk size

*/

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
	int sockfd,n,n1,timeouts;
	unsigned short portno;
	struct sockaddr_in serveraddr;
	socklen_t serverlen = sizeof(serveraddr);
	char buf[BUFSIZE], ack[BUFSIZE];
	char *hostname,*chunk = buf+HEADSIZE;
	unsigned int seqno = 0, ackno;
	
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

	bzero(buf, BUFSIZE);
	strcpy(chunk,filename);
	strcat(chunk,"$");
	sprintf(str,"%ld",sizeoffile);
	strcat(chunk,str);
	strcat(chunk,"$");
	sprintf(str,"%ld",numberofchunks);
	strcat(chunk,str);
	
	cout<<chunk<<endl;
	
	n = encode(buf, seqno, strlen(chunk));
	
	timeouts = 0;
	while(1)
	{
		n1 = sendto(sockfd , buf, n, 0, (struct sockaddr *)&serveraddr, serverlen);
		if (n1 < 0)
		{
			cerr<<"ERROR writing to socket\n";
			exit(0);
		}
		cout<<"Packet sent with sequence number "<<seqno<<".\n";
		bzero(ack,BUFSIZE);
		n1 = recvfrom(sockfd, ack, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
		serverlen = sizeof(serveraddr);
		if(errno == EWOULDBLOCK)
		{
			timeouts++;
			cout<<"No. of times timeout has occured : "<<timeouts<<endl;
			continue;
		}
		if(n1 < 0)
		{
			cerr<<"ERROR reading from socket\n";
			exit(0);
		}
		n1 = decode(ack,ackno,n1);
		if(n1 < 0)
		{
			cerr<<"ERROR in the acknowledgement\n";
			exit(0);
		}
		if(ackno == seqno)
		{
			break;
		}
	}
	
	
	printf("Server sends acknowledgement: %s\n",ack+HEADSIZE);
	
	unsigned char c[MD5_DIGEST_LENGTH];
	char filehash[MD5_DIGEST_LENGTH*2+1] = "";
	char temphash[3];
	MD5_CTX mdContext;
	MD5_Init (&mdContext);
	
	FILE *fin = fopen(filename,"rb");
	if(fin == NULL)
	{
		cerr<<"ERROR opening file "<<filename<<endl;
		exit(0);
	}
	
	while(!feof(fin))
	{
		seqno++;
		bzero(buf,BUFSIZE);
		n = fread(chunk,sizeof(char),CHUNKSIZE,fin);
		MD5_Update (&mdContext, chunk, n);
		n = encode(buf,seqno,n);
		timeouts = 0;
		while(1)
		{
			usleep(50000);
			n1 = sendto(sockfd , buf, n, 0, (struct sockaddr *)&serveraddr, serverlen);
			if (n1 < 0)
			{ 
				cerr<<"ERROR writing to socket\n";
				exit(0);
			}
			cout<<"Packet sent with sequence number "<<seqno<<".\n";
			bzero(ack,BUFSIZE);
			n1 = recvfrom(sockfd, ack, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
			serverlen = sizeof(serveraddr);
			if(errno == EWOULDBLOCK)
			{
				timeouts++;
				cout<<"No. of times timeout has occured : "<<timeouts<<endl;
				continue;
			}
			if(n1 < 0)
			{
				cerr<<"ERROR reading from socket\n";
				exit(0);
			}
			n1 = decode(ack,ackno,n1);
			if(n1 < 0)
			{
				cerr<<"ERROR in the acknowledgement\n";
				exit(0);
			}
			if(ackno == seqno)
			{
				break;
			}
		}
	}
	MD5_Final (c,&mdContext);
	fclose(fin);

	cout<<"File successfully sent.\n";
	cout<<"Waiting for md5sum.\n";
	
	for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
	{ 
		sprintf(temphash,"%02x", c[i]);
		strcat(filehash,temphash);
	}
	
	/*
	bzero(buf, CHUNKSIZE);
	n = read(sockfd, buf, CHUNKSIZE);
	if (n < 0)
	{
		cerr<<"ERROR reading from socket\n";
		exit(0);
	}
	*/
	
	cout<<"md5sum recieved from server : "<<ack+HEADSIZE<<endl;
	cout<<"md5sum calculated on client : "<<filehash<<endl;
	
	if(!strcmp(ack+HEADSIZE,filehash))
	{
		cout<<"MD5 Matched\n";
	}
	else
	{
		cout<<"MD5 not Matched\n";
	}
	
	close(sockfd);
	return 0;
}
