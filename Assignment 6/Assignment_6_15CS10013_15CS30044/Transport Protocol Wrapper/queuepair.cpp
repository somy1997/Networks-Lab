#include <iostream>
#include <utility>
#include <queue>

using namespace std;

int main()
{
	pair<char *, int> a;
	char str[] = "Hello World!";
	int c = '\n';
	a = make_pair(str,c);
	cout<<a.first<<(char)a.second;
	queue<pair<char *, int> > q;
	q.push(a);
	char str2[]="Yo!";
	q.push(make_pair(str2,c));
	cout<<q.front().first<<(char)q.front().second;
	q.pop();
	cout<<q.front().first<<(char)q.front().second;
	q.front().second = 100;
	cout<<c<<endl;
	q.pop();
}
