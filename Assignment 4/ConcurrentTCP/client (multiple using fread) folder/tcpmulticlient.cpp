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
#include <sys/stat.h>		//for stat

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
	
int main(int argc, char **argv)
{
	if (argc != 5)
	{
		 fprintf(stderr,"usage: %s <hostaddr(IPv4)> <port> <filename> <no. of clients>\n", argv[0]);
		 exit(0);
	}
	int nclients = atoi(argv[4]);
	for(int i=0;i<nclients;i++)
	{	
		if(fork()==0)
		{	
			int sockfd,portno,n;
			struct sockaddr_in serveraddr;
			struct hostent *server;
			char *hostname;
			char buf[BUFSIZE];
			int fd;
			//mode_t mode;

			hostname = argv[1];
			portno = atoi(argv[2]);

			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (sockfd < 0)
			{ 
				cerr<<"ERROR opening socket\n";
				exit(0);
			}

			serveraddr.sin_family = AF_INET;
			serveraddr.sin_port = htons(portno);
			inet_aton(hostname,&serveraddr.sin_addr);

			/*
			server = gethostbyname(hostname);
			if (server == NULL)
			{
				fprintf(stderr,"ERROR, no such host as %s\n", hostname);
				exit(0);
			}

			bzero((char *) &serveraddr, sizeof(serveraddr));
			serveraddr.sin_family = AF_INET;
			bcopy((char *)server->h_addr, (char*)&serveraddr.sin_addr.s_addr, server->h_length);
			serveraddr.sin_port = htons(portno);
			*/
			char filename[100];
			char filename2[100];
			sprintf(filename,"%s",argv[3]);
			sprintf(filename2,"%d%s",i,argv[3]);
			long int sizeoffile = 0;
			struct stat filestat;
			if(stat(filename,&filestat)<0)
			{
				cerr<<"ERROR finding stats of file : "<<filename<<endl;
				exit(0);
			}
			sizeoffile = filestat.st_size;
			cout<<sizeoffile<<endl;
			//exit(0);
	
			if (connect(sockfd,(struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
			{ 
				cerr<<"ERROR connecting\n";
				exit(0);
			}
			printf("Sending hello message\n");

			bzero(buf, BUFSIZE);
			strcpy(buf,filename2);
			strcat(buf," ");
			char str[21];
			sprintf(str,"%ld",sizeoffile);
			strcat(buf,str);

			cout<<buf<<endl;
			//exit(0);

			n = write(sockfd, buf, strlen(buf));
			if (n < 0)
			{ 
				cerr<<"ERROR writing to socket\n";
				exit(0);
			}

			bzero(buf, BUFSIZE);
			n = read(sockfd, buf, BUFSIZE);
			if (n < 0)
			{
				cerr<<"ERROR reading from socket\n";
				exit(0);
			}
			if(strcmp(buf,"Hello"))
			{
				cerr<<"ERROR server didn't send acknowledment\n";
				exit(0);
			}
			printf("Server sends acknowledgement: %s\n",buf);
	
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
			bzero(buf, BUFSIZE);
			while(!feof(fin))
			{
				n = fread(buf,sizeof(char),BUFSIZE,fin);
				MD5_Update (&mdContext, buf, n);
				n = write(sockfd, buf, n);
				if (n < 0)
				{ 
					cerr<<"ERROR writing to socket\n";
					exit(0);
				}
				bzero(buf, BUFSIZE);
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
	
			bzero(buf, BUFSIZE);
			n = read(sockfd, buf, BUFSIZE);
			if (n < 0)
			{
				cerr<<"ERROR reading from socket\n";
				exit(0);
			}
	
			cout<<"md5sum recieved from server : "<<buf<<endl;
			cout<<"md5sum calculated on client : "<<filehash<<endl;
	
			if(!strcmp(buf,filehash))
			{
				cout<<"MD5 Matched\n";
			}
			else
			{
				cout<<"MD5 not Matched\n";
			}
			close(sockfd);
			exit(0);
		}
	}
	while(wait(0)!=-1);
}
