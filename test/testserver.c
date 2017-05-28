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
#include "../header/JDFSServer.h"
#include "../header/network.h"

int main(int argc, char const *argv[])
{
	char *ip="192.168.137.135";
	int port=8888;
	int server_listen_fd=0;
	threadpool *pool=(threadpool *)malloc(sizeof(threadpool));
	threadpool_create(pool, 6, 20);
	Http_server_body(ip,port,&server_listen_fd,pool);
	destory_threadpool(pool);
	return 0;
}