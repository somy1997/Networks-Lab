#include <iostream>
#include <malloc.h>

using namespace std;

int main()
{
	int *a = new int;
	cout<<*a<<endl;
	//free(a);
	delete a;
	cout<<*a<<endl;
	int *b = (int *)malloc(sizeof(int));
	cout<<*b<<endl;
	free(b);
	cout<<*b<<endl;
	//free(b);
	//delete a;
	//cout<<*a<<endl;
}