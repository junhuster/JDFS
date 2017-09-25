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
#include<sys/types.h>
#include<netinet/tcp.h>
#include<fcntl.h>
#include<errno.h>
#define max_num_of_nodes  8
#define namenode_ip_def "192.168.137.154"
#define namenode_port_def 8888
typedef struct callback_arg_query
{
    int socket_fd;
	int epoll_fd;
    char file_name[100];
    unsigned char *server_buffer;
    int server_buffer_size;

}callback_arg_query;

typedef struct callback_arg_query_nodelist{
  int socket_fd;
  unsigned char *server_buffer;
  int server_buffer_size;
}callback_arg_query_nodelist;

typedef struct callback_arg_query_node{//query node alive or not
  int socket_fd;
  unsigned char *server_buffer;
  int server_buffer_size;
}callback_arg_query_node;

typedef struct callback_arg_query_ip_from_serial{
  int socket_fd;
  int node_serial;
  unsigned char *server_buffer;
  int server_buffer_size;
}callback_arg_query_ip_from_serial;

typedef struct callback_arg_upload
{
    int socket_fd;
    char file_name[100];
    long range_begin;
    long range_end;
    char nodes_8_bits;
    int  piece_num;
	int  total_piece_of_this_block;
	int  total_num_of_blocks;
    int  epoll_fd;
    unsigned char *server_buffer;
	char *tmp_buffer;
    int server_buffer_size;
}callback_arg_upload;

typedef struct callback_arg_download
{
    int socket_fd;
    char file_name[100];
    long range_begin;
    long range_end;
	int epoll_fd;
    unsigned char *server_buffer;
    int server_buffer_size;
}callback_arg_download;

typedef struct callback_arg_cloudstr_meta{
	int socket_fd;
	char file_name[100];
	int  epoll_fd;
	unsigned char *server_buffer;
	int server_buffer_size;
	int content_len;
}callback_arg_cloudstr_meta;

typedef struct callback_arg_query_file_meta_info{
	int socket_fd;
	char file_name[100];
	int epoll_fd;
	int flag;
	unsigned char *server_buffer;
	int server_buffer_size;
}callback_arg_query_file_meta_info;

typedef struct callback_arg_delete_meta_file{

	int socket_fd;
	char file_name[100];
	int epoll_fd;
	unsigned char *server_buffer;
	int server_buffer_size;

}callback_arg_delete_meta_file;

typedef struct callback_arg_delete_file{
	int socket_fd;
	char file_name[100];
	int  total_num_of_blocks;
	unsigned char *server_buffer;
	int server_buffer_size;
}callback_arg_delete_file;

typedef struct callback_arg_update_meta_info{
	int socket_fd;
	char file_name[100];
	int epoll_fd;
	int block_num;
	int node_serial;
	unsigned char *server_buffer;
	int server_buffer_size;
}callback_arg_update_meta_info;

typedef struct callback_arg_wait_meta_complete{
	int socket_fd;
	char file_name[100];
	unsigned char *server_buffer;
	int server_buffer_size;
}callback_arg_wait_meta_complete;

typedef struct res_nodelist{
  char node_name[10];
  int  node_serial;
  char ip_str[100];
  int port;
}res_nodelist;

typedef struct datanodes{
	char ip_str[max_num_of_nodes][100];
	int  port[max_num_of_nodes];
}datanodes;

#define server_listen_queue 20
#define event_for_epoll_wait_num 20
#define max_num_of_nodes 8
int Http_server_bind_and_listen(char *ip, int port, int *server_listen_fd);
int Http_server_body(char *ip, int port, int *server_listen_fd, threadpool *pool);
int DataNode_alive_or_not(char *ip, int port);
int make_socketfd_nonblocking(int socket_fd);
int Wait_for_socket_to_close(int socket_fd, int useconds, int total_useconds);

void *Http_server_callback(void *arg);
void *Http_server_callback_query(void *arg);
void *Http_server_callback_upload(void *arg);
void *Http_server_callback_download(void *arg);

void *Http_server_callback_query_nodelist(void *arg);
void *Http_server_callback_query_node_alive(void *arg);
void *Http_server_callback_query_ip_from_node_name(void *arg);
void *Http_server_callback_query_ip_from_node_serial(void *arg);
int   Http_stream_transform_file(callback_arg_upload *cb_arg_upload,  char *namenode_ip, int namenode_port);

void *Http_server_callback__cloudstr_meta(void *arg);
void *Http_server_callback_update_meta_info(void *arg);
void *Http_server_callback_wait_meta_complete(void *arg);

void *Http_server_callback_query_file_meta_info(void *arg);

int   Fetch_block_serial_from_file_name(char *file_name);
int   wait_block_transform_complete(int *block_meta_info);

void *Http_server_callback_delete_meta_file(void *arg);
void *Http_server_callback_delete_file(void *arg);
