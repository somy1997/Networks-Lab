#include <iostream>

using namespace std;

int fact(int n)
{
	int n1;
	if(n == 0)
		n1 = 0;
	else
		n1 = n*fact(n-1);
	return n1;
}

int main()
{
	int n;
	cout<<n<<endl;
	cout<<"Enter a number n : ";
	cin>>n;
	cout<<"Factorial (n) = "<<fact(n)<<endl;
	return 0;
}
