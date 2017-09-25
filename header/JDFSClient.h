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
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<unistd.h>
#define max_num_of_nodes  8
#define max_num_of_block  16
#define block_size 1024*1024
#define namenode_ip_def "192.168.137.154"
#define namenode_port_def 8888
typedef struct node_list
{
    int total_num_of_nodes;
    int serial_num_of_each_node[max_num_of_nodes];//在配置文件里应存储每一个结点的串号
    char ip_str_of_each_node[max_num_of_nodes][100];
    int  port_of_each_node[max_num_of_nodes];
}node_list;

typedef struct each_block_info
{
    int total_num_nodes_of_this_block;
    int serial_num_of_each_node[max_num_of_nodes];//serial num start from 0

}each_block_info;

typedef struct JDFS_file_info
{
    int total_block_num_of_this_file;
    each_block_info block_array[max_num_of_block];

}JDFS_file_info;

typedef struct http_request_buffer
{
    /*
    ** request_kind, 0:query file size 1: upload 2: download 3,4: ack
    ** 5: query node list alive  6: query one certain node alive
    ** 7: ack alive of one certain node
    ** 8: query ip form node serial 9: return to client,not found
    ** 10: stream transform
    */
	int request_kind;
    long num1;
    long num2;
    char nodes_8_bits;
    int  piece_serial_num;//start from 0
	int  total_piece_of_this_block;//start from 1
	int total_num_of_blocks;
    char file_name[100];//file name or node name
}http_request_buffer;

typedef struct res_nodelist{
  char node_name[10];
  int  node_serial;
  char ip_str[100];
  int port;
}res_nodelist;

int JDFS_cloud_query_node_list( char *server_ip,  int server_port, node_list *nli);
int JDFS_cloud_store_file(char *file_name, int flag);
int JDFS_cloud_store_one_block(char *file_name, int block_num,char nodes_8_bits, node_list *nli, int total_num_of_blocks);
int Split_file_into_several_parts(char *file_name, int part_size);
int Extract_part_file_and_store(FILE *fp,char *new_file_name,int range_begin,int range_end);
int JDFS_wait_file_transform_completed(char *file_name,  char *ip_str,  int port);
int JDFS_cloud_query_file(char *file_name, JDFS_file_info *jfi,int flag);//flag, 0: query for reading ,1 :query for deleting
int JDFS_cloud_query_ip_from_node_serial(int serial_num, char *ip_str);
int JDFS_cloud_fetch_file(char *file_name);
int JDFS_cloud_merge_file(char *file_name, int num_of_blocks);
int JDFS_cloud_delete_file(char *file_name);
int JDFS_cloud_delete_file_internal(char *file_name, char *ip_str, int port);
int JDFS_cloud_delete_meta_file(char *file_name, char *ip_str, int port);
int JDFS_cloud_display_file_info(char *file_name);

int JDFS_select_optimal_node_for_one_block(each_block_info *ebi);//return the optimal node num
int JDFS_cloud_fetch_one_block(char *file_name, int block_num,int node_num, char *destination_file_path);
