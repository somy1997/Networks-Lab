#include <iostream>
#include <strings.h>

using namespace std;

int main()
{

	int n[10],i;
	int discard = 3;
	int outstanding = 5;
	for(i=0; i<10; i++)
	{
		n[i] = i;
		cout<<n[i]<<endl;
	}
	cout<<endl;
	bcopy(n + discard, n, outstanding*sizeof(int));
	for(i=0; i<10; i++)
	{
		cout<<n[i]<<endl;
	}
}
