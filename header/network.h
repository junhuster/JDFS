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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include <unistd.h>

#define upload_one_piece_size   1024*10
typedef struct http_request_buffer
{
    /*
    ** request_kind, 0:query file size 1: upload 2: download 3,4: ack
    ** 5: query node list alive  6: query one certain node alive
    ** 7: ack alive of one certain node
    ** 8: query ip form node serial 9: return to client,not found
    ** 10: stream transform
	** 11: client post clousd store,giving the meta info 12: failed ack
	** 13: query file meta info, read or delete 14: not exsists ack
	** 15: update meta info 16: update failed ack
	** 17: wait meta complete 18: not complete
	** 19: delet meta file
	** 20: delete file
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
