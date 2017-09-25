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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define  Arg_Err -1
#define  Malloc_Err -2
#define  Thr_Cre_Err -3
#define  Mutex_Init_Err -4
#define  Cond_Init_Err -5
#define  Mutex_Lock_Err -6
#define  Mutex_uLock_Err -7
#define  Job_add_close_Err -8
#define  Cond_Wait_Err -9
#define  Cond_Brd_Err -10

#define server_buffer_size0 1024*1025


typedef struct job
{
    void * (*call_back_func)(void *arg);
    void *arg;
    struct job *next;
}job;

typedef struct task_queue
{
    volatile int is_queue_alive;
    int max_num_of_jobs_of_this_task_queue;
    volatile int current_num_of_jobs_of_this_task_queue;
    job * volatile task_queue_head;
    job * volatile task_queue_tail;

    pthread_cond_t task_queue_empty;
    pthread_cond_t task_queue_not_empty;
    pthread_cond_t task_queue_not_full;

}task_queue;

typedef struct threadpool
{
    int thread_num;
    pthread_t *thread_id_array;
    volatile int is_threadpool_alive;

    pthread_mutex_t mutex_of_threadpool;
    task_queue tq;

}threadpool;

typedef struct callback_arg
{
    int socket_fd;
    int epoll_fd;
	int source;
    unsigned char *server_buffer;
	unsigned char *tmp_buffer;
    int server_buffer_size;
}callback_arg;


int threadpool_create(threadpool *pool, int num_of_thread, int max_num_of_jobs_of_one_task_queue);
int threadpool_add_jobs_to_taskqueue(threadpool *pool, void * (*call_back_func)(void *arg), void *arg);
int threadpool_fetch_jobs_from_taskqueue(threadpool *pool, job **job_fetched);
void *thread_func(void *arg);
int destory_threadpool(threadpool *pool);
