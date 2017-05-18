/**
    JDFSFileTrans: http client component of JDFS
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
#include "../header/JDFSFileTrans.h"
int Http_connect_to_server(char *ip, int port, int *socket_fd){

    /**
     ** check argument error
     */
     if(ip==NULL || socket_fd==NULL){
        printf("Http_connect_to_server: argument error\n");
        exit(0);
     }

     *socket_fd=socket(AF_INET,SOCK_STREAM,0);
     if(*socket_fd<0){
        perror("Http_connect_to_server");
        exit(0);
     }

     struct sockaddr_in sock_address;
     sock_address.sin_family=AF_INET;
     int ret_0 =inet_pton(AF_INET,ip,&(sock_address.sin_addr.s_addr));
     if(ret_0!=1){
        printf("Http_connect_to_server: inet_pton failed\n");
        exit(0);
     }
     sock_address.sin_port=htons(port);

     int ret_1=connect(*socket_fd, (struct sockaddr*)&sock_address, sizeof(struct sockaddr_in));
     if(ret_1!=0){
        perror("Http_connect_to_server");
        exit(0);
     }

     return 1;
}

int Http_query_file_size(char *file_name, int socket_fd, long *file_size){
    /**
     ** check argument error
     */
     if(NULL==file_name){

        printf("Http_query_file_size: argument error\n");
        exit(0);
     }

     int file_name_len=strlen(file_name);
     int http_request_buffer_len=sizeof(http_request_buffer);
     unsigned char *send_buffer=(unsigned char *)malloc(file_name_len+http_request_buffer_len+1);
     http_request_buffer *rb=(http_request_buffer *)send_buffer;
     rb->request_kind=0;
     strcpy(rb->file_name, file_name);
     memcpy(send_buffer+http_request_buffer_len,file_name,file_name_len);
     send_buffer[http_request_buffer_len+file_name_len]='\0';

     int ret=send(socket_fd, send_buffer, http_request_buffer_len+file_name_len,0);
     if(ret<1){
        printf("Http_query_file_size: send failed\n");
        exit(0);
     }

     memset(send_buffer,0,file_name_len+http_request_buffer_len+1);
     unsigned char *recv_buffer=send_buffer;
     int ret_recv=recv(socket_fd,recv_buffer,500,0);
     if(ret_recv<1){
        printf("Http_query_file_size: recv failed\n");
        exit(0);
     }

     /** check response status, updated later*/

     
     http_request_buffer *rb1=(http_request_buffer *)recv_buffer;
     *file_size=rb1->num1;

     if(send_buffer!=NULL){
     	free(send_buffer);
     }

     return 1;

}

int Http_create_download_file(char *file_name, FILE **fp_download_file, int part_num){
    /**
     ** check argument error
     */
     if(file_name==NULL || fp_download_file==NULL || part_num<0){

        printf("Http_create_download_file: argument error\n");
        exit(0);
     }

     char buffer_for_part[max_download_thread+1];
     sprintf(buffer_for_part,"%d",part_num);
     int part_str_length=strlen(buffer_for_part);
     char *download_file_name=(char *)malloc((strlen(file_name)+5+part_str_length+1)*sizeof(char));
     if(NULL==download_file_name){

        printf("Http_create_download_file: malloc failed\n");
        exit(0);

     }
     strcpy(download_file_name,file_name);
     strcat(download_file_name,".part");
     strcat(download_file_name,buffer_for_part);

     if(access(download_file_name,F_OK)==0){
        int ret=remove(download_file_name);
        if(ret!=0){
            printf("Http_create_download_file: remove file failed\n");
            exit(0);
        }
     }

     *fp_download_file=fopen(download_file_name,"w+");
     if(NULL==*fp_download_file){
        printf("Http_create_download_file: fopen failed\n");
        exit(0);
     }


     if(download_file_name!=NULL){
        free(download_file_name);
     }

     return 1;
}

int Http_restore_orignal_file_name(char *download_part_file_name){
    /**
     ** check argument
     */
     if(download_part_file_name==NULL){
        printf("Http_restore_orignal_file_name: argument error\n");
        exit(0);
     }
     char *new_name=(char *)malloc(sizeof(char)*(strlen(download_part_file_name)+1));
     if(NULL==new_name){
        printf("Http_restore_orignal_file_name: malloc failed\n");
        exit(0);
     }

     strcpy(new_name,download_part_file_name);

     int file_name_length=strlen(new_name);
     for(int i=file_name_length;i>0;i--){
        if(new_name[i]=='.'){
            new_name[i]='\0';
            break;
        }
     }

     int ret=rename(download_part_file_name,new_name);
     if(ret!=0){
        printf("Http_restore_orignal_file_name: rename failed\n");
        exit(0);
     }

     return 1;
}

int Http_recv_file(int socket_fd, char *file_name, long range_begin, long range_end, unsigned char *buffer, long buffer_size, char *server_ip, int server_port){
    /**
     ** check argument
     */
     if(range_begin<0 || range_end<0 || range_end<range_begin || NULL==buffer || buffer_size<1){
        printf("Http_recv_file: rename failed\n");
        exit(0);
     }

     char send_buffer[200];
     int http_request_buffer_len=sizeof(http_request_buffer);
     int file_name_len=strlen(file_name);

     http_request_buffer *hrb=(http_request_buffer *)send_buffer;
     hrb->request_kind=2;//download
     hrb->num1=range_begin;
     hrb->num2=range_end;
     strcpy(hrb->file_name, file_name);

     memcpy(send_buffer+http_request_buffer_len,file_name,file_name_len);
     send_buffer[http_request_buffer_len+file_name_len]='\0';

     int download_size=range_end-range_begin+1;
     
     int ret0=send(socket_fd,send_buffer,http_request_buffer_len+file_name_len,0);
     if(ret0!=(http_request_buffer_len+file_name_len)){
        
        perror("Http_recv_file");
        exit(0);
     }
	
	int recv_size=0;
	int length_of_http_head=0;
    while(1){
			
        long ret=recv(socket_fd,buffer+recv_size+length_of_http_head,buffer_size,0);
        if(ret<=0){
            	
            recv_size=0;
            length_of_http_head=0;
            memset(buffer,0,buffer_size);
                
            int ret=close(socket_fd);
            if(ret!=0){
                perror("Http_recv_file");
                exit(0);
            }
				
				
            Http_connect_to_server(server_ip,server_port,&socket_fd);
            int ret0=send(socket_fd,send_buffer,http_request_buffer_len+file_name_len,0);
            if(ret0!=strlen(send_buffer)){
                perror("Http_recv_file");
                
            }

            continue;
        }

        if(recv_size==0){	 
            http_request_buffer *hrb=(http_request_buffer *)buffer;

            if(hrb->num1!=range_begin || hrb->num2!=range_end){

                printf("Http_recv_file: expected range do not match recv range\n");
                exit(0);
            }

            char *ptr1=strstr(buffer+http_request_buffer_len,"JDFS");
            if(ptr1==NULL){
                printf("Http_recv_file: http header not correct\n");
                exit(0);
            }

            length_of_http_head=ptr1-(char*)buffer+4;              
            recv_size=recv_size+ret-length_of_http_head;
				
        }else{

            recv_size+=ret;

        }      
            	
        if(recv_size==download_size){
            break;
        }
			
    }

     return 1;
}

int Save_download_part_of_file(FILE *fp, unsigned char *buffer, long buffer_size, long file_offset){
    /**
     ** check argument error
     */
     if(NULL==fp || NULL==buffer || buffer_size<1 || file_offset<0){
        printf("Save_download_part_of_file: argument error\n");
        exit(0);
     }


     fseek(fp,file_offset,SEEK_SET);
     int ret=fwrite(buffer,buffer_size,1,fp);
     if(ret!=1){
        printf("Save_download_part_of_file: fwrite failed\n");
        exit(0);
     }
    
     return 1;

}

int Http_create_breakpoint_file(char *file_name, FILE **fp_breakpoint, long file_size, long num_of_part_file, long size_of_one_piece,
																					   long total_num_of_piece_of_whole_file,
																					   char *server_ip, int server_port){
    /**
     ** check argument error
     */
     if(file_name==NULL || fp_breakpoint==NULL){

        printf("Http_create_breakpoint_file: argument error\n");
        exit(0);
     }

     char *break_point_file_name=(char *)malloc((strlen(file_name)+4+1)*sizeof(char));
     if(NULL==break_point_file_name){

        printf("Http_create_breakpoint_file: malloc failed\n");
        exit(0);

     }
     strcpy(break_point_file_name,file_name);
     strcat(break_point_file_name,".jbp");

     if(access(break_point_file_name,F_OK)==0){
        int ret=remove(break_point_file_name);
        if(ret!=0){
            perror("Http_create_breakpoint_file,remove,\n");
            exit(0);
        }
     }

     *fp_breakpoint=fopen(break_point_file_name,"w+");
     if(NULL==*fp_breakpoint){
        printf("Http_create_breakpoint_file: fopen failed\n");
        exit(0);
     }

     unsigned char *break_point_buffer=(unsigned char *)malloc(sizeof(break_point)+1000);
     if(NULL==break_point_buffer){
            printf("Http_create_breakpoint_file: malloc failed\n");
            exit(0);
     }

     ((break_point *)break_point_buffer)->file_size=file_size;
     ((break_point *)break_point_buffer)->num_of_part_file=num_of_part_file;
     ((break_point *)break_point_buffer)->size_of_one_piece=size_of_one_piece;
     ((break_point *)break_point_buffer)->total_num_of_piece_of_whole_file=total_num_of_piece_of_whole_file;
     ((break_point *)break_point_buffer)->server_port=server_port;

     memcpy(((break_point *)break_point_buffer)->server_ip, server_ip, strlen(server_ip));
     ((break_point *)break_point_buffer)->server_ip[strlen(server_ip)]='\0';

     
     int ret_fwrite=fwrite(break_point_buffer,sizeof(break_point),1,*fp_breakpoint);
     fflush(*fp_breakpoint);

     if(ret_fwrite!=1){
            printf("Http_create_breakpoint_file: fwrite failed \n");
            exit(0);
     }

     if(break_point_file_name!=NULL){
        free(break_point_file_name);
     }

     return 1;
}

int Http_create_breakpoint_part_file(char *file_name, FILE **fp_breakpoint_part, int part_num, long start_num_of_piece_of_this_part_file,
																				 long end_num_of_piece_of_this_part_file,
																				 long size_of_last_incompelet_piece,
																				 long alread_download_num_of_piece){
	if(file_name==NULL || fp_breakpoint_part==NULL || part_num<0){
		printf("Http_create_breakpoint_part_file, argument error\n");
		exit(0);
	}

	char buffer_for_part_num[6];
	sprintf(buffer_for_part_num, "%d",part_num);
	int part_num_str_len=strlen(buffer_for_part_num);
	char *break_point_part_file_name=(char *)malloc((strlen(file_name)+4+part_num_str_len+1)*sizeof(char));
	if(break_point_part_file_name==NULL){
		printf("Http_create_breakpoint_part_file,malloc failed\n");
		exit(0);
	}

	strcpy(break_point_part_file_name,file_name);
	strcat(break_point_part_file_name,".jbp");
	strcat(break_point_part_file_name,buffer_for_part_num);

	if(access(break_point_part_file_name,F_OK)==0){
		int ret=remove(break_point_part_file_name);
		if(ret!=0){
			perror("Http_create_breakpoint_part_file,remove");
			exit(0);
		}
	}

	*fp_breakpoint_part=fopen(break_point_part_file_name, "w+");
	if(*fp_breakpoint_part==NULL){
		printf("Http_create_breakpoint_part_file,fopen failed\n");
		exit(0);
	}

	break_point_of_part bpt;
	bpt.start_num_of_piece_of_this_part_file=start_num_of_piece_of_this_part_file;
	bpt.end_num_of_piece_of_this_part_file=end_num_of_piece_of_this_part_file;
	bpt.size_of_last_incompelet_piece=size_of_last_incompelet_piece;
	bpt.alread_download_num_of_piece=alread_download_num_of_piece;


	int ret=fwrite(&bpt, sizeof(break_point_of_part), 1, *fp_breakpoint_part);
	if(ret!=1){
		printf("Http_create_breakpoint_part_file,fwrite, break_point_of_part,error\n");
		exit(0);
	}

	
	fflush(*fp_breakpoint_part);


	if(break_point_part_file_name!=NULL){
		free(break_point_part_file_name);
	}

	return 0;

}

int Update_breakpoint_part_file(FILE *fp_breakpoint_part, int num_of_piece_tobe_added){

	if(fp_breakpoint_part==NULL || num_of_piece_tobe_added<1){
		printf("Update_breakpoint_part_file,argument error\n");
		exit(0);
	}

	break_point_of_part *bpt=(break_point_of_part *)malloc(sizeof(break_point_of_part));
	if(bpt==NULL){
		printf("Update_breakpoint_part_file,malloc failed\n");
		exit(0);
	}
	fseek(fp_breakpoint_part, 0, SEEK_SET);
	int ret_fread=fread(bpt, sizeof(break_point_of_part), 1, fp_breakpoint_part);

	int start_num=bpt->start_num_of_piece_of_this_part_file;
	int end_num=bpt->end_num_of_piece_of_this_part_file;
	bpt->alread_download_num_of_piece=bpt->alread_download_num_of_piece+num_of_piece_tobe_added;
	if((bpt->alread_download_num_of_piece)<=(bpt->end_num_of_piece_of_this_part_file-bpt->start_num_of_piece_of_this_part_file+1+1)){	
		fseek(fp_breakpoint_part, 0, SEEK_SET);
		int ret=fwrite(bpt, sizeof(break_point_of_part), 1, fp_breakpoint_part);
		if(ret!=1){
			printf("Update_breakpoint_part_file,fwrite failed\n");
			exit(0);
		}
		fflush(fp_breakpoint_part);
	}else{
		printf("Update_breakpoint_part_file, num_of_piece_tobe_added not correct\n");
		exit(0);
	}

}


int Delete_breakpoint_file(char *file_name,FILE *fp){

	if(file_name==NULL || fp==NULL){
		printf("Delete_breakpoint_file, argument error\n");
		exit(0);
	}

	break_point *bp=(break_point *)malloc(sizeof(break_point));
	fseek(fp, 0, SEEK_SET);
	int ret=fread(bp, sizeof(break_point), 1, fp);
	if(ret!=1){
		printf("Delete_breakpoint_file,fread failed\n");
		exit(0);
	}
	int num=bp->num_of_part_file;

    char *buffer=(char *)malloc((strlen(file_name)+4+100));
	for(int i=0;i<num;i++){

		char buffer_part[6];
		sprintf(buffer_part, "%d",i);
		strcpy(buffer,file_name);
		strcat(buffer,".jbp");
		strcat(buffer,buffer_part);

		if(access(buffer, F_OK)==0){
			if(remove(buffer)!=0){
				perror("Delete_breakpoint_file,remove .jbp_num");
				exit(0);
			}
		}

	}

	

	fclose(fp);
	strcpy(buffer, file_name);
	strcat(buffer, ".jbp");
	
	if(access(buffer, F_OK)==0){
		if(remove(buffer)!=0){
			perror("Delete_breakpoint_file,remove .jbp");
			exit(0);
		}
	}

	if(buffer!=NULL){
		free(buffer);
	}

	return 0;

}


int JDFS_http_download(char *file_name, char *server_ip, int server_port){
	/**
     ** check argument 
     */

	if(file_name==NULL || server_ip==NULL || server_port<0){
		printf("JDFS_http_download: argument error\n");
		exit(0);
	}

	int socket_fd=-1;
	Http_connect_to_server(server_ip, server_port, &socket_fd);

	long file_size=0;
	Http_query_file_size(file_name, socket_fd, &file_size);

	int piece_num=file_size/(download_one_piece_size);
	int size_of_last_piece=file_size%(download_one_piece_size);


	FILE *fp_breakpoint_file=NULL;
	Http_create_breakpoint_file(file_name, &fp_breakpoint_file, file_size, number_of_part_file, download_one_piece_size,piece_num,server_ip,server_port);

	FILE *fp_breakpoint_part_file=NULL;
	Http_create_breakpoint_part_file(file_name, &fp_breakpoint_part_file, 0, 1, piece_num, size_of_last_piece, 0);

	FILE *fp_download_file=NULL;
	Http_create_download_file(file_name, &fp_download_file, 0);

	long buffer_size=(download_one_piece_size)+1024;
	unsigned char *buffer=(unsigned char *)malloc(sizeof(unsigned char)*buffer_size);

	if(NULL==buffer){
		printf("JDFS_http_download: malloc failed\n");
		exit(0);
	}

	for(int i=1; i<=piece_num; i++){
		memset(buffer,0,buffer_size);

		long range_begin=(i-1)*(download_one_piece_size);
		long range_end=i*(download_one_piece_size)-1;

		Http_recv_file(socket_fd, file_name, range_begin, range_end, buffer, buffer_size, server_ip, server_port);

		long offset=(i-1)*(download_one_piece_size);
		long http_request_buffer_len=sizeof(http_request_buffer);
		char *ptr=buffer+http_request_buffer_len+4;
		if(NULL==ptr){
			printf("JDFS_http_download: recv file sees not correct\n");
			exit(0);	
		}

		
		Save_download_part_of_file(fp_download_file, ptr, (download_one_piece_size), offset);
		Update_breakpoint_part_file(fp_breakpoint_part_file, 1);
	}

	if(size_of_last_piece>0){
     	memset(buffer,0,buffer_size);
     	
        long range_begin=piece_num*(download_one_piece_size);
        long range_end=range_begin+size_of_last_piece-1;
		
        Http_recv_file(socket_fd, file_name,range_begin,range_end,buffer,buffer_size,server_ip,server_port);
		
        long offset=piece_num*(download_one_piece_size);
		long http_request_buffer_len=sizeof(http_request_buffer);
		char *ptr=buffer+http_request_buffer_len+4;
        
        if(NULL==ptr){
            printf("JDFS_http_download:recv file seems not correct\n");
            exit(0);
        }
		
       

        Save_download_part_of_file(fp_download_file,ptr,(range_end-range_begin+1),offset);	
        if(fclose(fp_breakpoint_part_file)!=0){
        	printf("JDFS_http_download:fclose failed\n");
        }
        	
	}

	if(fclose(fp_download_file)!=0){
		printf("JDFS_http_download: fclose file failed\n");
	}

	if(buffer!=NULL){
		free(buffer);
	}

	char *download_file_name_part=(char *)malloc(sizeof(char)*(strlen(file_name)+6+1));
	strcpy(download_file_name_part,file_name);
	strcat(download_file_name_part,".part0");

	Http_restore_orignal_file_name(download_file_name_part);
	Delete_breakpoint_file(file_name, fp_breakpoint_file);

	return 1;
}


int JDFS_http_download_jbp(char *file_name){
	/**
     ** check argument 
     */

	if(file_name==NULL){
		printf("JDFS_http_download: argument error\n");
		exit(0);
	}

	char *file_name_jbp=(char *)malloc((strlen(file_name))+4+1);
	if(file_name_jbp==NULL){
		printf("JDFS_http_download_jbp,malloc failed\n");
		exit(0);
	}
	strcpy(file_name_jbp,file_name);
	strcat(file_name_jbp,".jbp");
	FILE *fp=fopen(file_name_jbp, "r+");
	if(fp==NULL){
		printf("JDFS_http_download_jbp,fopen failed\n");
		exit(0);
	}

	break_point *bp=(break_point *)malloc(sizeof(break_point));

	if(bp==NULL){
		printf("JDFS_http_download_jbp,malloc failed\n");
		exit(0);
	}

	int ret=fread(bp, sizeof(break_point), 1, fp);
	if(ret!=1){
		printf("JDFS_http_download_jbp,fread failed\n");
		exit(0);
	}

	int num_of_part_file=bp->num_of_part_file;
	int size_of_one_piece=bp->size_of_one_piece;
	int total_piece=bp->total_num_of_piece_of_whole_file;

	char server_ip[128+1];
	strcpy(server_ip, bp->server_ip);
	int server_port=bp->server_port;

	int socket_fd=-1;
	Http_connect_to_server(server_ip, server_port, &socket_fd);


	long buffer_size=(size_of_one_piece)+1024;
	unsigned char *buffer=(unsigned char *)malloc(sizeof(unsigned char)*buffer_size);

	if(NULL==buffer){
		printf("JDFS_http_download: malloc failed\n");
		exit(0);
	}


	char *part_jbp_file_name=(char *)malloc(strlen(file_name)+100);
	char *part_file_name=(char *)malloc(strlen(file_name)+100);
	FILE *fp_jbp_part=NULL;
	FILE *fp_part=NULL;
	for(int i=0; i<number_of_part_file; i++){
		
		char temp_buffer[6];
		sprintf(temp_buffer, "%d",i);
		strcpy(part_jbp_file_name,file_name);
		strcat(part_jbp_file_name,".jbp");
		strcat(part_jbp_file_name,temp_buffer);

		strcpy(part_file_name, file_name);
		strcat(part_file_name,".part");
		strcat(part_file_name,temp_buffer);

		fp_jbp_part=fopen(part_jbp_file_name, "r+");
		fp_part=fopen(part_file_name,"r+");
		if(fp_part==NULL || fp_jbp_part==NULL){
			printf("JDFS_http_download_jbp,fopen failed\n");
			exit(0);
		}

		break_point_of_part bpt;
		int ret=fread(&bpt, sizeof(break_point_of_part), 1, fp_jbp_part);
		if(ret!=1){
			printf("JDFS_http_download_jbp,fread failed\n");
			exit(0);
		}

		long begin_piece=bpt.start_num_of_piece_of_this_part_file+bpt.alread_download_num_of_piece;
		long end_piece=bpt.end_num_of_piece_of_this_part_file;
		long size_of_last_incompelet_piece=bpt.size_of_last_incompelet_piece;
		
		for(int i=begin_piece;i<=end_piece;i++){

			memset(buffer, 0, buffer_size);
			long range_begin=(i-1)*size_of_one_piece;
			long range_end=i*(size_of_one_piece)-1;
			Http_recv_file(socket_fd, file_name, range_begin, range_end, buffer, buffer_size, server_ip, server_port);
			long offset=(i-1)*(download_one_piece_size);
			long http_request_buffer_len=sizeof(http_request_buffer);

			char *ptr=buffer+http_request_buffer_len+4;
			if(NULL==ptr){
				printf("JDFS_http_download: recv file sees not correct\n");
				exit(0);	
			}

			Save_download_part_of_file(fp_part, ptr, (download_one_piece_size), offset);
			Update_breakpoint_part_file(fp_jbp_part, 1);
						

		}

		if(size_of_last_incompelet_piece>0){

			memset(buffer, 0, buffer_size);
			long range_begin=total_piece*size_of_one_piece;
			long range_end=range_begin+size_of_last_incompelet_piece-1;
			Http_recv_file(socket_fd, file_name,range_begin,range_end,buffer,buffer_size,server_ip,server_port);

			long offset=total_piece*size_of_one_piece;
			long http_request_buffer_len=sizeof(http_request_buffer);
			char *ptr=buffer+http_request_buffer_len+4;

			if(NULL==ptr){
            	printf("JDFS_http_download_jbp:recv file seems not correct\n");
            	exit(0);
        	}

        	Save_download_part_of_file(fp_part, ptr, (range_end-range_begin+1), offset);
        		
		}

		fclose(fp_jbp_part);	
		
	}

	
	if(buffer!=NULL){
		free(buffer);
	}

	char *download_file_name_part=(char *)malloc(sizeof(char)*(strlen(file_name)+6+1));
	strcpy(download_file_name_part,file_name);
	strcat(download_file_name_part,".part0");

	Http_restore_orignal_file_name(download_file_name_part);

	Delete_breakpoint_file(file_name, fp);

	return 1;
}

int JDFS_http_upload(char *file_name, char *server_ip, int server_port){

    if(file_name==NULL || server_ip==NULL || server_port<0){
        printf("JDFS_http_upload, argument error\n");
        exit(0);
    }

    FILE *fp=NULL;
    fp=fopen(file_name,"r");
    if(fp==NULL){
        perror("JDFS_http_upload");
        exit(0);
    }

    fseek(fp, 0, SEEK_END);
    long file_size=ftell(fp);

    int upload_loop_num=file_size/(upload_one_piece_size);
    int size_of_last_piece=file_size%upload_one_piece_size;

    int socket_fd=-1;
    Http_connect_to_server(server_ip, server_port, &socket_fd);

    int upload_buffer_len=sizeof(http_request_buffer)+upload_one_piece_size;
    unsigned char *upload_buffer=(unsigned char *)malloc(upload_buffer_len+100);
    if(upload_buffer==NULL){
        printf("JDFS_http_upload,malloc failed\n");
        exit(0);
    }

    http_request_buffer *hrb=(http_request_buffer *)upload_buffer;
    hrb->request_kind=1;
    strcpy(hrb->file_name, file_name);
    memcpy(upload_buffer+sizeof(http_request_buffer), "JDFS", 4);

    for(int i=0;i<upload_loop_num;i++){

        printf("upload %s to server, piece: %d---%d\n",file_name, i,upload_loop_num);

        long offset=i*upload_one_piece_size;
        fseek(fp, offset, SEEK_SET);      
        hrb->num1=offset;
        hrb->num2=offset+upload_one_piece_size-1;
       
        int ret=fread(upload_buffer+sizeof(http_request_buffer)+4, upload_one_piece_size, 1, fp);
        if(ret!=1){
            printf("JDFS_http_upload,fread failed\n");
            exit(0);
        }

        while(1){
            int ret=send(socket_fd, upload_buffer, upload_buffer_len+4, 0);
            if(ret==(upload_buffer_len+4)){
                break;
            }else{
                int ret=close(socket_fd);
                if(ret==0){
                    Http_connect_to_server(server_ip, server_port, &socket_fd);
                    continue;
                }else{
                    perror("JDFS_http_upload, close");
                    exit(0);
                }
            }
        }

    }

    if(size_of_last_piece>0){

        long offset=upload_loop_num*upload_one_piece_size;
        fseek(fp, offset, SEEK_SET);
       
        hrb->num1=offset;
        hrb->num2=offset+size_of_last_piece-1;

        int ret=fread(upload_buffer+sizeof(http_request_buffer)+4, size_of_last_piece, 1, fp);
        if(ret!=1){
            printf("JDFS_http_upload,fread failed\n");
            exit(0);
        }

        while(1){
            int ret=send(socket_fd,upload_buffer,sizeof(http_request_buffer)+size_of_last_piece+4,0);
            if(ret==(sizeof(http_request_buffer)+size_of_last_piece+4)){
                break;
            }else{
                int ret=close(socket_fd);
                if(ret==0){
                    Http_connect_to_server(server_ip, server_port, &socket_fd);
                    continue;
                }else{
                    perror("JDFS_http_upload, close");
                    exit(0);
                }
            }
        }
    }
    
    return 0;
}

int main(int argc, char const *argv[])
{
	char *ip="192.168.137.132";
	char *file_name="APUE-en.pdf";
	int port=8888;
	/*if(argc==1){
		JDFS_http_download(file_name, ip, port);
	}else if(argc==2){
		JDFS_http_download_jbp(file_name);
	}else{
		printf("main,error occured\n");
	}*/
    file_name="CRLS-en.pdf";
    JDFS_http_upload(file_name, ip, port);
	

	return 0;
}