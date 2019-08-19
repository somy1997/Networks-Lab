#include <iostream>
#include <arpa/inet.h>

using namespace std;

int main()
{
	cout<<INADDR_ANY<<endl<<htonl(INADDR_ANY)<<endl;
	unsigned long ipaddr = 1;
	cout<<ipaddr<<endl<<htonl(ipaddr)<<endl<<(ipaddr<<24)<<endl;
	unsigned short portno = 8080;
	cout<<portno<<endl<<htons(portno)<<endl;
	cout<<AF_INET<<endl;
	return 0;
}
