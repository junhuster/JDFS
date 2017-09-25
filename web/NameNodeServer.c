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

	}else if(hrb->request_kind==5){

		callback_arg_query_nodelist cb_arg_query_nodelist;
		cb_arg_query_nodelist.socket_fd=client_socket_fd;
		cb_arg_query_nodelist.server_buffer=cb_arg->server_buffer;
		cb_arg_query_nodelist.server_buffer_size=cb_arg->server_buffer_size;
		Http_server_callback_query_nodelist((void *)(&cb_arg_query_nodelist));

	}else if(hrb->request_kind==8){

		callback_arg_query_ip_from_serial cb_arg_query_ip;
		cb_arg_query_ip.socket_fd=client_socket_fd;
		cb_arg_query_ip.server_buffer=cb_arg->server_buffer;
		cb_arg_query_ip.server_buffer_size=cb_arg->server_buffer_size;
		cb_arg_query_ip.node_serial=hrb->num1;
		Http_server_callback_query_ip_from_node_serial((void *)(&cb_arg_query_ip));

	}else if(hrb->request_kind==11){

		callback_arg_cloudstr_meta cb_arg_cloudstr_meta ;
		cb_arg_cloudstr_meta.socket_fd=client_socket_fd;
		cb_arg_cloudstr_meta.server_buffer=cb_arg->server_buffer;
		cb_arg_cloudstr_meta.server_buffer_size=cb_arg->server_buffer_size;
		cb_arg_cloudstr_meta.epoll_fd=cb_arg->epoll_fd;
		cb_arg_cloudstr_meta.content_len=hrb->num1;
		strcpy(cb_arg_cloudstr_meta.file_name,hrb->file_name);
		Http_server_callback__cloudstr_meta((void *)(&cb_arg_cloudstr_meta));

	}else if(hrb->request_kind==13){

		callback_arg_query_file_meta_info cb_arg_query_file_meta_info;
		cb_arg_query_file_meta_info.socket_fd=client_socket_fd;
		cb_arg_query_file_meta_info.epoll_fd=cb_arg->epoll_fd;
		cb_arg_query_file_meta_info.server_buffer=cb_arg->server_buffer;
		cb_arg_query_file_meta_info.server_buffer_size=cb_arg->server_buffer_size;
		cb_arg_query_file_meta_info.flag=hrb->num1;
		strcpy(cb_arg_query_file_meta_info.file_name,hrb->file_name);
		Http_server_callback_query_file_meta_info((void *)(&cb_arg_query_file_meta_info));

	}else if(hrb->request_kind==15){

		callback_arg_update_meta_info cb_arg_update_meta_info;
		cb_arg_update_meta_info.socket_fd=client_socket_fd;
		cb_arg_update_meta_info.epoll_fd=cb_arg->epoll_fd;
		cb_arg_update_meta_info.server_buffer=cb_arg->server_buffer;
		cb_arg_update_meta_info.server_buffer_size=cb_arg->server_buffer_size;
		cb_arg_update_meta_info.block_num=hrb->num1;
		cb_arg_update_meta_info.node_serial=hrb->num2;
		strcpy(cb_arg_update_meta_info.file_name,hrb->file_name);
		Http_server_callback_update_meta_info((void *)(&cb_arg_update_meta_info));

	}else if(hrb->request_kind==17){

		callback_arg_wait_meta_complete cb_arg_wait_meta_complete;
		cb_arg_wait_meta_complete.socket_fd=client_socket_fd;
		cb_arg_wait_meta_complete.server_buffer=cb_arg->server_buffer;
		cb_arg_wait_meta_complete.server_buffer_size=cb_arg->server_buffer_size;
		strcpy(cb_arg_wait_meta_complete.file_name,hrb->file_name);
		Http_server_callback_wait_meta_complete((void *)(&cb_arg_wait_meta_complete));
	}else if(hrb->request_kind==19){

		callback_arg_delete_meta_file cb_arg_delete_meta_file;
		cb_arg_delete_meta_file.socket_fd=client_socket_fd;
		cb_arg_delete_meta_file.server_buffer=cb_arg->server_buffer;
		cb_arg_delete_meta_file.server_buffer_size=cb_arg->server_buffer_size;
		cb_arg_delete_meta_file.epoll_fd=cb_arg->epoll_fd;
		strcpy(cb_arg_delete_meta_file.file_name,hrb->file_name);
		Http_server_callback_delete_meta_file((void *)(&cb_arg_delete_meta_file));

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

void *Http_server_callback_query_nodelist(void *arg){

		callback_arg_query_nodelist *cb_arg_query_nodelist=(callback_arg_query_nodelist *)arg;
		int client_socket_fd=cb_arg_query_nodelist->socket_fd;
		unsigned char *server_buffer=cb_arg_query_nodelist->server_buffer;
		int server_buffer_size=cb_arg_query_nodelist->server_buffer_size;

		int needed_buffer_length=max_num_of_nodes*sizeof(res_nodelist);

		unsigned char *server_buffer0=(unsigned char *)malloc(needed_buffer_length+4+4);
		if(server_buffer0==NULL){
				printf("Http_server_callback_query_nodelist,malloc failed\n");
				close(client_socket_fd);
				return (void *)-1;
		}


		FILE *fp=NULL;
		fp=fopen("config/node.txt","r");
		if(NULL==fp) {
	    printf("Http_server_callback_query_nodelist, file:node.txt not exists\n");
	    close(client_socket_fd);
        return (void *)1;
    }

	int alive_node_num=0;
	while (NULL!=(fgets(server_buffer,100,fp))) {

		char name[8];
		int  serial;
		char serial_str[6];
		char ip_str[100];
		int port;
		char port_str[6];
		char *ptr=server_buffer;
		int i=0;
		while(*ptr!=' '){
			name[i]=*ptr;
			i++;
			ptr++;
		}
      	name[i]='\0';
      	ptr++;
      	i=0;
		while(*ptr!=' '){
			serial_str[i]=*ptr;
			i++;
			ptr++;
		}

		serial_str[i]='\0';
     	serial=0;
      	for(i=0;serial_str[i]!='\0';i++){
			serial=serial*10+(serial_str[i]-'0');
		}

		i=0;
		ptr++;
		while (*ptr!=' ') {
			ip_str[i]=*ptr;
			i++;
			ptr++;
		}
		ip_str[i]='\0';

		ptr++;
		i=0;
		while(*ptr>='0' && *ptr<='9'){
			port_str[i]=*ptr;
			i++;
			ptr++;
		}

		port_str[i]='\0';
      	port=0;
      	for(i=0;port_str[i]!='\0';i++){
			port=port*10+(port_str[i]-'0');
		}

		int ret=DataNode_alive_or_not(ip_str,port);
      	if(ret==1){
			res_nodelist *res=(res_nodelist *)(server_buffer0+4+alive_node_num*sizeof(res_nodelist));
			strcpy(res->node_name,name);
			res->node_serial=serial;
			strcpy(res->ip_str,ip_str);
			res->port=port;
			alive_node_num+=1;
		}else {
			printf("not alive,node name: %s, node serial: %d, ip:%s, port:%d\n",name,serial,ip_str,port);
		}
	}
    memcpy(server_buffer0+4+alive_node_num*sizeof(res_nodelist),"JDFS",4);
	int *ptr=(int *)server_buffer0;
	*ptr=4+alive_node_num*sizeof(res_nodelist)+4;
	int ret1=send(client_socket_fd,server_buffer0,4+alive_node_num*sizeof(res_nodelist)+4,0);
	if(ret1!=(4+alive_node_num*sizeof(res_nodelist)+4)){
		perror("Http_server_callback_query_nodelist,send");
		close(client_socket_fd);
	}else{
		Wait_for_socket_to_close(client_socket_fd,1000000,8000000);
	}
    fclose(fp);
	free(server_buffer0);
}

int DataNode_alive_or_not(char *ip, int port){
	if(ip==NULL || port<0){
		printf("DataNode_alive_or_not: argument error\n");
		return -1;
	}

	int socket_fd=socket(AF_INET,SOCK_STREAM,0);
	if(socket_fd<0){
		perror("DataNode_alive_or_not");
		return -2;
	}

	struct sockaddr_in socket_address;
	socket_address.sin_family=AF_INET;
	int ret=inet_pton(AF_INET,ip,&(socket_address.sin_addr.s_addr));
	if(ret!=1){
		perror("DataNode_alive_or_not,inet_pton");
		return -3;
	}

	socket_address.sin_port=htons(port);

	int ret0=connect(socket_fd,(struct sockaddr *)&socket_address,sizeof(struct sockaddr_in));

	if(ret0==0){
      	unsigned char *buffer=(unsigned char *)malloc(sizeof(http_request_buffer)+4);
		if(buffer==NULL){
			printf("DataNode_alive_or_not,malloc failed\n");
			return -1;
		}
		http_request_buffer *hrb=(http_request_buffer *)buffer;
		hrb->request_kind=6;
		memcpy(buffer+sizeof(http_request_buffer),"JDFS",4);
		int ret1=send(socket_fd,buffer,4+sizeof(http_request_buffer),0);
		if(ret1!=(4+sizeof(http_request_buffer))){
			perror("DataNode_alive_or_not,send");
			free(buffer);
			close(socket_fd);
			return -1;
		}
		int ret2=recv(socket_fd,buffer,4+sizeof(http_request_buffer),0);
		if(ret2!=(4+sizeof(http_request_buffer))){
			perror("DataNode_alive_or_not,recv");
			free(buffer);
			close(socket_fd);
			return -1;
		}
		int *temp=(int *)buffer;
		if(*temp==7){
			free(buffer);
			close(socket_fd);
			return 1;
		}else{
			free(buffer);
			close(socket_fd);
			return 0;
		}
		close(socket_fd);
		return 1;
	}else{
		close(socket_fd);
		return 0;
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

void *Http_server_callback_query_ip_from_node_serial(void *arg){

	callback_arg_query_ip_from_serial *cb_arg_query_ip=(callback_arg_query_ip_from_serial *)arg;
	int client_socket_fd=cb_arg_query_ip->socket_fd;
	unsigned char *server_buffer=cb_arg_query_ip->server_buffer;
	int server_buffer_size=cb_arg_query_ip->server_buffer_size;
	int node_serial=cb_arg_query_ip->node_serial;

	FILE *fp=NULL;
	fp=fopen("config/node.txt","r");
	if(NULL==fp) {
		printf("Http_server_callback_query_nodelist, file:node.txt not exists\n");
		close(client_socket_fd);
		return (void *)1;
	}
  	int found=0;
	while (NULL!=(fgets(server_buffer,100,fp))) {

		char name[8];
		int  serial;
		char serial_str[6];
		char ip_str[100];
		int port;
		char port_str[6];
		char *ptr=server_buffer;
		int i=0;
		while(*ptr!=' '){
			name[i]=*ptr;
			i++;
			ptr++;
		}
		name[i]='\0';
		ptr++;
		i=0;
		while(*ptr!=' '){
			serial_str[i]=*ptr;
			i++;
			ptr++;
		}

		serial_str[i]='\0';
		serial=0;
		for(i=0;serial_str[i]!='\0';i++){
			serial=serial*10+(serial_str[i]-'0');
		}

		i=0;
		ptr++;
		while (*ptr!=' ') {
			ip_str[i]=*ptr;
			i++;
			ptr++;
		}
		ip_str[i]='\0';

		ptr++;
		i=0;
		while(*ptr>='0' && *ptr<='9'){
			port_str[i]=*ptr;
			i++;
			ptr++;
		}

		port_str[i]='\0';
		port=0;
		for(i=0;port_str[i]!='\0';i++){
			port=port*10+(port_str[i]-'0');
		}

		if(serial==node_serial){
			found=1;
			http_request_buffer *hrb=(http_request_buffer *)server_buffer;
			strcpy(hrb->file_name,ip_str);
			memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
			hrb->request_kind=8;
			int ret=send(client_socket_fd,server_buffer,sizeof(http_request_buffer)+4,0);
			if(ret!=(4+sizeof(http_request_buffer))){
				close(client_socket_fd);
				break;
			}else{
				break;
			}
		}
  	}

  	if(found==0){
		http_request_buffer *hrb=(http_request_buffer *)server_buffer;
		hrb->request_kind=9;
		memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
		int ret=send(client_socket_fd,server_buffer,sizeof(http_request_buffer)+4,0);
		if(ret!=(4+sizeof(http_request_buffer))){
			close(client_socket_fd);
			return (void *)-1;
		}
	}
	Wait_for_socket_to_close(client_socket_fd,1000000,8000000);
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


void *Http_server_callback__cloudstr_meta(void *arg){

	if(arg==NULL){
		printf("Http_server_callback__cloudstr_meta,argument error\n");
		return (void *)-1;
	}

	callback_arg_cloudstr_meta *cb_arg_cloudstr_meta=(callback_arg_cloudstr_meta *)arg;

	int client_socket_fd=cb_arg_cloudstr_meta->socket_fd;
	int content_len=cb_arg_cloudstr_meta->content_len;
	int epoll_fd=cb_arg_cloudstr_meta->epoll_fd;
	unsigned char *server_buffer=cb_arg_cloudstr_meta->server_buffer;
	int server_buffer_size=cb_arg_cloudstr_meta->server_buffer_size;
	char *file_name=cb_arg_cloudstr_meta->file_name;

	int recv_size=0;
	while(1){

		int ret=recv(client_socket_fd,server_buffer+recv_size,content_len-recv_size,0);
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
		if(recv_size==content_len){
			break;
		}
	}

	if(content_len!=recv_size){
		printf("Http_server_callback__cloudstr_meta,recv content failed\n");
		close(client_socket_fd);
		return (void *)-1;
	}

    int block_num=*((int *)server_buffer);

	int *array=(int *)(server_buffer+sizeof(int));
	int recv_ok=1;
	for(int i=0;i<block_num;i++){
		for(int j=0;j<max_num_of_nodes;j++){
			int number=*(array+i*max_num_of_nodes+j);
			if(number!=0 && number!=1){
				recv_ok=0;
				break;
			}
		}
	}

	if(recv_ok==0){
		http_request_buffer *hrb=(http_request_buffer *)server_buffer;
		hrb->request_kind=12;
		memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
		int ret=send(client_socket_fd,server_buffer,sizeof(http_request_buffer)+4,0);
		if(ret!=(4+sizeof(http_request_buffer))){
			perror("Http_server_callback__cloudstr_meta,send failed ack to client");
			close(client_socket_fd);
		}
		return (void *)-1;
	}

	int meta_file_buffer_len=100+sizeof(int)+sizeof(int)+block_num*max_num_of_nodes*sizeof(int);//filename-status-blocknum-block detail
	unsigned char *file_buffer=(unsigned char *)malloc(meta_file_buffer_len+100);
	if(file_buffer==NULL){
		printf("Http_server_callback__cloudstr_meta,malloc meta file buffer failed\n");
		close(client_socket_fd);
		return (void *)-1;
	}

	strcpy(file_buffer,file_name);
	char *ptr=file_buffer+100;
	*((int *)ptr)=0;
	ptr+=sizeof(int);
	*((int *)ptr)=block_num;
	ptr+=sizeof(int);
	int *ptr_int=(int *)ptr;
	for(int i=0;i<block_num;i++){
		for(int j=0;j<max_num_of_nodes;j++){
			*(ptr_int+i*max_num_of_nodes+j)=*(array+i*max_num_of_nodes+j);
		}
	}

	char buffer[100];
	strcpy(buffer,"meta/");
	strcat(buffer,file_name);
	FILE *fp_meta=fopen(buffer,"w");
	if(fp_meta==NULL){
		perror("Http_server_callback__cloudstr_meta,fopen meta file failed");
		free(file_buffer);
		close(client_socket_fd);
		return (void *)-1;
	}

	int ret0=fwrite(file_buffer,meta_file_buffer_len,1,fp_meta);
	if(ret0!=1){
		perror("Http_server_callback__cloudstr_meta,fwrite");
		fclose(fp_meta);
		close(client_socket_fd);
		free(file_buffer);
		return (void *)-1;
	}

	fclose(fp_meta);
	free(file_buffer);
	http_request_buffer *hrb=(http_request_buffer *)server_buffer;
	hrb->request_kind=11;
	memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
	int ret=send(client_socket_fd,server_buffer,sizeof(http_request_buffer)+4,0);
	if(ret!=(4+sizeof(http_request_buffer))){
		perror("Http_server_callback__cloudstr_meta,send failed ack to client");
		close(client_socket_fd);
		return (void *)-1;
	}

	Wait_for_socket_to_close(client_socket_fd,1000,8000000);
	return (void *)0;
}

void *Http_server_callback_query_file_meta_info(void *arg){

	if(arg==NULL){
		printf("Http_server_callback_query_file_meta_info,argument error\n");
		return (void *)-1;
	}

	callback_arg_query_file_meta_info *cb_arg_query_file_meta_info=(callback_arg_query_file_meta_info *)arg;
	int socket_fd=cb_arg_query_file_meta_info->socket_fd;
	int epoll_fd=cb_arg_query_file_meta_info->epoll_fd;
	unsigned char *server_buffer=cb_arg_query_file_meta_info->server_buffer;
	int server_buffer_size=cb_arg_query_file_meta_info->server_buffer_size;
	char *file_name=cb_arg_query_file_meta_info->file_name;
	int flag=cb_arg_query_file_meta_info->flag;

	char meta_file[100];
	strcpy(meta_file,"meta/");
	strcat(meta_file,file_name);

	FILE *meta_file_fp=fopen(meta_file,"rb+");
	if(meta_file_fp==NULL){
		printf("Http_server_callback_query_file_meta_info,file not exists\n");
		close(socket_fd);
		return (void *)-1;
	}
	fseek(meta_file_fp,0,SEEK_END);
	int file_size=ftell(meta_file_fp);
	fseek(meta_file_fp,0,SEEK_SET);

	unsigned char *buffer=(unsigned char *)malloc(file_size);
	int ret=fread(buffer,1,file_size,meta_file_fp);
	if(ret!=file_size){
		free(buffer);
		fclose(meta_file_fp);
		close(socket_fd);
		return (void *)-1;
	}

	int *ptr=(int *)(buffer+100);

	if(*ptr==1){//1 means file ready, 0 means waiting to be ready, 2 means waiting to be deleted

		http_request_buffer *hrb=(http_request_buffer *)server_buffer;
		hrb->request_kind=13;
		hrb->num1=file_size-100-sizeof(int);
		memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
		memcpy(server_buffer+sizeof(http_request_buffer)+4,buffer+100+sizeof(int),hrb->num1);
		int send_size=sizeof(http_request_buffer)+4+hrb->num1;
		int ret=send(socket_fd,server_buffer,send_size,0);


		if(flag==1){

			struct epoll_event ev;
			int epoll_fd=epoll_fd;
			ev.data.fd=socket_fd;
			ev.events=EPOLLIN | EPOLLET | EPOLLONESHOT;
			epoll_ctl(epoll_fd,EPOLL_CTL_MOD,socket_fd,&ev);

			*ptr=2;
			fseek(meta_file_fp,100,SEEK_SET);
			int ret1=fwrite(ptr,sizeof(int),1,meta_file_fp);
			if(ret1!=1){
				printf("Http_server_callback_query_file_meta_info, update meta_file failed\n");
			}
		}

		fclose(meta_file_fp);
		free(buffer);

		if(ret!=send_size){
			perror("Http_server_callback_query_file_meta_info,send meta info to client");
			close(socket_fd);
		}else{
			Wait_for_socket_to_close(socket_fd,1000,8000000);
		}

	}else{

		http_request_buffer *hrb=(http_request_buffer *)server_buffer;
		hrb->request_kind=14;
		memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
		int send_size=sizeof(http_request_buffer)+4;
		int ret=send(socket_fd,server_buffer,send_size,0);
		free(buffer);
		fclose(meta_file_fp);
		if(ret!=send_size){
			perror("Http_server_callback_query_file_meta_info,send non-meta info to client");
			close(socket_fd);
		}else{
			Wait_for_socket_to_close(socket_fd,1000,8000000);
		}
	}
}

void *Http_server_callback_update_meta_info(void *arg){

	if(arg==NULL){
		printf("Http_server_callback_update_meta_info,argument error\n");
		return (void *)-1;
	}

	callback_arg_update_meta_info *cb_arg_update_meta_info=(callback_arg_update_meta_info *)arg;
	int socket_fd=cb_arg_update_meta_info->socket_fd;
	int epoll_fd=cb_arg_update_meta_info->epoll_fd;
	unsigned char *server_buffer=cb_arg_update_meta_info->server_buffer;
	int server_buffer_size=cb_arg_update_meta_info->server_buffer_size;
	char *file_name=cb_arg_update_meta_info->file_name;
	int block_num=cb_arg_update_meta_info->block_num;
	int node_serial=cb_arg_update_meta_info->node_serial;

	char meta_file_name[100];
	strcpy(meta_file_name,"meta/");
	strcat(meta_file_name,file_name);

	FILE *meta_file_fp=fopen(meta_file_name,"r+");
	if(meta_file_fp==NULL){
		perror("Http_server_callback_update_meta_info,fopen meta file");printf("%s\n",meta_file_name );
		close(socket_fd);
		return (void *)-1;
	}

	fseek(meta_file_fp,0,SEEK_END);
	int file_size=ftell(meta_file_fp);
	fseek(meta_file_fp,0,SEEK_SET);

	char *ptr=server_buffer+sizeof(http_request_buffer)+4;
	int ret=fread(ptr,1,file_size,meta_file_fp);
	if(ret!=file_size){
		close(socket_fd);
		fclose(meta_file_fp);
		return (void *)-1;
	}else{
		int offset=100+sizeof(int)+sizeof(int)+(block_num)*max_num_of_nodes*sizeof(int)+(node_serial-1)*sizeof(int);
		fseek(meta_file_fp,offset,SEEK_SET);
		int *ptr_int=(int *)(ptr+offset);
		*ptr_int=2;//0 non 1 should 2 have stored

		int ret=fwrite(ptr_int,sizeof(int),1,meta_file_fp);

		if(ret!=1){
			close(socket_fd);
			fclose(meta_file_fp);
			return (void *)-1;
		}else{
			fclose(meta_file_fp);
			http_request_buffer *hrb=(http_request_buffer *)server_buffer;
			hrb->request_kind=15;
			hrb->num1=block_num;//start from 0
			hrb->num2=node_serial;//start from 1
			memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
			int send_size=sizeof(http_request_buffer)+4;
			int ret=send(socket_fd,server_buffer,send_size,0);

			Wait_for_socket_to_close(socket_fd,1000,8000000);
		}
	}
}


void *Http_server_callback_wait_meta_complete(void *arg){

	if(arg==NULL){
		printf("Http_server_callback_wait_meta_complete,argument error\n");
		return (void *)-1;
	}

	callback_arg_wait_meta_complete *cb_arg_wait_meta_complete=(callback_arg_wait_meta_complete *)arg;
	int socket_fd=cb_arg_wait_meta_complete->socket_fd;
	unsigned char *server_buffer=cb_arg_wait_meta_complete->server_buffer;
	int server_buffer_size=cb_arg_wait_meta_complete->server_buffer_size;
	char *file_name=cb_arg_wait_meta_complete->file_name;

	char meta_file_name[100];
	strcpy(meta_file_name,"meta/");
	strcat(meta_file_name,file_name);

	FILE *meta_file_fp=fopen(meta_file_name,"r+");
	if(meta_file_fp==NULL){
		perror("Http_server_callback_wait_meta_complete,fopen meta file");
		close(socket_fd);
		return (void *)-1;
	}

	fseek(meta_file_fp,0,SEEK_END);
	int file_size=ftell(meta_file_fp);
	fseek(meta_file_fp,0,SEEK_SET);

	int completed=1;
	char *ptr=server_buffer+sizeof(http_request_buffer)+4;
	int ret=fread(ptr,1,file_size,meta_file_fp);
	if(ret!=file_size){
		close(socket_fd);
		fclose(meta_file_fp);
		return (void *)-1;
	}else{
		char *ptr_tmp=ptr+100+sizeof(int);
		int ptr_int=*((int *)ptr_tmp);
		ptr_tmp+=sizeof(int);
		int *array=(int *)ptr_tmp;
		for(int i=0;i<ptr_int;i++){
			for(int j=0;j<max_num_of_nodes;j++){
				int status=*(array+i*max_num_of_nodes+j);
				if(status==1){
					completed=0;
					break;
				}
			}
		}

		if(completed==1){
			int offset=100;
			int *ptr_int=(int *)(ptr+offset);
			*ptr_int=1;//0 not ok 1 alreay ok 2 delete
			fseek(meta_file_fp,offset,SEEK_SET);
			int ret=fwrite(ptr_int,sizeof(int),1,meta_file_fp);
			if(ret!=1){
				close(socket_fd);
				fclose(meta_file_fp);
				return (void *)-1;
			}else{
				fclose(meta_file_fp);
				http_request_buffer *hrb=(http_request_buffer *)server_buffer;
				hrb->request_kind=17;
				memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
				int send_size=sizeof(http_request_buffer)+4;
				int ret=send(socket_fd,server_buffer,send_size,0);
				if(ret!=(send_size)){
					close(socket_fd);
					perror("Http_server_callback_wait_meta_complete,send success ack to client");
					return (void *)-1;
				}else{
					Wait_for_socket_to_close(socket_fd,1000,8000000);
				}
			}

		}else if(completed==0){
			http_request_buffer *hrb=(http_request_buffer *)server_buffer;
			hrb->request_kind=18;
			memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
			int send_size=sizeof(http_request_buffer)+4;
			int ret=send(socket_fd,server_buffer,send_size,0);
			if(ret!=(send_size)){
				close(socket_fd);
				fclose(meta_file_fp);
				perror("Http_server_callback_wait_meta_complete,send failed ack to client");
				return (void *)-1;
			}else{
				fclose(meta_file_fp);
				Wait_for_socket_to_close(socket_fd,1000,8000000);
			}
		}else{
			printf("Http_server_callback_wait_meta_complete,unknow completed flag\n");
			close(socket_fd);
			fclose(meta_file_fp);
			return (void *)-1;
		}
	}
}

void *Http_server_callback_delete_meta_file(void *arg){

	if(arg==NULL){
		printf("Http_server_callback_wait_meta_complete,argument error\n");
		return (void *)-1;
	}

	callback_arg_delete_meta_file *cb_arg_delete_meta_file=(callback_arg_delete_meta_file *)arg;
	int socket_fd=cb_arg_delete_meta_file->socket_fd;
	unsigned char *server_buffer=cb_arg_delete_meta_file->server_buffer;
	int server_buffer_size=cb_arg_delete_meta_file->server_buffer_size;
	char *file_name=cb_arg_delete_meta_file->file_name;

	char meta_file_name[100];
	strcpy(meta_file_name,"meta/");
	strcat(meta_file_name,file_name);

	int ret=-1;
	if(access(meta_file_name,F_OK)==0){
	   ret=remove(meta_file_name);
   	}else{
	   ret=0;
   	}

	if(ret==0){

		struct epoll_event ev;
		int epoll_fd=cb_arg_delete_meta_file->epoll_fd;
		ev.data.fd=socket_fd;
		ev.events=EPOLLIN | EPOLLET | EPOLLONESHOT;
		epoll_ctl(epoll_fd,EPOLL_CTL_MOD,socket_fd,&ev);

		http_request_buffer *hrb=(http_request_buffer *)server_buffer;
		hrb->request_kind=19;
		strcpy(hrb->file_name,cb_arg_delete_meta_file->file_name);
		memcpy(server_buffer+sizeof(http_request_buffer),"JDFS",4);
		int ret0=send(socket_fd,server_buffer,4+sizeof(http_request_buffer),0);
		if(ret0!=(4+sizeof(http_request_buffer))){
			close(socket_fd);
		}else{

		}
	}else{
		close(socket_fd);
	}
}
