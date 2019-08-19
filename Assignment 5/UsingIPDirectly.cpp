#include <iostream>
#include <signal.h>     // for signal()
#include <sys/types.h>
#include <sys/socket.h> // for bind()
#include <sys/time.h>   // for struct timeval
#include <sys/select.h> // for select()
#include <stdlib.h>     // for exit()
#include <unistd.h>     // for close()
#include <arpa/inet.h>  // for htons(), AF_INET, ...
#include <map>          // for using map stl
#include <string.h>     // for strstr()
#include <vector>		// for using vector stl

#define SERVER_PORT_NO 		8765
#define PEER_SERVER_PORT_NO 8432
#define BUFSIZE        		1024
#define CLOSE_MESSAGE		"$$###"

using namespace std;

/*

	Need to establish the server first, so create the corresponding fd and bind it to a known port.
	Every peer binds to the same port as a server so but as a client can use any port.
	Need to know if client has established connection already with a peer or not.
	Need to ensure that only one connection exist if two peers try to connect simultaneously.
	Here, implemented infinite timeout hence timeout value is NULL.
	Check whether accept puts the peeraddress ip and port in network byte order or host byte order.
	Later add the option to close the connection with a peer.
	Use getpeername() to get the sockaddr using a file descriptor.
	A peer might close a connection and reconnect in which case insert won't work on map.
	writefds is not implemented here. Directly using write function call.
	Ensure that connect is checked parallely using writefds on its socket.
	If peer server closes the connection, this server should also close.
	If user sends empty string to a server implies he/she wants to close the connection or set a special message for closing.
	Send this message when the Peer server is shut down.
	
	struct timeval {
               long    tv_sec;         // seconds
               long    tv_usec;        // microseconds
           };
	
	int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
	void FD_CLR(int fd, fd_set *set);
	int  FD_ISSET(int fd, fd_set *set);
	void FD_SET(int fd, fd_set *set);
	void FD_ZERO(fd_set *set);
	char *inet_ntoa(struct in_addr in);
	int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	
	struct sockaddr_in
	{
		sa_family_t    sin_family; // address family: AF_INET 
		in_port_t      sin_port;   // port in network byte order, uint16_t 
		struct in_addr sin_addr;   // internet address 
	};

*/

struct classcomp
{
	bool operator() (const in_addr& lhs, const in_addr& rhs) const
	{
		return lhs.s_addr<rhs.s_addr;
	}
};

int serverfd, stdinfd = 0, nfds = stdinfd+1;
map<in_addr,int,classcomp> peerfd;
map<in_addr,int>::iterator it;
vector<in_addr> erasepeerfd;

void closeallfds()
{
	char closemessage[21] = CLOSE_MESSAGE;
	for(it = peerfd.begin(); it != peerfd.end(); it++)
	{
		write(it->second,closemessage,strlen(closemessage));
		cout<<"Closing connection with "<<inet_ntoa(it->first)<<endl;
		close(it->second);
	}
	close(serverfd);
}

void catchtermination(int signal)
{
	cout<<endl<<endl;
	closeallfds();
	cout<<"Server Stopped"<<endl<<endl;
	exit(0);
}

int main()
{
	signal(SIGINT,catchtermination);
	unsigned short serverportno = SERVER_PORT_NO;
	unsigned short peerserverportno = PEER_SERVER_PORT_NO;
	struct sockaddr_in serveraddr,newpeeraddr;
	socklen_t newpeeraddrlen = sizeof(newpeeraddr);
	int n,newpeerfd,i;
	char buf[BUFSIZE];
	//map<in_addr,int,classcomp> peerfd;
	//map<in_addr,int>::iterator it;
	serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverfd < 0)
	{
		cerr<<"ERROR opening socket\n";
		exit(0);
	}
	nfds = serverfd+1;
	
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((unsigned short)serverportno);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(serverfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr)) < 0) 
	{
		cerr<<"ERROR on binding\n";
		exit(0);
	}
	
	if (listen(serverfd, 0) < 0)
	{ 
		cerr<<"ERROR on listen\n";
		exit(0);
	}
	
	cout<<"To send a message use the format '<reciever IP> : <message>'\n\n";
	cout<<"To close the application press CTRL-C\n\n";
	
	cout<<"Server Running at port "<<serverportno<<" ....\n\n";
	
	fd_set readfds,writefds,exceptfds;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	
	FD_SET(serverfd,&readfds);
	FD_SET(stdinfd,&readfds);
	
	FD_SET(serverfd,&exceptfds);
	
	while(true)
	{
		int n = select(nfds, &readfds, &writefds, &exceptfds, NULL);
		
		if(n<0)
		{
			cerr<<"ERROR on select\n";
			exit(0);
		}
		
		//check for exception on serverfd
		
		if(FD_ISSET(serverfd,&exceptfds))
		{
			cerr<<"Exception occured on server file descriptor"<<endl;
			closeallfds();
			exit(0);
		}
		
		//if there is a new peer trying to connect to this server
		
		if(FD_ISSET(serverfd,&readfds))
		{
			newpeerfd = accept(serverfd, (struct sockaddr *) &newpeeraddr, &newpeeraddrlen);
			if (newpeerfd < 0)
			{ 
				cerr<<"ERROR on accept\n";
				exit(0);
			}
			cout<<"New chat connection established with "<<inet_ntoa(newpeeraddr.sin_addr)
				<<" at port "<<ntohs(newpeeraddr.sin_port)<<endl;
				
			// If there is a new connection from the same ip address then the old connection would be replaced with the new one
			peerfd[newpeeraddr.sin_addr] = newpeerfd;
			
			nfds = newpeerfd + 1;
			n--;
		}
		
		// if there is an input from stdin
		
		if(FD_ISSET(stdinfd,&readfds))
		{
			cin.getline(buf,BUFSIZE-1);
			//cout<<buf<<endl;
			char *colon = strstr(buf,":");
			char peeraddra[21];
			strncpy(peeraddra,buf,colon-buf-1);
			peeraddra[colon-buf-1]='\0';
			//cout<<peeraddra<<endl;
			//write(1,colon+2,strlen(colon+2));
			in_addr peeraddrn;
			inet_aton(peeraddra,&peeraddrn);
			if(peerfd.count(peeraddrn)) // connection already exists
			{
				write(peerfd[peeraddrn],colon+2,strlen(colon+2));
			}
			else                    // connection doesn't exist
			{
				newpeerfd = socket(AF_INET, SOCK_STREAM, 0);
				if (newpeerfd < 0)
				{
					cerr<<"ERROR opening socket\n";
					exit(0);
				}
				nfds = newpeerfd+1;
				newpeeraddr.sin_family = AF_INET;
				newpeeraddr.sin_port = htons((unsigned short)peerserverportno);
				newpeeraddr.sin_addr = peeraddrn;
				if(connect(newpeerfd,(sockaddr *) &newpeeraddr, newpeeraddrlen)<0)
				{
					cerr<<"ERROR connecting to "<<peeraddra<<endl;
					closeallfds();
					exit(0);
				}
				peerfd[peeraddrn]=newpeerfd;
				write(peerfd[peeraddrn],colon+2,strlen(colon+2));
			}
			n--;
		}
		
		
		for(it = peerfd.begin(); n>0 && it != peerfd.end(); it++)
		{
			if(FD_ISSET(it->second,&readfds))
			{
				n--;
				bzero(buf,BUFSIZE);
				read(it->second,buf,BUFSIZE-1);
				if(!strcmp(buf,CLOSE_MESSAGE))
				{
					cout<<"Closing connection with "<<inet_ntoa(it->first)<<endl;
					close(it->second);
					//delete this from the map but then map would also change, this is not safe
					//hence storing it in a vector of keys to delete after traversing the loop
					erasepeerfd.push_back(it->first);
				}
				else
				{
					cout<<inet_ntoa(it->first)<<" : "<<buf<<endl;
				}
			}
		}
		
		for(i=0; i<erasepeerfd.size(); i++)
		{
			peerfd.erase(erasepeerfd[i]);
		}
		erasepeerfd.clear();
		
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);
	
		FD_SET(serverfd,&readfds);
		FD_SET(stdinfd,&readfds);
	
		FD_SET(serverfd,&exceptfds);
		
		// Following code keeps setting the old connection for the same ip in the readfds
		for(it = peerfd.begin() ; it != peerfd.end(); it++)
		{
			FD_SET(it->second,&readfds);
		}
	}
}
