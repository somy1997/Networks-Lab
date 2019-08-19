#include <iostream>
#include <set>
#include <arpa/inet.h>

using namespace std;

int main()
{
	set<unsigned int> temp;

	temp.insert(INADDR_ANY);
	temp.insert(1);
	in_addr moodle;
	inet_aton("10.5.18.110 :        ",&moodle);
	temp.insert(moodle.s_addr);
	for(set<unsigned int>::iterator it = temp.begin(); it!=temp.end();it++)
	{
		cout<<*it<<endl;
	}
	cout<<temp.count(2)<<endl;
	cout<<temp.count(1)<<endl;
	cout<<sizeof(unsigned)<<endl;
	cout<<inet_ntoa(moodle)<<endl;
	return 0;
}
