#include <iostream>

using namespace std;

enum abc{ p, q, r};

int main()
{
	bool a = true;
	cout<<sizeof(bool)<<endl
		<<a<<endl
		<<(a == 1)<<endl
		<<(a == 0)<<endl
		<<(a == -1)<<endl
		<<(a == 2)<<endl
		<<(a == -2)<<endl
		<<sizeof(abc)<<endl
		<<sizeof(p)<<endl;
	if(-1)
	{
		cout<<"Its true"<<endl;
	}
}
