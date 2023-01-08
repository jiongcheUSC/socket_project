#include "common.h"

//return <=0 if error, return socket if sucess
int init_udp(uint16_t port,const char *ip)
{
	int sockfd = socket(AF_INET,SOCK_DGRAM,0);
	if(sockfd == -1)
	{
		printf("init_udp socket failed\n");
		return -1;
	}

	struct sockaddr sa;
	struct sockaddr_in *p_sa_in = (struct sockaddr_in *)&sa;
	sa.sa_family = AF_INET;
	p_sa_in->sin_port = htons(port);
	p_sa_in->sin_addr.s_addr = inet_addr(ip);
	if(p_sa_in->sin_addr.s_addr == INADDR_NONE)
	{
		printf("init_udp ip addr is error,ip=[%s]\n",ip);
		return -2;
	}

	if(bind(sockfd,&sa,sizeof(sa)) == -1)
	{
		printf("init_udp bind failed,port=[%u]ip=[%s]errmsg[%s] \n",port,ip,strerror(errno));
		return -3;
	}

	return sockfd;
}

int init_tcp_server(uint16_t port,const char *ip)
{
	int sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd == -1)
	{
		printf("init_tcp_server socket failed\n");
		return -1;
	}

	//set TCP_NODELAY
	int flag = 1;
	if(setsockopt(sockfd,IPPROTO_TCP,TCP_NODELAY,&flag,sizeof(flag))==-1)
	{
		printf("set TCP_NODELAY failed,sockfd=%d\n",sockfd);
		return -2;
	}

	struct sockaddr sa;
	struct sockaddr_in *p_sa_in = (struct sockaddr_in *)&sa;
	sa.sa_family = AF_INET;
	p_sa_in->sin_port = htons(port);
	p_sa_in->sin_addr.s_addr = inet_addr(ip);
	if(p_sa_in->sin_addr.s_addr == INADDR_NONE)
	{
		printf("init_tcp_server ip addr is error,ip=[%s]\n",ip);
		return -3;
	}

	if(bind(sockfd,&sa,sizeof(sa)) == -1)
	{
		printf("init_tcp_server bind failed,port=[%u]ip=[%s]errmsg[%s] \n",port,ip,strerror(errno));
		return -4;
	}

	if(listen(sockfd,5) == -1)
	{
		printf("init_tcp_server listen failed,port=[%u]ip=[%s]errmsg[%s] \n",port,ip,strerror(errno));
		return -5;
	}

	return sockfd;
}

int init_tcp_client(uint16_t port,const char *ip)
{
	int sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd == -1)
	{
		printf("init_tcp_client socket failed\n");
		return -1;
	}

	int flag = 1;
	if(setsockopt(sockfd,IPPROTO_TCP,TCP_NODELAY,&flag,sizeof(flag))==-1)
	{
		printf("set TCP_NODELAY failed,sockfd=%d\n",sockfd);
		return -2;
	}

	
	struct sockaddr sa;
	struct sockaddr_in *p_sa_in = (struct sockaddr_in *)&sa;
	sa.sa_family = AF_INET;
	p_sa_in->sin_port = htons(port);
	p_sa_in->sin_addr.s_addr = inet_addr(ip);

	/*
	if(bind(sockfd,&sa,sizeof(sa)) == -1)
	{
		printf("init_tcp_client bind failed,port=[%u]ip=[%s]errmsg[%s]\n",port,ip,strerror(errno));
		return -4;
	}
	*/
	if(connect(sockfd,&sa,sizeof(sa)) == -1)
	{
		printf("init_tcp_client connect failed,,port=[%u]ip=[%s]errmsg[%s]\n",port,ip,strerror(errno));
	}

	return sockfd;
}

int client(const char *client_name,uint16_t remote_port,int argc,char *argv[])
{
	char buff[MSG_BUFF_MAX_SIZE];		//send and recv buffer
	uint16_t buff_len;					//Number of bytes want to send
	int send_len;						//Number of bytes actually sent
	int recv_len;						//Number of bytes receive 
	char username[USER_NAME_MAX_SIZE];	//first input parameters
	char username_to[USER_NAME_MAX_SIZE];//recipient/RECEIVER_USERNAME
	int amount;							//Transfer_Amount
	struct TAckToClient tAckToClient;

	printf("The %s is up and running.\n",client_name);

	int sockfd = init_tcp_client(remote_port,HOST_NAME);
	if(sockfd<=0)
		return -1;

	if(argc == 2 && !strcmp(argv[1],"TXLIST"))//TXLIST
	{
		sprintf(buff,"TXLIST");

		//send
		buff_len = strlen(buff);
		send_len = send(sockfd,buff,buff_len,0);
		if(send_len!=buff_len)
		{
			printf("send TXLIST failed,send_len=%d,buff=[%s]\n",send_len,buff);
			return -1;
		}

		printf("%s sent a sorted list request to the main server.\n",client_name);

	}
	else if(argc == 2)//CHECK WALLET
	{
		snprintf(username,sizeof(username),"%s",argv[1]);
		sprintf(buff,"CHECK %s",username);

		//send
		buff_len = strlen(buff);
		send_len = send(sockfd,buff,buff_len,0);
		if(send_len!=buff_len)
		{
			printf("send failed,send_len=%d,buff=[%s]\n",send_len,buff);
			return -1;
		}

		printf("\"%s\" sent a balance enquiry request to the main server.\n",username);

		recv_len = recv(sockfd,&tAckToClient,sizeof(tAckToClient),0);
		if(recv_len!=sizeof(tAckToClient))
		{
			printf("recv failed ,recv_len=%d,errmsg=%s\n",recv_len,strerror(errno));
			return -1;
		}
		if(tAckToClient.result_code == 1)//success
		{
			printf("The current balance of \"%s\" is : %d alicoins.\n",username,ntohl(tAckToClient.balance_amount));
			
		}
		else if(tAckToClient.result_code == 2)//sender is not exists
		{
			printf("Unable to proceed with the transaction as"
					" \"%s\" is not part of the network.\n",username);
		}
		else//shouldn't happen
		{
			printf("CHECK receive err result_code=[%u]\n",tAckToClient.result_code);
		}

	}
	else if(argc == 4)//TXCOINS
	{
		snprintf(username,sizeof(username),"%s",argv[1]);
		snprintf(username_to,sizeof(username_to),"%s",argv[2]);
		amount = atoi(argv[3]);

		sprintf(buff,"TXCOINS %s %s %d",username,username_to,amount);

		//send
		buff_len = strlen(buff);
		send_len = send(sockfd,buff,buff_len,0);
		if(send_len!=buff_len)
		{
			printf("send failed,send_len=%d,buff=[%s]\n",send_len,buff);
			return -1;
		}

		printf("\"%s\" has requested to transfer %d coins to \"%s\".\n",username,amount,username_to);

		recv_len = recv(sockfd,&tAckToClient,sizeof(tAckToClient),0);
		if(recv_len!=sizeof(tAckToClient))
		{
			printf("recv failed ,recv_len=%d,errmsg=%s\n",recv_len,strerror(errno));
			return -1;
		}
		if(tAckToClient.result_code == 1)//success
		{
			printf("\"%s\" successfully transferred %d alicoins to \"%s\".\n",username,amount,username_to);
			printf("The current balance of \"%s\" is : %d alicoins.\n",username,ntohl(tAckToClient.balance_amount));
		}
		else if(tAckToClient.result_code == 2)//sender is not exists
		{
			printf("Unable to proceed with the transaction as"
					" \"%s\" is not part of the network.\n",username);
		}
		else if(tAckToClient.result_code == 3)//receiver is not exists
		{
			printf("Unable to proceed with the transaction as"
					" \"%s\" is not part of the network.\n",username_to);
		}
		else if(tAckToClient.result_code == 4)//both are not exists
		{
			printf("Unable to proceed with the transaction as"
					" \"%s\" and \"%s\" are not part of the network.\n",username,username_to);
		}
		else if(tAckToClient.result_code == 5)//insufficient balance
		{
			printf("\"%s\" was unable to transfer %d alicoins to \"%s\" because of insufficient balance.\n"
				,username,amount,username_to);
			printf("The current balance of \"%s\" is : %d alicoins.\n"
				,username,ntohl(tAckToClient.balance_amount));
		}
		else//shouldn't happen
		{
			printf("CHECK receive err result_code=[%u]\n",tAckToClient.result_code);
		}

	}
	else if(argc == 3 && !strcmp(argv[2],"stats"))
	{
		snprintf(username,sizeof(username),"%s",argv[1]);
		sprintf(buff,"STATS %s",username);

		//send
		buff_len = strlen(buff);
		send_len = send(sockfd,buff,buff_len,0);
		if(send_len!=buff_len)
		{
			printf("send failed,send_len=%d,buff=[%s]\n",send_len,buff);
			return -1;
		}

		printf("\"%s\" sent a stats request to the main server.\n",username);

		printf("\"%s\" statistics are the following.:\n",username);

		int i;
		for(i=1;;i++)
		{
			struct TRecord tRecord;
			recv_len = recv(sockfd,&tRecord,sizeof(tRecord),0);
			if(recv_len!=sizeof(tRecord))
			{
				printf("recv tRecord failed ,recv_len=%d,errmsg=%s\n",recv_len,strerror(errno));
				return -1;
			}

			tRecord.serial_no = ntohl(tRecord.serial_no);

			if( tRecord.serial_no == -1)
				break;
			
			tRecord.transfer_amount = ntohl(tRecord.transfer_amount);
			
			if( !strcmp(tRecord.sender,username) )
				printf("%3d %16s %3d %5d\n",i,tRecord.receiver,tRecord.serial_no,-tRecord.transfer_amount);
			else 
				printf("%3d %16s %3d %5d\n",i,tRecord.sender,tRecord.serial_no,tRecord.transfer_amount);
		}

		
	}
	else
	{
		printf("Incorrect number of input parameters,argc=%d\n",argc);
		return -1;
	}

	
	close(sockfd);

	return 0;
}

int server(const char *server_name,uint16_t local_port,const char *filename)
{
	char buff[MSG_BUFF_MAX_SIZE];
	char username[USER_NAME_MAX_SIZE];
	char username_to[USER_NAME_MAX_SIZE];
	int send_len;

	struct TRecord arrTRecord[BLOCK_FILE_MAX_ROWS];
	int arrTRecord_offset = 0;//current arrTRecord rows used
	int i;
	uint8_t result_code;

	printf("The %s is up and running using UDP on port %u.\n",server_name,local_port);

	//deal file begin ==file to array
	FILE *fp=fopen(filename,"r+");
	if(fp == NULL)
	{
		printf("open file failed,filename=%s\n",filename);
		return -1;
	}
	
	//read file to array
	while(fgets(buff,sizeof(buff),fp))
	{
		if(strlen(buff) <= 7)//fault-tolerant
		{
			if(fseek(fp,-strlen(buff),1)!=0)//Ignore blank 
			{
				printf("fseek failed\n");
				return -1;
			}
			break;
		}
		
		char *token = strtok(buff," ");
		arrTRecord[arrTRecord_offset].serial_no = atoi(token);
		token = strtok(NULL," ");
		sprintf(arrTRecord[arrTRecord_offset].sender,"%s",token);
		token = strtok(NULL," ");
		sprintf(arrTRecord[arrTRecord_offset].receiver,"%s",token);
		token = strtok(NULL,"\n");
		arrTRecord[arrTRecord_offset].transfer_amount = atoi(token);
		/*
		printf("[%d][%s][%s][%d]\n"
			,arrTRecord[arrTRecord_offset].serial_no
			,arrTRecord[arrTRecord_offset].sender
			,arrTRecord[arrTRecord_offset].receiver
			,arrTRecord[arrTRecord_offset].transfer_amount);
		*/
		arrTRecord_offset++;
	}
	//printf("arrTRecord_offset=%d\n",arrTRecord_offset);

	//Retrieve the last character
	if(arrTRecord_offset>0)
	{
		if(fseek(fp,-1,1)!=0)//get end char
		{
			printf("fseek failed 2\n");
			return -1;
		}

		//If the last character is not a newline character, a newline character is added
		if(fgetc(fp) != '\n')
		{
			fputc('\n',fp);
			fflush(fp);
		}
	}
	//deal file end
	
	
	//deal socket begin
	int sockfd = init_udp(local_port,HOST_NAME);
	if(sockfd<=0)
		return -1;

	struct sockaddr addrM;
	struct sockaddr_in *p_in_tmp;

	p_in_tmp = (struct sockaddr_in *)&addrM;
	p_in_tmp->sin_family = AF_INET;
	p_in_tmp->sin_port = htons(SERVER_M_UDP_PORT);
	p_in_tmp->sin_addr.s_addr = inet_addr(HOST_NAME);
	
	while(1)
	{
		int recv_len = recv(sockfd,buff,sizeof(buff),0);
		if(recv_len<=0)
		{
			printf("recv failed,recv_len=%d,errmsg=%s\n",recv_len,strerror(errno));
			return -1;
		}

		buff[recv_len] = '\0';
		//printf("receive [%s]\n",buff);

		if(!strncmp(buff,"CHECK",sizeof("CHECK")-1))//if CHECK WALLET
		{
			printf("The %s received a request from the Main Server.\n",server_name);

			int max_serial_no = 0;
			sprintf(username,"%s",buff + sizeof("CHECK"));
			for(i = 0;i<arrTRecord_offset;i++)
			{
				if(!strcmp(arrTRecord[i].sender,username)||!strcmp(arrTRecord[i].receiver,username))
				{
					struct TRecord tRecordSend;//Host sequence to network sequence
					tRecordSend = arrTRecord[i];
					tRecordSend.serial_no = htonl(tRecordSend.serial_no);
					tRecordSend.transfer_amount = htonl(tRecordSend.transfer_amount);
					send_len = sendto(sockfd,&tRecordSend,sizeof(tRecordSend),0,&addrM,sizeof(addrM));
					if(send_len!=sizeof(tRecordSend))
					{
						printf("sendto failed 1,send_len=%d,errmsg=%s\n",send_len,strerror(errno));
						return -1;
					}
				}
				max_serial_no = max_serial_no > arrTRecord[i].serial_no?max_serial_no:arrTRecord[i].serial_no;
			}

			//send end msg with max serial_no
			struct TRecord tRecordEnd;
			tRecordEnd.serial_no = htonl(-1);//Indicates the end
			tRecordEnd.transfer_amount = htonl(max_serial_no);
			send_len = sendto(sockfd,&tRecordEnd,sizeof(tRecordEnd),0,&addrM,sizeof(addrM));
			if(send_len!=sizeof(tRecordEnd))
			{
				printf("sendto failed 2,send_len=%d,errmsg=%s\n",send_len,strerror(errno));
				return -1;
			}

			printf("The %s finished sending the response to the Main Server.\n",server_name);
		}
		else if(!strncmp(buff,"TXCOINS",sizeof("TXCOINS")-1))//if CHECK WALLET
		{
			printf("The %s received a request from the Main Server.\n",server_name);

			int max_serial_no = 0;
			char * token = strtok(buff + sizeof("TXCOINS")," ");
			sprintf(username,"%s",token);
			token = strtok(NULL," ");
			sprintf(username_to,"%s",token);

			for(i = 0;i<arrTRecord_offset;i++)
			{
				if( !strcmp(arrTRecord[i].sender,username)
				||!strcmp(arrTRecord[i].receiver,username)
				||!strcmp(arrTRecord[i].sender,username_to)
				||!strcmp(arrTRecord[i].receiver,username_to) )
				{
					struct TRecord tRecordSend;//Host sequence to network sequence
					tRecordSend = arrTRecord[i];
					tRecordSend.serial_no = htonl(tRecordSend.serial_no);
					tRecordSend.transfer_amount = htonl(tRecordSend.transfer_amount);
					send_len = sendto(sockfd,&tRecordSend,sizeof(tRecordSend),0,&addrM,sizeof(addrM));
					if(send_len!=sizeof(tRecordSend))
					{
						printf("sendto failed 1,send_len=%d,errmsg=%s\n",send_len,strerror(errno));
						return -1;
					}
				}
				max_serial_no = max_serial_no > arrTRecord[i].serial_no?max_serial_no:arrTRecord[i].serial_no;
			}

			//send end msg with max serial_no
			struct TRecord tRecordEnd;
			tRecordEnd.serial_no = htonl(-1);//Indicates the end
			tRecordEnd.transfer_amount = htonl(max_serial_no);
			send_len = sendto(sockfd,&tRecordEnd,sizeof(tRecordEnd),0,&addrM,sizeof(addrM));
			if(send_len!=sizeof(tRecordEnd))
			{
				printf("sendto failed 2,send_len=%d,errmsg=%s\n",send_len,strerror(errno));
				return -1;
			}

			printf("The %s finished sending the response to the Main Server.\n",server_name);
		}
		else if(!strncmp(buff,"WRITE_FILE",sizeof("WRITE_FILE")-1))
		{
			char buff_for_write[MSG_BUFF_MAX_SIZE];
			sprintf(buff_for_write,"%s\n",buff + sizeof("WRITE_FILE"));

			char * token = strtok(buff + sizeof("WRITE_FILE")," ");//strtok will change buff
			arrTRecord[arrTRecord_offset].serial_no = atoi(token);
			token = strtok(NULL," ");
			sprintf(arrTRecord[arrTRecord_offset].sender,"%s",token);
			token = strtok(NULL," ");
			sprintf(arrTRecord[arrTRecord_offset].receiver,"%s",token);
			token = strtok(NULL," ");
			arrTRecord[arrTRecord_offset].transfer_amount = atoi(token);
			
			arrTRecord_offset++;

			if(fputs(buff_for_write,fp) == -1)
			{
				printf("fputs failed,buff_for_write=%s\n",buff_for_write);

				result_code = 6;
				send_len = sendto(sockfd,&result_code,sizeof(result_code),0,&addrM,sizeof(addrM));
				if(send_len!=sizeof(result_code))
				{
					printf("sendto failed 3,send_len=%d,errmsg=%s\n",send_len,strerror(errno));
					return -1;
				}

				return -1;
			}
			fflush(fp);

			//return success
			result_code = 1;
			send_len = sendto(sockfd,&result_code,sizeof(result_code),0,&addrM,sizeof(addrM));
			if(send_len!=sizeof(result_code))
			{
				printf("sendto failed 4,send_len=%d,errmsg=%s\n",send_len,strerror(errno));
				return -1;
			}
		}
		else if(!strncmp(buff,"TXLIST",sizeof("TXLIST")-1))
		{
			printf("The %s received a request from the Main Server.\n",server_name);

			for(i = 0;i<arrTRecord_offset;i++)
			{
				struct TRecord tRecordSend;//Host sequence to network sequence
				tRecordSend = arrTRecord[i];
				tRecordSend.serial_no = htonl(tRecordSend.serial_no);
				tRecordSend.transfer_amount = htonl(tRecordSend.transfer_amount);
				send_len = sendto(sockfd,&tRecordSend,sizeof(tRecordSend),0,&addrM,sizeof(addrM));
				if(send_len!=sizeof(tRecordSend))
				{
					printf("sendto TXLIST failed 1,send_len=%d,errmsg=%s\n",send_len,strerror(errno));
					return -1;
				}
			}

			//send end msg with max serial_no
			struct TRecord tRecordEnd;
			tRecordEnd.serial_no = htonl(-1);//Indicates the end
			send_len = sendto(sockfd,&tRecordEnd,sizeof(tRecordEnd),0,&addrM,sizeof(addrM));
			if(send_len!=sizeof(tRecordEnd))
			{
				printf("sendto TXLIST failed 2,send_len=%d,errmsg=%s\n",send_len,strerror(errno));
				return -1;
			}

			printf("The %s finished sending the response to the Main Server.\n",server_name);
		}
		else if(!strncmp(buff,"STATS",sizeof("STATS")-1))
		{
			printf("The %s received a request from the Main Server.\n",server_name);

			sprintf(username,"%s",buff + sizeof("STATS"));

			for(i = 0;i<arrTRecord_offset;i++)
			{
				if(!strcmp(arrTRecord[i].sender,username)
					||!strcmp(arrTRecord[i].receiver,username) )
				{
					struct TRecord tRecordSend;//Host sequence to network sequence
					tRecordSend = arrTRecord[i];
					tRecordSend.serial_no = htonl(tRecordSend.serial_no);
					tRecordSend.transfer_amount = htonl(tRecordSend.transfer_amount);
					send_len = sendto(sockfd,&tRecordSend,sizeof(tRecordSend),0,&addrM,sizeof(addrM));
					if(send_len!=sizeof(tRecordSend))
					{
						printf("sendto STATS failed 1,send_len=%d,errmsg=%s\n",send_len,strerror(errno));
						return -1;
					}
				}
			}

			//send end msg with max serial_no
			struct TRecord tRecordEnd;
			tRecordEnd.serial_no = htonl(-1);//Indicates the end
			send_len = sendto(sockfd,&tRecordEnd,sizeof(tRecordEnd),0,&addrM,sizeof(addrM));
			if(send_len!=sizeof(tRecordEnd))
			{
				printf("sendto STATS failed 2,send_len=%d,errmsg=%s\n",send_len,strerror(errno));
				return -1;
			}

			printf("The %s finished sending the response to the Main Server.\n",server_name);
		}
		else
		{
			printf("receive error msg=%s\n",buff);
			return -1;
		}

		

	}

	//The end of the program will automatically close the File descriptors
	//So it's OK not to close here
	fclose(fp);
	close(sockfd);

	return 0;
}