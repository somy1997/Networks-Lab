#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "../lib/tlp.h"
#include <openssl/md5.h>	//for md5 checksum functions

using namespace std;

void catchtermination(int signo)
{
	finish();
	exit(0);
}

int main(int argc, char **argv)
{
	//signal(SIGINT,catchtermination);
	unsigned short portno;
	//struct sockaddr_in clientaddr;
	
	if (argc != 2)
	{
		cerr<<"usage: "<<argv[0]<<" <port>\n";
		//exit(0);
	}
	
	portno = (unsigned short)atoi(argv[1]);
	
	if(bindnaccept(portno) < 0)
	{
		cerr<<"ERROR on binding and accepting\n";
		exit(0);
	}
	
	printf("Server Running ....\n\n");
	
	char chunk[2000];
	apprecv(chunk,199);

	char filename[100];
	strcpy(filename,strtok(chunk,"$"));
	cout<<"Name of the file to be recieved from the host : "<<filename<<endl;
	
	char* size = strtok(NULL,"$");
	long long int sizeoffile = atoi(size);
	cout<<"Size of the file to be recieved from the host : "<<sizeoffile<<endl;
	
	size = strtok(NULL,"$");
	long long int numberofchunks = atoi(size);
	cout<<"Number of chunks to be recieved from the host : "<<numberofchunks<<endl;

	FILE *fout = fopen(filename,"wb");
	if(fout == NULL)
	{
		cerr<<"Error creating file\n";
		exit(0);
	}
	int n;
	while(sizeoffile)
	{
		n = apprecv(chunk, 2000);
		sizeoffile-=n;
		fwrite(chunk,sizeof(char),n,fout);
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
	
	char checksum[2000];
	bzero(checksum,2000);
	// bzero(ack + HEADSIZE,BUFSIZE);
	for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	{ 
		sprintf(temphash,"%02x", c[i]);
		strcat(checksum,temphash);
	}
	
	cout<<"md5sum calculated for "<<filename<<" : "<<checksum<<endl;
	
	cout<<"Sending md5sum to host\n";
	appsend(checksum,strlen(checksum));
	if (n < 0)
	{
		cerr<<"ERROR writing to socket\n";
		exit(0);
	}
		
	finish();
	
}
