#include "threadpool.h"
void *call_back(void *arg){
	long a=(long)arg;
	printf("thread-id: %lu, arg=%ld\n",(unsigned long int)pthread_self(),a);
	sleep(1);
	return NULL;
}
int main(){
	threadpool *pool=(threadpool *)malloc(sizeof(threadpool));
	threadpool_create(pool, 6, 20);

	for(long i=1;i<50;i++){
		threadpool_add_jobs_to_taskqueue(pool, call_back, (void *)i);
	}

	destory_threadpool(pool);
	return 0;
}