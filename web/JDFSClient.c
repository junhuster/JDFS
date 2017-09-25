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
#include "../header/JDFSClient.h"

extern int Http_connect_to_server(char *ip, int port, int *socket_fd);
extern int JDFS_http_upload(char *file_name, char *server_ip, int server_port, char nodes_8_bits, int total_num_of_blocks);
int JDFS_http_download(char *file_name, char *server_ip, int server_port);

int JDFS_cloud_query_node_list( char *server_ip, int server_port, node_list *nli){

	if(server_ip==NULL || server_port<0 || nli==NULL){
		printf("JDFS_cloud_query_node_list: argument error\n");
		return -1;
	}
    int socket_fd=-1;
	Http_connect_to_server(server_ip, server_port, &socket_fd);

	unsigned char *request_buffer=(unsigned char *)malloc(sizeof(http_request_buffer)+4);
	http_request_buffer *hrb=(http_request_buffer *)request_buffer;
	hrb->request_kind=5;
    int http_request_buffer_len=sizeof(http_request_buffer);
	memcpy(request_buffer+http_request_buffer_len,"JDFS",4);

	int ret0=send(socket_fd,request_buffer,http_request_buffer_len+4,0);
	if(ret0!=http_request_buffer_len+4){
		perror("JDFS_cloud_query_node_list,send");
		close(socket_fd);
		return -1;
	}
	unsigned char *buffer=(unsigned char *)malloc(4+max_num_of_nodes*sizeof(res_nodelist)+4);
    int ret=recv(socket_fd,buffer,4+max_num_of_nodes*sizeof(res_nodelist)+4,0);
	int *ptr=(int *)buffer;
	if(ret==*ptr){
		int res_num=(*ptr-4-4)/(sizeof(res_nodelist));
		nli->total_num_of_nodes=res_num;
		for(int i=0;i<res_num;i++){
			res_nodelist * res=(res_nodelist *)(buffer+4+i*sizeof(res_nodelist));
			(nli->serial_num_of_each_node)[i]=res->node_serial;
			(nli->port_of_each_node)[i]=res->port;
			strcpy((nli->ip_str_of_each_node)[i],res->ip_str);
			printf("name:%s, serial num: %d, ip str:%s, port:%d\n",res->node_name,res->node_serial,res->ip_str,res->port);
		}

	}else{
		printf("JDFS_cloud_query_node_list, recv failed or none node alive,ret=%d\n",ret);
	}
	close(socket_fd);
}

int JDFS_cloud_store_file(char *file_name, int flag){

	if(file_name==NULL){
		printf("JDFS_cloud_store_file,argument error\n");
		return -1;
	}

	node_list nli;
	JDFS_cloud_query_node_list(namenode_ip_def,namenode_port_def,&nli);
	int backup_num=3;
	if(nli.total_num_of_nodes <0){
		printf("JDFS_cloud_store_file,currently no data node alive,please try later\n");
		return -1;
	}

	if(nli.total_num_of_nodes<backup_num){
		backup_num=nli.total_num_of_nodes;
	}

	char nodes_8_bits=0;
	for(int i=0;i<backup_num;i++){
		nodes_8_bits = nodes_8_bits | (1<<i);
	}

	int block_num=-1;
	if(flag==0){
		block_num=1;
	}else{
		block_num=Split_file_into_several_parts(file_name,block_size);
		if(block_num<1){
			return -1;
		}
	}

	//send to namenode to create the meta info for this file_name
	//first int: block_num, following each block and its nodelist
	int content_len=sizeof(int)+block_num*max_num_of_nodes*sizeof(int);
	unsigned char *meta_file_info_buffer=(unsigned char *)malloc(sizeof(http_request_buffer)+4+content_len);
	if(meta_file_info_buffer==NULL){
		printf("JDFS_cloud_store_file,malloc meta file info buffer failed\n");
		return -1;
	}

	char *ptr=meta_file_info_buffer+sizeof(http_request_buffer)+4;
	*((int *)(ptr))=block_num;
	ptr+=sizeof(int);
	int *array=(int *)ptr;
	for(int i=0;i<block_num;i++){
		for(int j=0;j<max_num_of_nodes;j++){
			*(array+i*max_num_of_nodes+j)=0;
		}
	}

	for(int i=0;i<8;i++){
		if(nodes_8_bits & (1<<i)){
			int node_serial=nli.serial_num_of_each_node[i];
			for(int j=0;j<block_num;j++){
				*(array+j*max_num_of_nodes+node_serial-1)=1;
			}
		}
	}

	//now the meta info ructed send it to namenode server
	while (1) {

		http_request_buffer *hrb=(http_request_buffer *)meta_file_info_buffer;
		hrb->request_kind=11;
		hrb->num1=content_len;
		strcpy(hrb->file_name,file_name);
		int socket_fd=-1;
		Http_connect_to_server(namenode_ip_def,namenode_port_def,&socket_fd);
		int ret=send(socket_fd,meta_file_info_buffer,4+sizeof(http_request_buffer)+content_len,0);
		if(ret!=(4+sizeof(http_request_buffer)+content_len)){
			close(socket_fd);
			continue;
		}else{
			int ret=recv(socket_fd,meta_file_info_buffer,sizeof(http_request_buffer)+4,0);
			if(ret!=(sizeof(http_request_buffer)+4)){
				close(socket_fd);
				continue;
			}else{
				http_request_buffer *hrb=(http_request_buffer *)meta_file_info_buffer;
				if(hrb->request_kind==11){
					close(socket_fd);
					break;
				}else{
					close(socket_fd);
					continue;
				}
			}
		}
	}

	free(meta_file_info_buffer);

	if(flag==0){//don't split
		int ret=JDFS_cloud_store_one_block(file_name,1,nodes_8_bits,&nli,block_num);//second parameter start from 1
		if(ret==0){
			printf("file %s transfor success\n", file_name);
		}else{
      		printf("file %s transfor failed\n", file_name);
			return -1;
		}
  	}else if(flag==1){//split file

		char tmp_buffer[8];
		char *new_file_name=(char *)malloc(strlen(file_name)+10);
		if(new_file_name==NULL){
			printf("JDFS_cloud_store_file,malloc failed\n");
			return -1;
		}

		for(int i=1;i<=block_num;i++){//block num start from 1

			sprintf(tmp_buffer,"%d",i-1);
			strcpy(new_file_name,file_name);
			strcat(new_file_name,".blk");
			strcat(new_file_name,tmp_buffer);

			int ret=JDFS_cloud_store_one_block(new_file_name,i,nodes_8_bits,&nli,block_num);
			if(ret==0){
				printf("JDFS_cloud_store_file, %s transfor success\n", file_name);
			}else{
	      		printf("JDFS_cloud_store_file %s transfor failed\n", file_name);
				return -1;
			}
		}

		int ret0=JDFS_wait_file_transform_completed(file_name, namenode_ip_def, namenode_port_def);
		if(ret0==0){
			printf("JDFS_cloud_store_file: file %s store successfully\n",file_name);
		}

	}else{
		printf("JDFS_cloud_store_file, flag error\n");
		return -1;
	}

	return 0;
}

int JDFS_cloud_store_one_block(char *file_name, int block_num,char nodes_8_bits, node_list *nli, int total_num_of_blocks){

	if(file_name==NULL || nli==NULL){
		printf("JDFS_cloud_store_one_block,argument error\n");
		return -1;
	}

	char temp_8_bits=0;
	char *ip_str=NULL;
	int   port=0;
	int first_found=0;
	for(int i=0;i<8;i++){
		if(nodes_8_bits & (1<<i)){
			if(first_found==0){
				first_found=1;
				ip_str=(nli->ip_str_of_each_node)[i];
				port=(nli->port_of_each_node)[i];
			}else{
				int node_serial=(nli->serial_num_of_each_node)[i];
				temp_8_bits=temp_8_bits | (1<<(node_serial-1));
			}
		}
	}

	JDFS_http_upload(file_name,ip_str,port,temp_8_bits,total_num_of_blocks);
	return 0;
}


int Split_file_into_several_parts(char *file_name,int part_size){

	if(file_name==NULL || part_size<1){
		printf("Split_file_into_several_parts,argument error\n");
		return -1;
	}

	char temp_buffer[100];
	strcpy(temp_buffer,"file/");
	strcat(temp_buffer,file_name);
	FILE *fp_read=fopen(temp_buffer,"r");
	if(fp_read==NULL){
		perror("Split_file_into_several_parts,read file failed,fopen");
		return -1;
	}

	fseek(fp_read,0,SEEK_END);
	long file_size=ftell(fp_read);

	fseek(fp_read,0,SEEK_SET);

	int block_num=file_size/part_size;
	int last_block_size=file_size%part_size;

	int file_name_len=strlen(file_name);
	char *file_name_buffer=(char *)malloc(file_name_len+10);
	char tmp_buffer[6];

	for(int i=1;i<=block_num+1;i++){

		int range_begin=(i-1)*part_size;
		int range_end=-1;
		if(i==block_num+1){
			if(last_block_size>0){
				range_end=range_begin+last_block_size-1;
			}else{
				range_end=i*part_size-1;
			}
		}else{
			range_end=i*part_size-1;
		}

		sprintf(tmp_buffer,"%d",i-1);
		strcpy(file_name_buffer,file_name);
		strcat(file_name_buffer,".blk");
		strcat(file_name_buffer,tmp_buffer);

		int ret=Extract_part_file_and_store(fp_read,file_name_buffer,range_begin,range_end);
		if(ret!=0){
			return -1;
		}
	}

	fclose(fp_read);
	free(file_name_buffer);

	if(last_block_size>0){
		return block_num+1;
	}else{
		return (block_num);
	}
}

int Extract_part_file_and_store(FILE *fp,char *new_file_name,int range_begin,int range_end){

	if(fp==NULL || new_file_name==NULL || range_begin>range_end){
		printf("Extract_part_file_and_store,argument error\n");
		return -1;
	}

	char temp_buffer[100];
	strcpy(temp_buffer,"file/");
	strcat(temp_buffer,new_file_name);
	FILE *new_fp=fopen(temp_buffer,"w");
	if(new_fp==NULL){
		perror("Extract_part_file_and_store,fopen");
		return -1;
	}

	int needed_processed_size=range_end-range_begin+1;
	int one_piece=1024*1024;
	if(one_piece>range_end-range_begin+1){
		one_piece=range_end-range_begin+1;
	}
	unsigned char *file_buffer=(unsigned char *)malloc(one_piece);
	if(file_buffer==NULL){
		printf("Extract_part_file_and_store,malloc failed\n");
		return -1;
	}
	fseek(fp,range_begin,SEEK_SET);
	while(1){

		int ret=-1;
		if(one_piece<needed_processed_size){
			ret=fread(file_buffer,1,one_piece,fp);
		}else{
			ret=fread(file_buffer,1,needed_processed_size,fp);
		}

		if(ret==0 && needed_processed_size!=0){
			printf("Extract_part_file_and_store,process seems not correct\n");
			return -1;
		}

		if(ret<0){
			printf("Extract_part_file_and_store,fread failed\n");
			return -1;
		}

		int ret0=fwrite(file_buffer,1,ret,new_fp);
		needed_processed_size-=ret;
		if(needed_processed_size==0){
			break;
		}
	}
	fclose(new_fp);
	return 0;
}

int JDFS_wait_file_transform_completed( char *file_name,  char *ip_str, int port){

	if(file_name==NULL || ip_str==NULL || port<0){
		printf("JDFS_wait_file_transform_completed,argument error\n");
		return -1;
	}

	unsigned char *buffer=(unsigned char *)malloc(sizeof(http_request_buffer)+4);
	if(buffer==NULL){
		printf("JDFS_wait_file_transform_completed,malloc failed\n");
		return -1;
	}

	http_request_buffer *hrb=(http_request_buffer *)buffer;
	while (1) {

		hrb->request_kind=17;
		strcpy(hrb->file_name,file_name);
		memcpy(buffer+sizeof(http_request_buffer),"JDFS",4);
		int socket_fd=-1;
		while (1) {
			int ret=Http_connect_to_server(ip_str,port,&socket_fd);
			if(ret==0){
				break;
			}else{
				usleep(1000);
				printf("JDFS_wait_file_transform_completed,connect to server failed, retry\n");
				continue;
			}
		}

		int ret=send(socket_fd,buffer,4+sizeof(http_request_buffer),0);
		if(ret!=(4+sizeof(http_request_buffer))){
			close(socket_fd);
			continue;
		}else{
			int ret=recv(socket_fd,buffer,4+sizeof(http_request_buffer),0);
			if(ret!=(4+sizeof(http_request_buffer))){
				close(socket_fd);
				continue;
			}else{
				http_request_buffer *hrb=(http_request_buffer *)buffer;
				if(hrb->request_kind==17){
					close(socket_fd);
					break;
				}else{
					close(socket_fd);
					usleep(1000);
					continue;
				}
			}
		}
	}
	free(buffer);
	return 0;
}

int JDFS_cloud_query_file(char *file_name, JDFS_file_info *jfi, int flag){

	if(file_name==NULL || jfi==NULL || (flag!=0 && flag!=1)){
		printf("JDFS_cloud_query_file,argument error\n");
		return -1;
	}

	unsigned char *buffer=(unsigned char *)malloc(sizeof(http_request_buffer)+4+100*max_num_of_nodes*sizeof(int));//assume max block 100
	if(buffer==NULL){
		printf("JDFS_cloud_query_file,malloc failed\n");
		return -1;
	}

	while (1) {

		http_request_buffer *hrb=(http_request_buffer *)buffer;
		hrb->request_kind=13;
		hrb->num1=flag;
		strcpy(hrb->file_name,file_name);
		memcpy(buffer+sizeof(http_request_buffer),"JDFS",4);

		int socket_fd=-1;
		Http_connect_to_server(namenode_ip_def,namenode_port_def,&socket_fd);
		int ret=send(socket_fd,buffer,sizeof(http_request_buffer)+4,0);
		if(ret!=(4+sizeof(http_request_buffer))){
			close(socket_fd);
			continue;
		}else{
			int ret=recv(socket_fd,buffer,4+sizeof(http_request_buffer),0);
			if(ret==(sizeof(http_request_buffer)+4)){
				http_request_buffer *hrb=(http_request_buffer *)buffer;
				if(hrb->request_kind==13){
					int content_len=hrb->num1;
					int ret=recv(socket_fd,buffer,content_len,0);
					if(ret!=content_len){
						close(socket_fd);
						continue;
					}else{
						close(socket_fd);
						char *ptr=buffer;
						int block_num=*((int *)(ptr));
						jfi->total_block_num_of_this_file=block_num;
						ptr+=sizeof(int);
						int *array=(int *)ptr;
						int nodes_num_of_this_block=0;
						for(int i=0;i<block_num;i++){
							nodes_num_of_this_block=-1;
							each_block_info *ebi=&((jfi->block_array)[i]);
							for(int j=0;j<max_num_of_nodes;j++){
								int status=*(array+i*max_num_of_nodes+j);
								if(status==2){
									nodes_num_of_this_block+=1;
									ebi->serial_num_of_each_node[nodes_num_of_this_block]=j;
								}else{

								}
							}
							ebi->total_num_nodes_of_this_block=nodes_num_of_this_block+1;
						}
						break;
					}
				}else{
					close(socket_fd);
					continue;
				}
			}else{
				close(socket_fd);
				continue;
			}
		}
	}

	return 0;
}


int JDFS_cloud_query_ip_from_node_serial(int serial_num, char *ip_str){//serial_num start from 1

	if(serial_num<1 || serial_num>max_num_of_nodes){
		printf("JDFS_cloud_query_ip_from_node_serial,argument error\n");
		return -1;
	}

	unsigned char *buffer=(unsigned char *)malloc(sizeof(http_request_buffer)+4);
	if(buffer==NULL){
		printf("JDFS_cloud_query_ip_from_node_serial, malloc failed\n");
		return -1;
	}

	http_request_buffer *hrb=(http_request_buffer *)buffer;
	hrb->request_kind=8;
	hrb->num1=serial_num;

	memcpy(buffer+sizeof(http_request_buffer),"JDFS",4);

	int socket_fd_to_server=-1;
	Http_connect_to_server(namenode_ip_def,namenode_port_def,&socket_fd_to_server);

	int ret=send(socket_fd_to_server,buffer,sizeof(http_request_buffer)+4,0);
	if(ret!=(4+sizeof(http_request_buffer))){
		close(socket_fd_to_server);
		free(buffer);
		return -1;
	}

	memset(buffer,0,4+sizeof(http_request_buffer));
	int ret0=recv(socket_fd_to_server,buffer,4+sizeof(http_request_buffer),0);
	if(ret0!=(4+sizeof(http_request_buffer))){
		close(socket_fd_to_server);
		free(buffer);
		return -1;
	}

	hrb=(http_request_buffer *)buffer;
	if(hrb->request_kind!=8){
		printf("JDFS_cloud_query_ip_from_node_serial, ack info wrong, retry\n");
		close(socket_fd_to_server);
		free(buffer);
		return -1;
	}else{
		strcpy(ip_str,hrb->file_name);
		close(socket_fd_to_server);
		free(buffer);
		return 0;
	}
}

int JDFS_cloud_fetch_file(char *file_name){

	if(file_name==NULL){
		printf("JDFS_cloud_fetch_file, argument error\n");
		return -1;
	}

	JDFS_file_info jfi;
  	JDFS_cloud_query_file(file_name,&jfi,0);

	int block_num=jfi.total_block_num_of_this_file;

	for(int i=0;i<block_num;i++){

		each_block_info *ebi=&((jfi.block_array)[i]);
		int total_num_nodes_of_this_block=ebi->total_num_nodes_of_this_block;
		int *serial_num_of_each_node=ebi->serial_num_of_each_node;
		for(int j=0;j<total_num_nodes_of_this_block;j++){

			int real_node_serial=serial_num_of_each_node[total_num_nodes_of_this_block-1]+1;//at server point, node serial start from 1
			char ip_str[100];
			while(1){
				int ret=JDFS_cloud_query_ip_from_node_serial(real_node_serial,ip_str);
				if(ret==0){
					break;
				}else{
					printf("JDFS_cloud_fetch_file, query ip, retry\n");
					continue;
				}
			}

			char buffer[100];
			strcpy(buffer,file_name);
			strcat(buffer,".blk");
			char temp[8];
			sprintf(temp,"%d",i);
			strcat(buffer,temp);
		    JDFS_http_download(buffer, ip_str, 8888);
			break;//later we will add a method to select optimal node for downloading
		}
	}

	if(block_num==1){

	}else if(block_num>1){
		int ret=JDFS_cloud_merge_file(file_name,block_num);
		if(ret==0){
			printf("file %s fetched and merge succeed\n",file_name);
		}else{
			printf("file %s fetched but merge failed\n", file_name);
		}
	}

	return 0;
}

int JDFS_cloud_merge_file(char *file_name, int num_of_blocks){

	if(file_name==NULL || num_of_blocks<1){
		printf("JDFS_cloud_merge_file,argument error\n");
		return -1;
	}

	char buffer[100];
	strcpy(buffer,"fetch/");
	strcat(buffer,file_name);
	FILE *fp_file=fopen(buffer,"w");
	if(fp_file==NULL){
		printf("JDFS_cloud_merge_file, fopen failed\n");
		return -1;
	}

	int piece_size=1024*1024;
	char *file_buffer=(char *)malloc(piece_size);
	if(file_buffer==NULL){
		printf("JDFS_cloud_merge_file,malloc failed\n");
		return -1;
	}
	for(int i=0;i<num_of_blocks;i++){

		char buffer1[100];
		char temp[8];
		strcpy(buffer1,buffer);
		sprintf(temp,"%d",i);
		strcat(buffer1,".blk");
		strcat(buffer1,temp);

		FILE *fp=fopen(buffer1,"r");
		if(fp==NULL){
			printf("JDFS_cloud_merge_file,fopen failed\n");
			return -1;
		}

		int ret=-1;
		while((ret=fread(file_buffer,1,piece_size,fp))!=0){
			fwrite(file_buffer,1,ret,fp_file);
		}

		fclose(fp);
		remove(buffer1);

	}

	return 0;
}

int JDFS_cloud_delete_file(char *file_name){

	if(file_name==NULL){
		printf("JDFS_cloud_delete_file, argument error\n");
		return -1;
	}

	JDFS_file_info jfi;
  	JDFS_cloud_query_file(file_name,&jfi,1);

	int block_num=jfi.total_block_num_of_this_file;

	for(int i=0;i<block_num;i++){

		each_block_info *ebi=&((jfi.block_array)[i]);
		int total_num_nodes_of_this_block=ebi->total_num_nodes_of_this_block;
		int *serial_num_of_each_node=ebi->serial_num_of_each_node;
		for(int j=0;j<total_num_nodes_of_this_block;j++){

			int real_node_serial=serial_num_of_each_node[j]+1;//at server point, node serial start from 1
			char ip_str[100];
			while(1){
				int ret=JDFS_cloud_query_ip_from_node_serial(real_node_serial,ip_str);
				if(ret==0){
					break;
				}else{
					printf("JDFS_cloud_fetch_file, query ip, retry\n");
					continue;
				}
			}

			char buffer[100];
			if(block_num==1){
				strcpy(buffer,file_name);
			}else{
				strcpy(buffer,file_name);
				strcat(buffer,".blk");
				char temp[8];
				sprintf(temp,"%d",i);
				strcat(buffer,temp);
			}

		    int ret=JDFS_cloud_delete_file_internal(buffer,ip_str,8888);
			if(ret!=0){
				printf("JDFS_cloud_delete_file,internal delete failed\n");
				return -1;
			}else{

			}

		}
	}

	int ret1=JDFS_cloud_delete_meta_file(file_name, namenode_ip_def, namenode_port_def);
	if(ret1==0){
		printf("JDFS_cloud_delete_file: name node delete meta file succeeds\n");
	}else{
		printf("JDFS_cloud_delete_file: name node delete meta file fialed\n");
	}

	return 0;
}


int JDFS_cloud_delete_file_internal(char *file_name, char *ip_str, int port){

	if(file_name==NULL || ip_str==NULL || port<0){
		printf("JDFS_cloud_delete_file_internal,argument error\n");
		return -1;
	}

	char buffer[1000];
	while(1){

		int socket_fd=-1;
		while(1){
			int ret=Http_connect_to_server(ip_str,port,&socket_fd);
			if(ret==0){
				break;
			}
		}

		http_request_buffer *hrb=(http_request_buffer *)buffer;
		hrb->request_kind=20;
		strcpy(hrb->file_name,file_name);
		memcpy(buffer+sizeof(http_request_buffer),"JDFS",4);
		int ret=send(socket_fd,buffer,4+sizeof(http_request_buffer),0);
		if(ret!=(4+sizeof(http_request_buffer))){
			close(socket_fd);
			continue;
		}else{
			int ret=recv(socket_fd,buffer,4+sizeof(http_request_buffer),0);
			if(ret!=(4+sizeof(http_request_buffer))){
				close(socket_fd);
				continue;
			}else{
				http_request_buffer *hrb=(http_request_buffer *)buffer;
				if(hrb->request_kind==20){
					close(socket_fd);
					break;
				}else{
					close(socket_fd);
					continue;
				}
			}
		}
	}

	return 0;
}

int JDFS_cloud_delete_meta_file(char *file_name, char *ip_str, int port){

	if(file_name==NULL || ip_str==NULL || port<0){
		printf("JDFS_cloud_delete_meta_file, argument error\n");
		return -1;
	}

	char buffer[1000];
	int socket_fd=-1;
	int ret=Http_connect_to_server(ip_str,port,&socket_fd);
	if(ret!=0){
		printf("JDFS_cloud_delete_meta_file, connect to server failed\n");
		return -1;
	}

	http_request_buffer *hrb=(http_request_buffer *)buffer;
	hrb->request_kind=19;
	strcpy(hrb->file_name,file_name);
	memcpy(buffer+sizeof(http_request_buffer),"JDFS",4);
	int ret1=send(socket_fd,buffer,4+sizeof(http_request_buffer),0);
	if(ret1!=(4+sizeof(http_request_buffer))){
		close(socket_fd);
		return -1;
	}else{
		int ret=recv(socket_fd,buffer,4+sizeof(http_request_buffer),0);
		if(ret!=(4+sizeof(http_request_buffer))){
			close(socket_fd);
			return -1;
		}else{
			http_request_buffer *hrb=(http_request_buffer *)buffer;
			if(hrb->request_kind==19){
				close(socket_fd);
				return 0;
			}else{
				close(socket_fd);
				return -1;
			}
		}
	}
	return 0;
}


int JDFS_cloud_display_file_info(char *file_name){

	if(file_name==NULL){
		printf("JDFS_cloud_display_file_info, argument error\n");
		return -1;
	}

	JDFS_file_info jfi;
  	JDFS_cloud_query_file(file_name,&jfi,0);

	int total_num_of_blocks=jfi.total_block_num_of_this_file;
	printf("%s has %d blocks\n",file_name,total_num_of_blocks);

	for(int i=0;i<total_num_of_blocks;i++){

		each_block_info *ebi=&((jfi.block_array)[i]);
		int total_num_of_nodes=ebi->total_num_nodes_of_this_block;
		printf("block %d located at %d data nodes: ",i, total_num_of_nodes);
		int *serial_num_of_each_node=ebi->serial_num_of_each_node;
		for(int j=0;j<total_num_of_nodes;j++){
			int data_node_serial=serial_num_of_each_node[j]+1;
			printf("%d ",data_node_serial);
		}

		printf("\n");

	}

	return 0;
}
