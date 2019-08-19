#include <iostream>
#include <queue>
#include <utility>
#include <vector>

using namespace std;

class mycomparision
{
	public:

	bool operator()(const pair<int, int> & a, const pair<int, int> & b) const
	{
		if(a.first == b.first)
			return a.second<b.second;
		else
			return a.first>b.first;
	}

};

int main()
{
	priority_queue< pair< int, int>,vector<pair< int, int> >, mycomparision > q;
	q.push(make_pair(1,2));
	q.push(make_pair(1,3));
	q.push(make_pair(2,2));
	while(!q.empty())
	{
		cout<<q.top().first<<'\t'<<q.top().second<<endl;
		q.pop();
	}
	priority_queue<int> r;
	r.push(1);
	r.push(2);
	r.push(3);
	while(!r.empty())
	{
		cout<<r.top()<<endl;
		r.pop();
	}
}