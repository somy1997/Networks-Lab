#include <iostream>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>

using namespace std;

int main()
{
	mode_t mode = S_IRUSR;
	char buf[1024];
	cout<<mode<<endl<<atoi("888")<<endl<<open("gg243376.pdf",O_RDONLY,S_IRUSR)<<endl;
	int fd = open("gg243376.pdf",O_RDONLY,S_IRUSR);
	int ctr = 0;
	while(read(fd,buf,1024))
		ctr++;
	cout<<ctr<<endl;
	close(fd);
}
