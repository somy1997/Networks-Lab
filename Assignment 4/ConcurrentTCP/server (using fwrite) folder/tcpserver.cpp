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

#define BUFSIZE 1024

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

*/

int parentfd,childfd,fd;

void catchtermination(int signal)
{
	while(wait(0)!=-1);
	close(parentfd);
	cout<<"\n\nServer Stopped"<<endl;
	exit(0);
}

void catchterminationchild(int signal)
{
	close(parentfd);
	close(childfd);
	close(fd);
	exit(0);
}

int main(int argc, char **argv)
{
	//int parentfd;
	signal(SIGINT,catchtermination);
	int childfd;
	int portno;
	socklen_t clientlen;				//unsigned integer
	struct sockaddr_in serveraddr;
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	char buf[BUFSIZE];
	char *hostaddrp;
	//int optval;
	int n;
	
	
	if (argc != 2)
	{
		cerr<<"usage: "<<argv[0]<<" <port>\n";
		exit(0);
	}
	
	portno = atoi(argv[1]);
	
	parentfd = socket(AF_INET, SOCK_STREAM, 0);
	if (parentfd < 0)
	{ 
		cerr<<"ERROR opening socket\n";
		exit(0);
	}
	
	//optval = 1;
	//setsockopt(parentfd,SOL_SOCKET,SO_REUSEADDR,(const void *)&optval,sizeof(optval));
	
	bzero((char*)&serveraddr,sizeof(serveraddr));
	
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((unsigned short)portno);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//cout<<sizeof(short unsigned)<<endl<<sizeof serveraddr<<endl;
	
	if (bind(parentfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr)) < 0) 
	{
		cerr<<"ERROR on binding\n";
		exit(0);
	}
	
	if (listen(parentfd, 0) < 0)
	{ 
		cerr<<"ERROR on listen\n";
		exit(0);
	}
	cout<<"Server File Descriptor value "<<parentfd<<endl;
	printf("Server Running ....\n\n");
	clientlen = sizeof(clientaddr);
	while (1)
	{
		childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
		if (childfd < 0)
		{ 
			cerr<<"ERROR on accept\n";
			exit(0);
		}
		cout<<"Child File Descriptor value "<<childfd<<endl;
		/*
		hostp = gethostbyaddr((const void *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		if (hostp == NULL)
		{
			cerr<<"ERROR on gethostbyaddr\n";
			exit(0);
		}
		*/
		
		//cout<<hostp->h_name<<endl
		//	<<(char*)*(hostp->h_aliases)<<endl
		//	<<AF_INET<<endl
		//	<<hostp->h_addrtype<<endl
		//	<<hostp->h_length<<endl
		//	<<hostp->h_addr_list<<endl;
		int childpid;
		while((childpid=fork()) == -1);
		
		if(childpid==0)
		{
			signal(SIGINT,catchterminationchild);
			close(parentfd);
			hostaddrp = inet_ntoa(clientaddr.sin_addr);
			if (hostaddrp == NULL)
			{
				cerr<<"ERROR on inet_ntoa\n";
				exit(0);
			}
			printf("Server established connection with %s\n", hostaddrp);
		
			//cout<<"yo"<<endl;
			//exit(0);
		
			bzero(buf, BUFSIZE);
			n = read(childfd, buf, BUFSIZE);
		
			//cout<<"yo"<<endl;
			//exit(0);
		
			if (n < 0)
			{ 
				cerr<<"ERROR reading from socket\n";
				exit(0);
			}
			cout<<"Server received "<<buf<<endl;
		
			//cout<<"yo"<<endl;
			//exit(0);
		
		
			//char recvd[100];
			char filename[100];
			strcpy(filename,strtok(buf," "));
			//strcat(filename,"a");
			cout<<"Name of the file to be recieved from the host : "<<filename<<endl;
			char* size = strtok(NULL," ");
			//cout<<size<<endl;
			long long int sizeoffile = atoi(size);
			cout<<"Size of the file to be recieved from the host : "<<sizeoffile<<endl;
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
		
			bzero(buf, BUFSIZE);
			strcpy(buf,"Hello");
			n = write(childfd, buf, strlen(buf));
			if (n < 0)
			{
				cerr<<"ERROR writing to socket\n";
				exit(0);
			}
			cout<<"Hello sent to host.\n";
			//cout<<"yo\n"<<endl<<flush;
			//exit(0);
		
			while(sizeoffile)
			{
				bzero(buf, BUFSIZE);
				n = read(childfd, buf, BUFSIZE);
				if(n < 0)
				{
					cerr<<"ERROR reading from socket\n";
					exit(0);
				}
				sizeoffile-=n;
				fwrite(buf,sizeof(char),n,fout);
			}
		
			fclose(fout);
		
			cout<<"File recieved successfully from the host."<<endl;
		
			//cerr<<"yo\n";
			unsigned char c[MD5_DIGEST_LENGTH];
			char temphash[3];
			bzero(buf, BUFSIZE);
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
			for(i = 0; i < MD5_DIGEST_LENGTH; i++)
			{ 
				sprintf(temphash,"%02x", c[i]);
				strcat(buf,temphash);
			}
		
			cout<<"md5sum calculated for "<<filename<<" : "<<buf<<endl;
			cout<<"Sending md5sum to host\n";
			n = write(childfd, buf, strlen(buf));
			if (n < 0)
			{
				cerr<<"ERROR writing to socket\n";
				exit(0);
			}
			cout<<"md5sum successfully sent.\n\n";
		
			close(childfd);
			exit(0);
		}
	}
}
