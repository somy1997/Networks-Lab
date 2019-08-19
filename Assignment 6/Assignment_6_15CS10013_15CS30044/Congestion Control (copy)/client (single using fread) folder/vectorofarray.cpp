#include <iostream>
#include <vector>
#include <string.h>

using namespace std;

//using field = char [50];

int main()
{
	/*
	vector<char[100]> b(2);
	vector<char[100]>::iterator it;
	
	it = b.begin();
	strcpy(*it,"Hello");
	it++;
	strcpy(*it,"World");
	
	for(it = b.begin(); it!=b.end(); it++)
	{
		cout<<*it<<endl;
	}
	*/
	
	
	vector<char*> a(2);
	vector<char*>::iterator it;
	
	it = a.begin();
	*it = new char[100];
	strcpy(*it,"Hello");
	it++;
	*it = new char[100];
	strcpy(*it,"World");
	
	a[0] = new char[100];
	strcpy(a[0],"Hello");
	a[1] = new char[100];
	strcpy(a[1],"World");
	
	for(it = a.begin(); it!=a.end(); it++)
	{
		cout<<*it<<endl;
	}
	
}
