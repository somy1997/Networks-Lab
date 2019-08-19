#include <iostream>

using namespace std;

enum state {working, failed};

void callforenum(state one)
{
	if(one == working)
		std::cout<<"wow\n";
	else
		cout<<"yo\n";
}

int main() 
{ 
	callforenum(working);
	callforenum(failed);
	//callforenum(0);
	cout<<working<<endl;
	return 0; 
}
