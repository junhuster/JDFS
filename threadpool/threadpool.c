/**
    threadpool: used for socket server
    Copyright (C) 2017  zhang jun
    contact me: zhangjunhust@hust.edu.cn
            http://www.cnblogs.com/junhuster/
            http://weibo.com/junhuster 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **/

#include "../header/threadpool.h"

int threadpool_create(threadpool *pool,int num_of_thread, int max_num_of_jobs_of_one_task_queue){
    
    if(num_of_thread<1 || max_num_of_jobs_of_one_task_queue <1 ){
        printf("threadpool_create:argument error\n");
        return Arg_Err;
    }

   
    pool->thread_num=num_of_thread;
    pool->thread_id_array=(pthread_t *)malloc(sizeof(pthread_t)*num_of_thread);
    if(NULL==pool->thread_id_array){
        printf("threadpool_create,malloc thread_id_array failed\n");
        return Malloc_Err;
    }

    for(int i=0;i<num_of_thread;i++){

        int ret=pthread_create((pool->thread_id_array)+i, NULL, thread_func, (void *)pool);
        if(ret!=0){
            perror("threadpool_create,pthread_create");
            return Thr_Cre_Err;
        }
         
    }

    pool->is_threadpool_alive=1;
    int ret1=pthread_mutex_init(&(pool->mutex_of_threadpool), NULL);
    if(ret1!=0){
        perror("threadpool_create,pthread_mutex_init");
        return Mutex_Init_Err;
    }

    task_queue *tq=&(pool->tq);
    tq->is_queue_alive=1;
    tq->max_num_of_jobs_of_this_task_queue=max_num_of_jobs_of_one_task_queue;
    tq->current_num_of_jobs_of_this_task_queue=0;
    tq->task_queue_head=NULL;
    tq->task_queue_tail=NULL;

    int ret2=pthread_cond_init(&(tq->task_queue_empty), NULL);
    if(ret2!=0){
        perror("threadpool_create,pthread_cond_init");
        return Cond_Init_Err;
    }
    int ret3=pthread_cond_init(&(tq->task_queue_not_empty), NULL);
    if(ret3!=0){
        perror("threadpool_create,pthread_cond_init");
        return Cond_Init_Err;
    }
    int ret4=pthread_cond_init(&(tq->task_queue_not_full), NULL);
    if(ret4!=0){
        perror("threadpool_create,pthread_cond_init");
        return Cond_Init_Err;
    }

    return 0;
}


int threadpool_add_jobs_to_taskqueue(threadpool *pool, void * (*call_back_func)(void *arg), void *arg){

    if(pool==NULL || call_back_func==NULL || arg==NULL){
        printf("threadpool_add_jobs_to_taskqueue argument error\n");
        return Arg_Err;
    }

    int ret=pthread_mutex_lock(&(pool->mutex_of_threadpool));
    if(ret!=0){
        perror("threadpool_add_jobs_to_taskqueue,pthread_mutex_lock");
        return Mutex_Lock_Err;
    }

    task_queue *tq=&(pool->tq);
    if(!(pool->is_threadpool_alive && tq->is_queue_alive)){
        int ret=pthread_mutex_unlock(&(pool->mutex_of_threadpool));
        if(ret!=0){
            perror("threadpool_add_jobs_to_taskqueue,pthread_mutex_unlock");
            return Mutex_uLock_Err;
        }
        return Job_add_close_Err;// thread pool or task queue not alive
    }

    
    while(tq->current_num_of_jobs_of_this_task_queue==tq->max_num_of_jobs_of_this_task_queue){
        int ret=pthread_cond_wait(&(tq->task_queue_not_full), &(pool->mutex_of_threadpool));
        if(ret!=0){
            perror("threadpool_add_jobs_to_taskqueue,pthread_cond_wait");
            return Cond_Wait_Err;
        }
    }

    job *job_node=(job *)malloc(sizeof(job));
    if(job_node==NULL){
        printf("threadpool_add_jobs_to_taskqueue,malloc job node failed\n");
        int ret=pthread_mutex_unlock(&(pool->mutex_of_threadpool));
        if(ret!=0){
            perror("threadpool_add_jobs_to_taskqueue,pthread_mutex_unlock");
            return Mutex_uLock_Err;
        }

        return Malloc_Err;//malloc job failed
    }

    job_node->call_back_func=call_back_func;
    job_node->arg=arg;
    job_node->next=NULL;

    if(tq->task_queue_head==NULL){
        tq->task_queue_head=job_node;
        tq->task_queue_tail=job_node;
    }else{
        tq->task_queue_tail->next=job_node;
        tq->task_queue_tail=job_node;
    }

    tq->current_num_of_jobs_of_this_task_queue=tq->current_num_of_jobs_of_this_task_queue+1;
    if(tq->current_num_of_jobs_of_this_task_queue==1){
        int ret=pthread_cond_broadcast(&(tq->task_queue_not_empty));      
        if(ret!=0){
            perror("threadpool_add_jobs_to_taskqueue,pthread_cond_broadcast failed\n");
            return Cond_Brd_Err;
        }
    }

    int ret1=pthread_mutex_unlock(&(pool->mutex_of_threadpool));
    if(ret1!=0){
        perror("threadpool_add_jobs_to_taskqueue,pthread_mutex_unlock failed\n");
        return Mutex_uLock_Err;
    }

    return 0;
}


int threadpool_fetch_jobs_from_taskqueue(threadpool *pool, job **job_fetched){

    if(pool==NULL){
        printf("threadpool_fetch_jobs_from_taskqueue,argument error\n");
        return Arg_Err;
    }

    
    int ret=pthread_mutex_lock(&(pool->mutex_of_threadpool));
    if(ret!=0){
        perror("threadpool_fetch_jobs_from_taskqueue,pthread_mutex_lock");
        return Mutex_Lock_Err;
    }
   
    task_queue *tq=&(pool->tq);
    while(tq->current_num_of_jobs_of_this_task_queue==0 && tq->is_queue_alive==1){
        int ret=pthread_cond_wait(&(tq->task_queue_not_empty), &(pool->mutex_of_threadpool));
        if(ret!=0){
            perror("threadpool_fetch_jobs_from_taskqueue,pthread_cond_wait");
            return Cond_Wait_Err;
        }
    }

    if(tq->is_queue_alive==0){
        int ret=pthread_mutex_unlock(&(pool->mutex_of_threadpool));
        if(ret!=0){
            perror("threadpool_fetch_jobs_from_taskqueue,pthread_mutex_unlock");
            return Mutex_uLock_Err;
        }
        pthread_exit(NULL);
    }

 
    *job_fetched=tq->task_queue_head;
    tq->current_num_of_jobs_of_this_task_queue=tq->current_num_of_jobs_of_this_task_queue-1;
    if(tq->task_queue_head==tq->task_queue_tail){
        tq->task_queue_head=NULL;
        tq->task_queue_tail=NULL;
    }else{
        tq->task_queue_head=tq->task_queue_head->next;        
    }

    (*job_fetched)->next=NULL;

    if(tq->current_num_of_jobs_of_this_task_queue==0){

        int ret=pthread_cond_broadcast(&(tq->task_queue_empty));
        if(ret!=0){
            return Cond_Brd_Err;
        }
    }

    if(tq->current_num_of_jobs_of_this_task_queue==tq->max_num_of_jobs_of_this_task_queue-1){
        int ret=pthread_cond_broadcast(&(tq->task_queue_not_full));
        if(ret!=0){
            return Cond_Brd_Err;
        }
    }

    int ret1=pthread_mutex_unlock(&(pool->mutex_of_threadpool));
    if(ret1!=0){
        return Mutex_uLock_Err;
    }

    return 0;
}

void *thread_func(void *arg){

    if(arg==NULL){
        printf("thread_func, argument error\n");
        pthread_exit(NULL);
    }

    threadpool *pool=(threadpool *)arg;
    job *job_fetched=NULL;
 
    unsigned char server_buffer[server_buffer_size0];

    while(1){
        int ret=threadpool_fetch_jobs_from_taskqueue(pool, &job_fetched);
        if(ret!=0){
            printf("thread_func,fetch job failed\n");
        }else{

            callback_arg *cb_arg=(callback_arg *)(job_fetched->arg);
            cb_arg->server_buffer=server_buffer;
            cb_arg->server_buffer_size=server_buffer_size0;
            job_fetched->call_back_func(job_fetched->arg);
            free(job_fetched);
            if(job_fetched->arg){
                free(job_fetched->arg);
            }                       
        }
    }

    return NULL;
}

int destory_threadpool(threadpool *pool){

    if(pool==NULL){
        printf("destory_threadpool,argument error\n");
        return Arg_Err;
    }

    int ret=pthread_mutex_lock(&(pool->mutex_of_threadpool));
    if(ret!=0){
        perror("destory_threadpool,mutex lock");
        return Mutex_Lock_Err;
    }

    pool->is_threadpool_alive=0;

    task_queue *tq=&(pool->tq);
    while(tq->current_num_of_jobs_of_this_task_queue!=0){
        int ret=pthread_cond_wait(&(tq->task_queue_empty), &(pool->mutex_of_threadpool));
        if(ret!=0){
            perror("destory_threadpool, cond wait");
            return Cond_Wait_Err;            
        }
    }

    tq->is_queue_alive=0;

    int ret1=pthread_cond_broadcast(&(tq->task_queue_not_empty));
    if(ret1!=0){
        perror("destory_threadpool, cond broad cast task queue not empty");
        return Cond_Brd_Err;
    }

    int ret2=pthread_mutex_unlock(&(pool->mutex_of_threadpool));
    if(ret2!=0){
        perror("destory_threadpool,mutex unlock");   
        return Mutex_uLock_Err;  
    }

    for(int i=0;i<pool->thread_num;i++){
        pthread_join(pool->thread_id_array[i], NULL);
    }

    ret2=pthread_mutex_destroy(&(pool->mutex_of_threadpool));

    int ret3=pthread_cond_destroy(&(tq->task_queue_empty));
    int ret4=pthread_cond_destroy(&(tq->task_queue_not_empty));
    int ret5=pthread_cond_destroy(&(tq->task_queue_not_full));

    if(pool->thread_id_array){
        free(pool->thread_id_array);
    }

    job *job_node=tq->task_queue_head;
    job *temp=NULL;
    while(job_node!=NULL){
        temp=job_node->next;
        free(job_node);
        job_node=temp;
    }

    if(pool){
        free(pool);
    }

    return 0;
}