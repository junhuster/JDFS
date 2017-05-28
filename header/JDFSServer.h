/**
    JDFSServer: http server component of JDFS
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

typedef struct callback_arg_query
{
    int socket_fd;
    char file_name[100];
    unsigned char *server_buffer;
    int server_buffer_size;
    
}callback_arg_query;

typedef struct callback_arg_upload
{
    int socket_fd;
    char file_name[100];
    long range_begin;
    long range_end;
    unsigned char *server_buffer;
    int server_buffer_size;


}callback_arg_upload;

typedef struct callback_arg_download
{
    int socket_fd;
    char file_name[100];
    long range_begin;
    long range_end;
    unsigned char *server_buffer;
    int server_buffer_size;
}callback_arg_download;


#define server_listen_queue 20
#define event_for_epoll_wait_num 20

int Http_server_bind_and_listen(char *ip, int port, int *server_listen_fd);
int Http_server_body(char *ip, int port, int *server_listen_fd, threadpool *pool);

void *Http_server_callback_query(void *arg);
void *Http_server_callback_upload(void *arg);
void *Http_server_callback_download(void *arg);
void *Http_server_callback(void *arg);