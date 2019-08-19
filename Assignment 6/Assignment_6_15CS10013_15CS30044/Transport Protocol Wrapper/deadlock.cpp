#include <iostream>
#include <pthread.h>

using namespace std;

char buf[1000];
pthread_mutex_t count_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  condition_var   = PTHREAD_COND_INITIALIZER;
/*
	after pthread_cond_signal() the mutex should be released immediately, if again pthread_mutex_lock is called then
	there will be a deadlock
*/

void * a(void * arg)
{

}

int main()
{
	pthread_t tid;
	pthread_create(&tid, NULL, &a, NULL);

}