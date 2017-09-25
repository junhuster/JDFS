/**
    JDFSFileTrans: header files
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

#include "../header/network.h"

#define max_download_thread 8
#define download_one_piece_size 1024*10
#define number_of_part_file 1

typedef struct break_point_of_part
{

	long start_num_of_piece_of_this_part_file;
	long end_num_of_piece_of_this_part_file;
	long size_of_last_incompelet_piece;
	long alread_download_num_of_piece;

}break_point_of_part;

typedef struct break_point
{
	long file_size;
	long num_of_part_file;
	long size_of_one_piece;
	long total_num_of_piece_of_whole_file;
	char server_ip[128+1];
	int server_port;
	//char file_name[]

}break_point;


int Http_connect_to_server( char *ip,  int port, int *socket_fd);
int Http_query_file_size(char *file_name, int socket_fd, long *file_size);

int Http_create_download_file(char *file_name, FILE **fp_download_file, int part_num);
int Http_restore_orignal_file_name(char *download_part_file_name);
int Http_create_breakpoint_file(char *file_name, FILE **fp_breakpoint, long file_size, long num_of_part_file, long size_of_one_piece,
																					   long total_num_of_piece_of_whole_file,
																					   char *server_ip, int server_port);
int Http_create_breakpoint_part_file(char *file_name, FILE **fp_breakpoint_part, int part_num, long start_num_of_piece_of_this_part_file,
																				 long end_num_of_piece_of_this_part_file,
																				 long size_of_last_incompelet_piece,
																				 long alread_download_num_of_piece);
int Update_breakpoint_part_file(FILE *fp_breakpoint_part, int num_of_piece_tobe_added);
int Save_breakpoint_part_file(FILE *fp_breakpoint_part);
int Delete_breakpoint_file(char *file_name, FILE *fp);


int Get_part_file_name(char *file_name, int part_num, char *part_file_name);

int Http_recv_file(int socket_fd, char *file_name, long range_begin, long range_end, unsigned char *buffer, long buffer_size,char *server_ip, int server_port);
int Save_download_part_of_file(FILE *fp, unsigned char *buffer, long buffer_size, long file_offset);
int JDFS_http_download(char *file_name, char *server_ip, int server_port);
int JDFS_http_download_jbp(char *file_name);
int JDFS_http_upload(char *file_name, char *server_ip, int server_port, char nodes_8_bits, int total_num_of_blocks);
