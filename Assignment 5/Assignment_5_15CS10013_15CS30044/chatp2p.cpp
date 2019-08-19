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
#include <string>		// for using string stl
#include <fstream>		// for ifstream

#define SERVER_PORT_NO 		8100
//#define PEER_SERVER_PORT_NO 8432
#define BUFSIZE        		1024
#define CLOSE_MESSAGE		"$$###"
#define NO_OF_USERS			5

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
	IP addresses and portno handled in network byte order because for ip addr always need to show using inet_ntoa.
	And for ports ntohs can be used.
	
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
	
	Changes:
	fillfriendtoiplist
	stdin reading and sending
	map of peerfd
*/

struct ip_port
{
	in_addr ip;
	unsigned short port;
};

struct classcomp
{
	bool operator() (const in_addr& lhs, const in_addr& rhs) const
	{
		return lhs.s_addr<rhs.s_addr;
	}
};

int serverfd, stdinfd = 0, nfds = stdinfd+1;
char closemessage[21] = CLOSE_MESSAGE;
map<in_addr,int,classcomp> peerfd;
map<in_addr,int>::iterator it;
vector<in_addr> erasepeerfd;
map<string,ip_port> friendtoiplist;
map<in_addr,string,classcomp> iptofriendlist;

void fillfriendiplist()
{
	ifstream fin;
	char line[101];
	string friendname,ip_addr_a,port_no_a;
	ip_port friendaddr;
	fin.open("user_info.txt",ios::in);
	while(fin.getline(line,100))
	{
		friendname = strtok(line," ");
		ip_addr_a  = strtok(NULL," ");
		port_no_a  = strtok(NULL," ");
		//cout<<friendname<<"-"<<ip_addr_a<<":"<<port_no_a<<endl;
		inet_aton(ip_addr_a.c_str(),&friendaddr.ip);
		friendaddr.port = htons((unsigned short)atoi(port_no_a.c_str()));
		friendtoiplist[friendname] = friendaddr;
		iptofriendlist[friendaddr.ip] = friendname;
		//cout<<friendname<<"-"<<friendaddr.ip.s_addr<<":"<<friendaddr.port<<endl;
	}
}

void closeallfds()
{
	for(it = peerfd.begin(); it != peerfd.end(); it++)
	{
		write(it->second,closemessage,strlen(closemessage));
		cout<<"Closing connection with "<<iptofriendlist[it->first]<<" at "<<inet_ntoa(it->first)<<endl;
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
	fillfriendiplist();
	//exit(0);
	unsigned short serverportno = SERVER_PORT_NO;
	//unsigned short peerserverportno = PEER_SERVER_PORT_NO;
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
	
	cout<<"To send a message use the format '<friend name>/<message>'\n\n";
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
		n = select(nfds, &readfds, &writefds, &exceptfds, NULL);
		
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
			n--;
			newpeerfd = accept(serverfd, (struct sockaddr *) &newpeeraddr, &newpeeraddrlen);
			if (newpeerfd < 0)
			{ 
				cerr<<"ERROR on accept\n";
				exit(0);
			}
			nfds = newpeerfd + 1;
			
			if(iptofriendlist.count(newpeeraddr.sin_addr) && peerfd.size()<NO_OF_USERS)
			{
				cout<<"New chat connection established with "<<iptofriendlist[newpeeraddr.sin_addr]<<" at ip "<<inet_ntoa(newpeeraddr.sin_addr)
					<<" at port "<<ntohs(newpeeraddr.sin_port)<<endl;
			
				// If there is a new connection from the same ip address then the old connection would be replaced with the new one
				peerfd[newpeeraddr.sin_addr] = newpeerfd;
			}
			else
			{
				// Here we can read to clear the message sent by this unknown peer but anyways we are closing the connection
				write(newpeerfd,closemessage,strlen(closemessage));
				close(newpeerfd);
			}
		}
		
		// if there is an input from stdin
		
		if(FD_ISSET(stdinfd,&readfds))
		{
			n--;
			bzero(buf,BUFSIZE);
			cin.getline(buf,BUFSIZE-1);
			//cout<<buf<<endl;
			char *sep = strstr(buf,"/");
			char peername[21];
			strncpy(peername,buf,sep-buf);
			peername[sep-buf]='\0';
			string friendname = peername;
			//cout<<peername<<endl;
			//cin>>i;
			//write(1,colon+2,strlen(colon+2));
			ip_port peeraddrn;
			if(friendtoiplist.count(friendname))
			{
				//cout<<"I have come here\n";
				//cin>>i;
				peeraddrn = friendtoiplist[friendname];
				if(peerfd.count(peeraddrn.ip)) // connection already exists
				{
					write(peerfd[peeraddrn.ip],sep+1,strlen(sep+1));
				}
				else                    // connection doesn't exist
				{
					//cout<<"I have come here\n";
					//cin>>i;
					newpeerfd = socket(AF_INET, SOCK_STREAM, 0);
					if (newpeerfd < 0)
					{
						cerr<<"ERROR opening socket\n";
						exit(0);
					}
					nfds = newpeerfd+1;
					newpeeraddr.sin_family = AF_INET;
					newpeeraddr.sin_port = peeraddrn.port;
					newpeeraddr.sin_addr = peeraddrn.ip;
					if(connect(newpeerfd,(sockaddr *) &newpeeraddr, newpeeraddrlen)<0)
					{
						//cout<<"I have come here1\n";
						//cin>>i;
						cerr<<"ERROR connecting to "<<friendname<<endl;
						closeallfds();
						exit(0);
					}
					//cout<<"I have come here2\n";
					//cin>>i;
					
					peerfd[peeraddrn.ip]=newpeerfd;
					write(peerfd[peeraddrn.ip],sep+1,strlen(sep+1));
				}
			}
			else
			{
				cout<<"ERROR : No address corresponding to friend "<<friendname<<endl;
			}
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
					cout<<"Closing connection with "<<iptofriendlist[it->first]<<" at "<<inet_ntoa(it->first)<<endl;
					close(it->second);
					//delete this from the map but then map would also change, this is not safe
					//hence storing it in a vector of keys to delete after traversing the loop
					erasepeerfd.push_back(it->first);
				}
				else
				{
					cout<<iptofriendlist[it->first]<<"\\"<<buf<<endl;
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
