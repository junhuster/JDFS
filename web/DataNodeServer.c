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
#include <unistd.h>
#define debug 1

datanodes data_nodes;
extern int node_num_self;
int **node_block_meta_info=NULL;
volatile int node_block_meta_info_ok=0;
extern int Http_connect_to_server( char *ip,  int port, int *socket_fd);
int Http_server_bind_and_listen(char *ip, int port, int *server_listen_fd){

	if(ip==NULL || port<0 || server_listen_fd==NULL){
		printf("Http_server_bind_and_listen: argument error\n");
		exit(0);
	}

  	for(int i=0;i<max_num_of_nodes;i++){
		data_nodes.ip_str[i][0]='\0';
		data_nodes.port[i]=-1;
	}


	*server_listen_fd=socket(AF_INET,SOCK_STREAM,0);
	if((*server_listen_fd)<0){
		perror("Http_server_bind_and_listen, socket");
		exit(0);
	}

	struct sockaddr_in server_socket_address;
	server_socket_address.sin_family=AF_INET;
	server_socket_address.sin_port=htons(port);
	int ret=inet_pton(AF_INET,ip,&(server_socket_address.sin_addr.s_addr));
	if(ret!=1){
		printf("Http_server_bind_and_listen: inet_pton failed\n");
		exit(0);
	}

    int ret1=bind(*server_listen_fd,(struct sockaddr *)&server_socket_address,sizeof(server_socket_address));
    if(ret1==-1){
    	perror("Http_server_bind_and_listen, bind");
    	exit(0);
    }

    int ret2=listen(*server_listen_fd,server_listen_queue);
    if(ret2==-1){
    	perror("Http_server_bind_and_listen,listen");
    	exit(0);
    }
    return 0;
}

int Http_server_body(char *ip, int port, int *server_listen_fd, threadpool *pool){

	if(ip==NULL || port<0 || server_listen_fd==NULL){
		printf("Http_server_body: argument error\n");
		exit(0);
	}

	Http_server_bind_and_listen(ip, port, server_listen_fd);

	struct epoll_event event_for_epoll_ctl;
	struct epoll_event event_for_epoll_wait[event_for_epoll_wait_num];

	event_for_epoll_ctl.data.fd=*server_listen_fd;
	event_for_epoll_ctl.events= EPOLLIN;

	int epoll_fd=epoll_create(20);
	if(epoll_fd==-1){
		perror("Http_server_body,epoll_create");
		exit(0);
	}

	int ret=epoll_ctl(epoll_fd,EPOLL_CTL_ADD,*server_listen_fd,&event_for_epoll_ctl);
	if(ret==-1){
		perror("Http_server_body,epoll_ctl");
		exit(0);
	}

	int num_of_events_to_happen=0;
	while(1){

		num_of_events_to_happen=epoll_wait(epoll_fd,event_for_epoll_wait,event_for_epoll_wait_num,-1);
		if(num_of_events_to_happen==-1){
			perror("Http_server_body,epoll_wait");
			exit(0);
		}

		for(int i=0;i<num_of_events_to_happen;i++){
			struct sockaddr_in client_socket;
			int client_socket_len=sizeof(client_socket);
			if(*server_listen_fd==event_for_epoll_wait[i].data.fd){
				int client_socket_fd=accept(*server_listen_fd,(struct sockaddr *)&client_socket,&client_socket_len);
				if(client_socket_fd==-1){
					perror("Http_server_body,accept");
					continue;
				}

        		int ret=make_socketfd_nonblocking(client_socket_fd);
				if(ret!=0){
					printf("Http_server_body,set nonblocking socket_fd failed\n");
				}
				event_for_epoll_ctl.data.fd=client_socket_fd;
				event_for_epoll_ctl.events=EPOLLIN | EPOLLET | EPOLLONESHOT;

				epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_socket_fd,&event_for_epoll_ctl);

			}else if(event_for_epoll_wait[i].events & EPOLLIN){

				int client_socket_fd=event_for_epoll_wait[i].data.fd;
				if(client_socket_fd<0){
					continue;
				}

				int socket_buffer_size=80*1024;
				int ret_set=setsockopt(client_socket_fd,SOL_SOCKET,SO_RCVBUF,( char *)&socket_buffer_size,sizeof(int));
				if(ret_set!=0){
					perror("Http_server_body,setsockopt");
				}
				int ret_set0=setsockopt(client_socket_fd,SOL_SOCKET,SO_SNDBUF,( char *)&socket_buffer_size,sizeof(int));
				if(ret_set0!=0){
					perror("Http_server_body,setsockopt");
				}

				callback_arg *cb_arg=(callback_arg *)malloc(sizeof(callback_arg));
				cb_arg->socket_fd=client_socket_fd;
				cb_arg->epoll_fd=epoll_fd;
				threadpool_add_jobs_to_taskqueue(pool, Http_server_callback, (void *)cb_arg);
			}
		}
	}
	return 0;
}

void *Http_server_callback(void *arg){

	if(arg==NULL){
		printf("Http_server_callback,argument error\n");
		exit(0);
	}

	callback_arg *cb_arg=(callback_arg *)arg;
	int client_socket_fd=cb_arg->socket_fd;
	memset(cb_arg->server_buffer, 0, sizeof(http_request_buffer)+4);

  	int recv_size=0;
	while (1) {
		int ret=recv(client_socket_fd,(cb_arg->server_buffer)+recv_size,sizeof(http_request_buffer)+4-recv_size,0);
		if(ret==0){
			close(client_socket_fd);
			break;
		}
		if(ret<0){
			if(errno==EAGAIN || errno==EINTR){
				continue;
			}
			close(client_socket_fd);
			break;
		}
		recv_size+=ret;
		if(recv_size==(4+sizeof(http_request_buffer))){
			break;
		}
	}

  	if(recv_size!=(4+sizeof(http_request_buffer))){
		close(client_socket_fd);
		return (void *)-1;
	}

	http_request_buffer *hrb=(http_request_buffer *)(cb_arg->server_buffer);
	if(hrb->request_kind!=10){
		hrb->nodes_8_bits=(hrb->nodes_8_bits) & 0x00;
	}
	if(hrb->request_kind==0){

		callback_arg_query cb_arg_query;
		cb_arg_query.socket_fd=client_socket_fd;
		cb_arg_query.server_buffer=cb_arg->server_buffer;
		cb_arg_query.server_buffer_size=cb_arg->server_buffer_size;
		cb_arg_query.epoll_fd=cb_arg->epoll_fd;
		strcpy(cb_arg_query.file_name, hrb->file_name);

		Http_server_callback_query((void *)(&cb_arg_query));

	}else if(hrb->request_kind==1 || hrb->request_kind==10){

		callback_arg_upload cb_arg_upload;
		cb_arg_upload.socket_fd=client_socket_fd;
		cb_arg_upload.server_buffer=cb_arg->server_buffer;
		cb_arg_upload.server_buffer_size=cb_arg->server_buffer_size;
		cb_arg_upload.range_begin=hrb->num1;
		cb_arg_upload.range_end=hrb->num2;
		cb_arg_upload.epoll_fd=cb_arg->epoll_fd;
		cb_arg_upload.nodes_8_bits=hrb->nodes_8_bits;
		cb_arg_upload.tmp_buffer=cb_arg->tmp_buffer;
		cb_arg_upload.piece_num=hrb->piece_serial_num;
		cb_arg_upload.total_piece_of_this_block=hrb->total_piece_of_this_block;
		cb_arg_upload.total_num_of_blocks=hrb->total_num_of_blocks;
		if(cb_arg_upload.range_end-cb_arg_upload.range_begin<=0){
			printf("Http_server_callback, the checked upload range is wrong\n");
			return (void *)1;
		}

		strcpy(cb_arg_upload.file_name, hrb->file_name);

		int block_serial=Fetch_block_serial_from_file_name(hrb->file_name);
		if(cb_arg_upload.range_begin==0 && block_serial==0){
			node_block_meta_info=(int* *)malloc((cb_arg_upload.total_num_of_blocks)*sizeof(int *));
			if(node_block_meta_info==NULL){
				printf("Http_server_callback,malloc failed\n");
				return (void *)-1;
			}
			for(int i=0;i<cb_arg_upload.total_num_of_blocks;i++){
				node_block_meta_info[i]=NULL;
			}
		}

		Http_server_callback_upload((void *)(&cb_arg_upload));

	}else if(hrb->request_kind==2){

		callback_arg_download cb_arg_download;
		cb_arg_download.socket_fd=client_socket_fd;
		cb_arg_download.server_buffer=cb_arg->server_buffer;
		cb_arg_download.server_buffer_size=cb_arg->server_buffer_size;
		cb_arg_download.range_begin=hrb->num1;
		cb_arg_download.range_end=hrb->num2;
		cb_arg_download.epoll_fd=cb_arg->epoll_fd;

		strcpy(cb_arg_download.file_name, hrb->file_name);

		Http_server_callback_download((void *)(&cb_arg_download));

	}else if(hrb->request_kind==6){

		callback_arg_query_node cb_arg_query_node;
		cb_arg_query_node.socket_fd=client_socket_fd;
		cb_arg_query_node.server_buffer=cb_arg->server_buffer;
		cb_arg_query_node.server_buffer_size=cb_arg->server_buffer_size;
		Http_server_callback_query_node_alive((void *)(&cb_arg_query_node));
	}else if(hrb->request_kind==20){
		callback_arg_delete_file cb_arg_delete_file;
		cb_arg_delete_file.socket_fd=client_socket_fd;
		cb_arg_delete_file.server_buffer=cb_arg->server_buffer;
		cb_arg_delete_file.server_buffer_size=cb_arg->server_buffer_size;
		cb_arg_delete_file.total_num_of_blocks=hrb->total_num_of_blocks;
		strcpy(cb_arg_delete_file.file_name, hrb->file_name);
		Http_server_callback_delete_file((void *)(&cb_arg_delete_file));
	}else{
		printf("unknow request_kind: %d\ncontent:\n%s",hrb->request_kind,cb_arg->server_buffer);
		close(client_socket_fd);
	}
}

void *Http_server_callback_query(void *arg){

	callback_arg_query *cb_arg_query=(callback_arg_query *)arg;
	char *filename=cb_arg_query->file_name;
	int client_socket_fd=cb_arg_query->socket_fd;
	unsigned char *server_buffer=cb_arg_query->server_buffer;

	struct epoll_event ev;
	int epoll_fd=cb_arg_query->epoll_fd;
	ev.data.fd=client_socket_fd;
	ev.events=EPOLLIN | EPOLLET | EPOLLONESHOT;
	epoll_ctl(epoll_fd,EPOLL_CTL_MOD,client_socket_fd,&ev);

	char buffer[100];
	strcpy(buffer,"file/");
	strcat(buffer,filename);
	FILE *fp=fopen(buffer,"r");
    if(NULL==fp) {
	    printf("Http_server_callback_query, file:%s not exists, please provide the right name\n",filename);
	    close(client_socket_fd);
        return (void *)1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size=ftell(fp);

    http_request_buffer *hrb=(http_request_buffer *)(server_buffer);
    hrb->num1=file_size;
    memcpy(server_buffer+sizeof(http_request_buffer), "JDFS", 4);
    send(client_socket_fd,server_buffer,sizeof(http_request_buffer)+4,0);
    if(fclose(fp)!=0){
    	perror("Http_server_callback_query, fclose\n");
    	return (void *)2;
    }

    return NULL;
}

void *Http_server_callback_upload(void *arg){

	callback_arg_upload *cb_arg_upload=(callback_arg_upload *)arg;

	int client_socket_fd=cb_arg_upload->socket_fd;
	long range_begin=cb_arg_upload->range_begin;
	long range_end=cb_arg_upload->range_end;
	unsigned char *server_buffer=cb_arg_upload->server_buffer;
	int piece_num=cb_arg_upload->piece_num;
	int total_piece_of_this_block=cb_arg_upload->total_piece_of_this_block;
	int total_num_of_blocks=cb_arg_upload->total_num_of_blocks;
	FILE *fp=NULL;
	char *file_name=cb_arg_upload->file_name;
	char *new_file_name=cb_arg_upload->tmp_buffer;

	strcpy(new_file_name,"file/");
	strcat(new_file_name,file_name);

	int block_serial=-1;
	block_serial=Fetch_block_serial_from_file_name(file_name);
	if(range_begin==0){
		if(0==(access(new_file_name,F_OK))){
			fp=fopen(new_file_name,"r+");
		}else{
			fp=fopen(new_file_name, "w+");
		}
		if(fp==NULL){
			perror("Http_server_callback_upload,fopen");
			return (void *)-1;
		}
		int block_serial_array_len=sizeof(int)+total_piece_of_this_block*sizeof(int);//total piece num--piece detail

		node_block_meta_info[block_serial]=(int *)malloc(block_serial_array_len);
		if(node_block_meta_info[block_serial]==NULL){
			printf("Http_server_callback_upload,malloc node_block_meta_info[block_serial] failed\n");
			return (void *)-1;
		}
		node_block_meta_info[block_serial][0]=total_piece_of_this_block;
		for(int j=1;j<=total_piece_of_this_block;j++){
			node_block_meta_info[block_serial][j]=0;
		}
		node_block_meta_info_ok=1;
	}else{
		while(1){
			if(0==(access(new_file_name,F_OK))){
				break;
			}else{
				usleep(1000);
			}
		}
		fp=fopen(new_file_name,"r+");
	}


	if(fp==NULL){
		perror("Http_server_callback_upload,fopen");
		close(client_socket_fd);
	}else{

		long offset=range_begin;
	    fseek(fp, offset, SEEK_SET);

	    int recv_size=0;
		int hrb_len=sizeof(http_request_buffer)+4;

	    while(1){
	    	int ret=recv(client_socket_fd,server_buffer+hrb_len+recv_size,range_end-range_begin+1-recv_size,0);
			if(ret==0){
				close(client_socket_fd);
				break;
			}
	    	if(ret<0){
				if(errno=EAGAIN || errno==EINTR){
					continue;
				}
				close(client_socket_fd);
	    		perror("Http_server_callback_upload,recv in while");
	    		break;
	    	}

	    	recv_size+=ret;
	    	if(recv_size==(range_end-range_begin+1)){
	    		break;
	    	}

	    }

	    if(recv_size==(range_end-range_begin+1)){

			struct epoll_event ev;
			int epoll_fd=cb_arg_upload->epoll_fd;
			ev.data.fd=client_socket_fd;
			ev.events=EPOLLIN | EPOLLET | EPOLLONESHOT;
			epoll_ctl(epoll_fd,EPOLL_CTL_MOD,client_socket_fd,&ev);

	    	int ret1=fwrite(server_buffer+hrb_len,range_end-range_begin+1, 1, fp);

			memset(server_buffer,0,sizeof(http_request_buffer));
	    	http_request_buffer *hrb=(http_request_buffer *)server_buffer;
	    	if(ret1==1){
				printf("accept %s from client, range(byte): %ld---%ld\n",cb_arg_upload->file_name,range_begin,range_end);
				while (node_block_meta_info_ok==0) {
					usleep(1000);
				}


				node_block_meta_info[block_serial][piece_num+1]=1;
	    		hrb->request_kind=3;
	    		hrb->num1=range_begin;
	    		hrb->num2=range_end;
	    	}else{
	    		hrb->request_kind=4;
	    	}
			fclose(fp);
	    	int ret=send(client_socket_fd,server_buffer,sizeof(http_request_buffer)+4,0);

	    	if(ret!=(sizeof(http_request_buffer)+4)){
	    		perror("Http_server_callback_upload, send ack to client");
	    	}else{

	    	}
			/*if(source==2){
				Wait_for_socket_to_close(client_socket_fd,0,0);
			}*/

        // send to server to confirm one total block has been stored

			hrb->request_kind=10;
			hrb->num1=range_begin;
			hrb->num2=range_end;
			hrb->nodes_8_bits=cb_arg_upload->nodes_8_bits;
			hrb->piece_serial_num=piece_num;
			hrb->total_piece_of_this_block=total_piece_of_this_block;
			hrb->total_num_of_blocks=total_num_of_blocks;
			memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);

			int ret2=Http_stream_transform_file((callback_arg_upload *)cb_arg_upload,namenode_ip_def,namenode_port_def);

			if(piece_num==total_piece_of_this_block-1){
				int ret=wait_block_transform_complete(node_block_meta_info[block_serial]);
				if(ret==0){

					while (1) {
						http_request_buffer *hrb=(http_request_buffer *)server_buffer;
						hrb->request_kind=15;
						hrb->num1=block_serial;
						hrb->num2=node_num_self;
						strcpy(hrb->file_name,file_name);
						int len=strlen(hrb->file_name);
						char *ptr=hrb->file_name;
						for(int j=len;j>2;j--){
							if(ptr[j]=='k' && ptr[j-1]=='l' && ptr[j-2]=='b'){
								ptr[j-3]='\0';
								break;
							}
						}
						memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
						int socket_fd=-1;
						while (1) {
							int ret=Http_connect_to_server(namenode_ip_def,namenode_port_def,&socket_fd);
							if(ret==0){
								break;
							}else{
								usleep(1000);
								printf("Http_server_callback_upload,connect to server failed0, retry\n");
								continue;
							}
						}

						int ret=send(socket_fd,server_buffer,4+sizeof(http_request_buffer),0);
						if(ret!=4+sizeof(http_request_buffer)){
							close(socket_fd);
							continue;
						}else{
							int ret=recv(socket_fd,server_buffer,4+sizeof(http_request_buffer),0);
							if(ret!=4+sizeof(http_request_buffer)){
								close(socket_fd);
								continue;
							}else{
								http_request_buffer *hrb=(http_request_buffer *)server_buffer;
								if(hrb->request_kind==15){
									if(hrb->num1==block_serial && hrb->num2==node_num_self){
										printf("Http_server_callback_upload,update block info success\n");
										close(socket_fd);
										break;
									}else{
										close(socket_fd);
										continue;
									}
								}else{
									printf("Http_server_callback_upload,unknow block update ack info=%d\n",hrb->request_kind);
									close(socket_fd);
									continue;
								}
							}
						}
					}

					free(node_block_meta_info[block_serial]);
					node_block_meta_info[block_serial]=NULL;

				}else{
					printf("wait_block_transform_complete failed\n");
				}
			}

			if(piece_num==total_piece_of_this_block-1 && block_serial==total_num_of_blocks-1){
				while(1){
					int free_flag=1;
					for(int i=0;i<total_num_of_blocks;i++){
						if(node_block_meta_info[i]!=NULL){
							free_flag=0;
							usleep(1000);
							break;
						}
					}
					if(free_flag==1){
						free(node_block_meta_info);
						break;
					}else{
						continue;
					}
				}
			}

	    }else{

	    	memset(server_buffer, 0, sizeof(http_request_buffer)+4);
	    	http_request_buffer *hrb=(http_request_buffer *)server_buffer;
	    	hrb->request_kind=4;
			memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
	    	int ret=send(client_socket_fd,server_buffer,sizeof(http_request_buffer)+4,0);
	    	if(ret!=(sizeof(http_request_buffer)+4)){
	    		perror("Http_server_callback_upload, send ack to client");
	    	}else{

	    	}

			//close(client_socket_fd);
			fclose(fp);
	    }
	}
}

void *Http_server_callback_download(void *arg){

	callback_arg_download *cb_arg_download=(callback_arg_download *)arg;
	char *file_name=cb_arg_download->file_name;
	int  client_socket_fd=cb_arg_download->socket_fd;
	long range_begin=cb_arg_download->range_begin;
	long range_end=cb_arg_download->range_end;
	unsigned char *server_buffer=cb_arg_download->server_buffer;
	char buffer[100];
	strcpy(buffer,"file/");
	strcat(buffer,file_name);
	FILE *fp=fopen(buffer, "r");
	if(fp==NULL){
		perror("Http_server_callback_download,fopen\n");
		close(client_socket_fd);
		return (void *)1;
	}


	http_request_buffer *hrb=(http_request_buffer *)server_buffer;
	hrb->num1=range_begin;
	hrb->num2=range_end;

	fseek(fp, range_begin, SEEK_SET);
    long http_request_buffer_len=sizeof(http_request_buffer);
    memcpy(server_buffer+http_request_buffer_len,"JDFS",4);

    fread(server_buffer+http_request_buffer_len+4, range_end-range_begin+1, 1, fp);

    int ret=send(client_socket_fd,server_buffer,http_request_buffer_len+4+range_end-range_begin+1,0);
    if(ret==-1){
        perror("Http_server_body,send");
        close(client_socket_fd);
    }else{
		struct epoll_event ev;
		int epoll_fd=cb_arg_download->epoll_fd;
		ev.data.fd=client_socket_fd;
		ev.events=EPOLLIN | EPOLLET | EPOLLONESHOT;
		epoll_ctl(epoll_fd,EPOLL_CTL_MOD,client_socket_fd,&ev);
	}

    if(fclose(fp)!=0){
    	perror("Http_server_callback_download,fclose");
    }
}


void *Http_server_callback_query_node_alive(void *arg){

	callback_arg_query_node *cb_arg_query_node=(callback_arg_query_node *)arg;
	int socket_fd=cb_arg_query_node->socket_fd;
	unsigned char *server_buffer=cb_arg_query_node->server_buffer;
	int server_buffer_len=cb_arg_query_node->server_buffer_size;

    http_request_buffer *hrb=(http_request_buffer *)server_buffer;
	hrb->request_kind=7;
	memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
	int ret=send(socket_fd,server_buffer,sizeof(http_request_buffer)+4,0);
	if(ret!=(4+sizeof(http_request_buffer))){
		perror("Http_server_query_node_alive,send");
		close(socket_fd);
	}else{
		Wait_for_socket_to_close(socket_fd,1000000,8000000);
	}
}

int make_socketfd_nonblocking(int socket_fd){

	if(socket_fd<0){
		printf("make_socketfd_nonblocking,argument error\n");
		return -1;
	}

	int flags=0;
	flags=fcntl(socket_fd,F_GETFL,0);
	if(flags==-1){
		perror("make_socketfd_nonblocking,fcntl");
		return -1;
	}
	flags |=O_NONBLOCK;
	int ret=fcntl(socket_fd,F_SETFL,flags);
	if(ret==-1){
		perror("make_socketfd_nonblocking,fcntl");
		return -1;
	}
	return 0;
}

int Http_stream_transform_file(callback_arg_upload *cb_arg_upload,  char *namenode_ip,  int namenode_port){

	if(cb_arg_upload==NULL){
		printf("Http_stream_transform_file,argument error\n");
		return -1;
	}

	char nodes_8_bits=cb_arg_upload->nodes_8_bits;
	if(nodes_8_bits==0){//either do not need or complete stream transform
		return 0;
	}
    int node_serial=-1;
	for(int i=1;i<=8;i++){
		if(nodes_8_bits & (1<<(i-1))){
			node_serial=i;
			nodes_8_bits=nodes_8_bits & (~(1<<(i-1)));
			break;
		}
	}
    if(node_serial<1 || node_serial>8){
		printf("Http_stream_transform_file,node_serial error\n");
		return -1;
	}

    char *ip_str=NULL;
	int   port=-1;
	if((data_nodes.ip_str)[node_serial][0]!='\0'){
			ip_str=(data_nodes.ip_str)[node_serial];
			port=(data_nodes.port)[node_serial];
	}else{
		unsigned char *server_buffer=(unsigned char *)malloc(4+sizeof(http_request_buffer));
		if(server_buffer==NULL){
			printf("Http_stream_transform_file,malloc failed\n");
			return -1;
		}
		int server_buffer_len=cb_arg_upload->server_buffer_size;
		http_request_buffer *hrb=(http_request_buffer *)server_buffer;
		hrb->request_kind=8;
		hrb->num1=node_serial;

		memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);

		int namenode_socket_fd=-1;
		while (1) {
			int ret=Http_connect_to_server(namenode_ip_def,namenode_port_def,&namenode_socket_fd);
			if(ret==0){
				break;
			}else{
				usleep(1000);
				printf("Http_stream_transform_file,connect to server failed1, retry\n");
				continue;
			}
		}


		int ret=send(namenode_socket_fd,server_buffer,4+sizeof(http_request_buffer),0);
		if(ret!=(4+sizeof(http_request_buffer))){
			perror("Http_stream_transform_file,send\n");
			close(namenode_socket_fd);
			return -1;
		}

		//later we should update the while to consider about complicate situation
		int recv_size=0;
		while(1){

			int ret=recv(namenode_socket_fd,server_buffer+recv_size,4+sizeof(http_request_buffer),0);
			if(ret==0){
				close(namenode_socket_fd);
				break;
			}
			if(ret<0){
				if(errno==EAGAIN || errno==EINTR){
					continue;
				}
				perror("Http_stream_transform_file,recv");
				close(namenode_socket_fd);
				break;
			}
			recv_size+=ret;
			if(recv_size==(4+sizeof(http_request_buffer))){
				break;
			}
		}

		if(recv_size!=(4+sizeof(http_request_buffer))){
			printf("Http_stream_transform_file,query ip from node_serial failed\n");
			close(namenode_socket_fd);
			return -1;
		}else{

			http_request_buffer *hrb=(http_request_buffer *)server_buffer;
			strcpy((data_nodes.ip_str)[node_serial],hrb->file_name);
			(data_nodes.port)[node_serial]=8888;
			ip_str=hrb->file_name;
			port=8888;
			close(namenode_socket_fd);

		}
		free(server_buffer);
	}

	unsigned char *ack_buffer=(unsigned char *)malloc(4+sizeof(http_request_buffer));
  	while(1){

		int range_begin=cb_arg_upload->range_begin;
		int range_end=cb_arg_upload->range_end;
		int piece_len=range_end-range_begin+1;
		http_request_buffer *hrb=(http_request_buffer *)(cb_arg_upload->server_buffer);
		hrb->request_kind=10;
		hrb->num1=range_begin;
		hrb->num2=range_end;
		hrb->nodes_8_bits=nodes_8_bits;
		hrb->piece_serial_num=cb_arg_upload->piece_num;
		hrb->total_piece_of_this_block=cb_arg_upload->total_piece_of_this_block;
		hrb->total_num_of_blocks=cb_arg_upload->total_num_of_blocks;
		strcpy(hrb->file_name,cb_arg_upload->file_name);

		int datanode_socket_fd=-1;
		while (1) {
			int ret=Http_connect_to_server(ip_str,port,&datanode_socket_fd);
			if(ret==0){
				break;
			}else{
				usleep(1000);
				printf("Http_stream_transform_file,connect to server failed2, retry\n");
				continue;
			}
		}


		int ret=send(datanode_socket_fd,cb_arg_upload->server_buffer,4+sizeof(http_request_buffer)+piece_len,0);
		if(ret!=(4+sizeof(http_request_buffer)+piece_len)){
			if(ret==-1){
				perror("Http_stream_transform_file,stream send to other datanode");
				printf("datanode_socket_fd=%d\n",datanode_socket_fd );
			}
			close(datanode_socket_fd);
		    continue;

		}else{
			//wait data node complete recv, then close the socket fd;
			memset(ack_buffer,0,4+sizeof(http_request_buffer));
			int ret=recv(datanode_socket_fd,ack_buffer,4+sizeof(http_request_buffer),0);
			if(ret==(4+sizeof(http_request_buffer))){
				http_request_buffer *hrb=(http_request_buffer *)(ack_buffer);
				if(hrb->request_kind==3){
					if(range_begin==hrb->num1 && range_end==hrb->num2){
						printf("stream transform success,range:%ld--%ld\n",hrb->num1,hrb->num2);
						close(datanode_socket_fd);
						break;
					}else{
						printf("stream transform expected ack range<%d,%d> but got<%ld,%ld>\n",range_begin,range_end,hrb->num1,hrb->num2);
						close(datanode_socket_fd);
						continue;
					}
				}else if(hrb->request_kind==4){
					printf("stream transform failed,range:%d-%d\n",range_begin,range_end);
					close(datanode_socket_fd);
					continue;
				}else{
					printf("stream transform failed, unknow ack number %d,expected range<%d,%d>\n",hrb->request_kind,range_begin,range_end);
					close(datanode_socket_fd);
					continue;
				}
			}else{
				if(ret!=0){
					perror("Http_stream_transform_file,recv ack failed\n");
				}
				close(datanode_socket_fd);
				continue;
			}
			return 0;
		}
	}
}


int Wait_for_socket_to_close(int socket_fd, int useconds, int total_useconds){

	if(socket_fd<0 || useconds<0){
		printf("Wait_for_socket_to_close,argument error\n");
		return -1;
	}

	int total_sleep_time=0;
	while(1){
		struct tcp_info info;
		int len=sizeof(info);
		int ret2=getsockopt(socket_fd,IPPROTO_TCP,TCP_INFO,&info,(socklen_t *)&len);
		volatile int tcpi_state=(volatile int)(info.tcpi_state);
		if(ret2!=0){
			//perror("Wait_for_socket_to_close,getsockopt");
			close(socket_fd);
			break;
		}
		if(tcpi_state!=TCP_ESTABLISHED){
			close(socket_fd);
			break;
		}
		if(useconds>0){
			if(total_sleep_time>=total_useconds){
				printf("Wait_for_socket_to_close,timedout close the socket\n");
				close(socket_fd);
				break;
			}else{
				total_sleep_time+=useconds;
				usleep(useconds);
			}
		}

	}
	return 0;
}

int Fetch_block_serial_from_file_name(char *file_name){

	if(file_name==NULL){
		printf("Fetch_block_serial_from_file_name,argument error\n");
		return -1;
	}

	int file_name_len=strlen(file_name);
	int i;
	int found=0;
	for(i=file_name_len;i>2;i--){
		if(file_name[i]=='k' && file_name[i-1]=='l' && file_name[i-2]=='b'){
			found=1;
			break;
		}else{
			continue;
		}
	}

	if(found==0) return 0;

	i+=1;
	int serial_num=0;
	if(i>0){
		for(int j=i;file_name[j]!='\0';j++){
			serial_num=serial_num*10+(file_name[i]-'0');
		}
		return serial_num;
	}else{
		printf("Fetch_block_serial_from_file_name,file_name has no serial num\n");
		return -1;
	}
}

int wait_block_transform_complete(int *block_meta_info){

	if(block_meta_info==NULL){
		printf("wait_block_transform_complete,argument error\n");
		return -1;
	}

	int piece_num=*block_meta_info;
	int *array=block_meta_info+1;

	int completed_flag=1;
	while(1){
		completed_flag=1;
		for(int i=0;i<piece_num;i++){
			if(array[i]==0){
				completed_flag=0;
				break;
			}
		}
		if(completed_flag==0){
			usleep(5000);
			continue;
		}else if(completed_flag==1){
			break;
		}else{
			printf("wait_block_transform_complete,unknow completed_flag\n");
			break;
		}
	}

	if(completed_flag==1){
		return 0;
	}else return -1;
}

void *Http_server_callback_delete_file(void *arg){

	callback_arg_delete_file *cb_arg_delete_file=(callback_arg_delete_file *)arg;
	int socket_fd=cb_arg_delete_file->socket_fd;
	int total_num_of_blocks=cb_arg_delete_file->total_num_of_blocks;
	unsigned char *server_buffer=cb_arg_delete_file->server_buffer;
	int server_buffer_len=cb_arg_delete_file->server_buffer_size;
	char *file_name=cb_arg_delete_file->file_name;

	int delete_flag=0;

	char temp[100];
	strcpy(temp,"file/");
	strcat(temp,file_name);
	if(access(temp,F_OK)==0){
		int ret=remove(temp);
		if(ret==0){
			delete_flag=1;
		}
	}else{
		delete_flag=1;
	}


	if(delete_flag==1){

		http_request_buffer *hrb=(http_request_buffer *)server_buffer;
		hrb->request_kind=20;
		memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
		int ret=send(socket_fd,server_buffer,4+sizeof(http_request_buffer),0);
		if(ret==0){

		}else{
			close(socket_fd);
		}

	}else{
		close(socket_fd);
	}
}
