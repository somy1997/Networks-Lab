#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../lib/tlp.h"
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
#include <queue>
#include <utility>
#include <pthread.h>
#include <vector>
#include <time.h>

using namespace std;

int main(int argc, char *argv[])
{
	if (argc != 4)
	{
		cerr<<"usage: "<<argv[0]<<" <hostaddr(IPv4)> <port> <filename>\n";
		exit(0);
	}
	
	unsigned short portno;
	char *hostname;
	
	hostname = argv[1];
	portno = (unsigned short)atoi(argv[2]);
	
	if(init(hostname, portno) < 0)
	{
		cerr<<"ERROR on connecting to server";
		exit(0);
	}
	
	char filename[100];
	sprintf(filename,"%s",argv[3]);
	long int sizeoffile, numberofchunks;
	char str[21];
	
	struct stat filestat;
	if(stat(filename,&filestat)<0)
	{
		cerr<<"ERROR finding stats of file : "<<filename<<endl;
		exit(0);
	}
	sizeoffile = filestat.st_size;
	cout<<sizeoffile<<endl;
	numberofchunks = sizeoffile%MAXPAYLOADSIZE? (sizeoffile/MAXPAYLOADSIZE+1): (sizeoffile/MAXPAYLOADSIZE);
	cout<<numberofchunks<<endl;
	
	printf("Sending file details : ");
	
	char chunk[2000];
	
	bzero(chunk,200);
	strcpy(chunk,filename);
	strcat(chunk,"$");
	sprintf(str,"%ld",sizeoffile);
	strcat(chunk,str);
	strcat(chunk,"$");
	sprintf(str,"%ld",numberofchunks);
	strcat(chunk,str);
	
	cout<<chunk<<endl;
	appsend(chunk, strlen(chunk));

	FILE *fin = fopen(filename,"rb");
	if(fin == NULL)
	{
		cerr<<"ERROR opening file "<<filename<<endl;
		exit(0);
	}

	unsigned char c[MD5_DIGEST_LENGTH];
	char filehash[MD5_DIGEST_LENGTH*2+1] = "";
	char temphash[3];
	MD5_CTX mdContext;
	MD5_Init (&mdContext);
	
	while(!feof(fin))
	{
		int n = fread(chunk, sizeof(char), 200, fin);
		MD5_Update (&mdContext, chunk, n);
		appsend(chunk, strlen(chunk));

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
	
	apprecv(chunk,199);

	cout<<"md5sum calculated on client : "<<filehash<<endl;
	cout<<"md5sum recieved from server : "<<chunk<<endl;
	
	
	finish();
	
	return 0;
}
