#include <iostream>
#include <cstdio>
#include <string.h>

using namespace std;

int main()
{
	FILE *fin,*fout;
	char buf[10];
	int n,n1;
	fin = fopen("temp.txt","r");
	fout = fopen("temp2.txt","w");
	while(!feof(fin))
	{
		//bzero(buf,9);
		//n=fread(buf,sizeof(char),9,fin);
		bzero(buf,10);
		n=fread(buf,sizeof(char),10,fin);
		cout<<buf<<endl;
		n1=fwrite(buf,sizeof(char),n,fout);
		//break;
	}
	cout<<buf<<endl;
	fclose(fin);
	fclose(fout);
	cout<<n<<endl;
	cout<<n1<<endl;
	cout<<(int)buf[n-2]<<endl;
	cout<<(int)buf[n-1]<<endl;
	cout<<(int)buf[n]<<endl;
	cout<<(char)0<<endl;
	cout<<(char)255<<endl;
	cout<<sizeof(off_t)<<endl<<sizeof(long)<<endl<<sizeof(long long int)<<endl;
}
