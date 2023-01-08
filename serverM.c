#include "common.h"

static int send_check(int sockfd_udp,const struct sockaddr* pAddr
			,const char *username,char server_name);
static int receive_check(int sockfd_udp,const char *username
			,int *p_is_username_exists,int *p_sum_amount_one_server
			,int *p_max_serial_no,char server_name);

static int send_txcoins(int sockfd_udp,const struct sockaddr* pAddr
					,const char *username,const char *username_to,char server_name);
static int receive_txcoins(int sockfd_udp,const char *username,const char *username_to
			,int *p_is_username_exists,int *p_is_username_to_exists
			,int *p_sum_amount_one_server,int *p_max_serial_no
			,char server_name);

static int send_txlist(int sockfd_udp,const struct sockaddr* pAddr);
static int receive_txlist(int sockfd_udp,struct TRecord *arrTRecord,int *p_arrTRecord_offset);

static void sort_record(struct TRecord *arrTRecord,int rows);

static int send_stats(int sockfd_udp,const struct sockaddr* pAddr,const char *username);


int main()
{
	int sockfd_udp;		//udp for server A B C
	int sockfd_tcp_A;	//tcp for client A
	int sockfd_tcp_B;	//tcp for clinet B

	char buff[MSG_BUFF_MAX_SIZE];//send or receive buffer
	int buff_len;
	int send_len;
	int recv_len;
	struct TAckToClient tAckToClient;

	char username[USER_NAME_MAX_SIZE];	//first input parameters
	char username_to[USER_NAME_MAX_SIZE];//recipient/RECEIVER_USERNAME
	int amount;							//Transfer_Amount

	printf("The main server is up and running.\n");

	sockfd_udp = init_udp(SERVER_M_UDP_PORT,HOST_NAME);
	if(sockfd_udp <= 0)
		return -1;

	sockfd_tcp_A = init_tcp_server(SERVER_M_TCP_PORT_A,HOST_NAME);
	if(sockfd_tcp_A <= 0)
		return -2;

	sockfd_tcp_B = init_tcp_server(SERVER_M_TCP_PORT_B,HOST_NAME);
	if(sockfd_tcp_B <= 0)
		return -3;

	//printf("sockfds = %d %d %d\n",sockfd_udp,sockfd_tcp_A,sockfd_tcp_B);

	struct sockaddr addrA;//address of server A
	struct sockaddr addrB;//address of server B
	struct sockaddr addrC;//address of server C
	struct sockaddr_in *p_in_tmp;

	p_in_tmp = (struct sockaddr_in *)&addrA;
	p_in_tmp->sin_family = AF_INET;
	p_in_tmp->sin_port = htons(SERVER_A_UDP_PORT);
	p_in_tmp->sin_addr.s_addr = inet_addr(HOST_NAME);

	p_in_tmp = (struct sockaddr_in *)&addrB;
	p_in_tmp->sin_family = AF_INET;
	p_in_tmp->sin_port = htons(SERVER_B_UDP_PORT);
	p_in_tmp->sin_addr.s_addr = inet_addr(HOST_NAME);

	p_in_tmp = (struct sockaddr_in *)&addrC;
	p_in_tmp->sin_family = AF_INET;
	p_in_tmp->sin_port = htons(SERVER_C_UDP_PORT);
	p_in_tmp->sin_addr.s_addr = inet_addr(HOST_NAME);

	int sockfd_max = sockfd_tcp_B > sockfd_tcp_A ? sockfd_tcp_B: sockfd_tcp_A;//for select() function

	fd_set readfds;
	
	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(sockfd_tcp_A,&readfds);
		FD_SET(sockfd_tcp_B,&readfds);

		int ready_cnt = select(sockfd_max+1,&readfds,NULL,NULL,NULL);
		
		if(ready_cnt>0)//success to receive one or more msg
		{
			int connfd;
			uint16_t local_tcp_port;//only for print
			char client_name;//only for print

			if(FD_ISSET(sockfd_tcp_A,&readfds))//client A connect msg
			{
				//printf("receive client A connect\n");
				connfd = accept(sockfd_tcp_A,NULL,NULL);
				if(connfd == -1)
				{
					printf("accept failed,sockfd_tcp_A=%d\n",sockfd_tcp_A);
					return -1;
				}
				local_tcp_port = SERVER_M_TCP_PORT_A;
				client_name = 'A';
			}
			else if(FD_ISSET(sockfd_tcp_B,&readfds))//client B connect msg
			{
				//printf("receive client B connect\n");
				connfd = accept(sockfd_tcp_B,NULL,NULL);
				if(connfd == -1)
				{
					printf("accept failed,sockfd_tcp_B=%d\n",sockfd_tcp_B);
					return -1;
				}
				local_tcp_port = SERVER_M_TCP_PORT_B;
				client_name = 'B';
			}
			else//shouldn't happen
			{
				printf("err,ready_cnt=%d\n",ready_cnt);
				return -1;
			}

			//recv command
			recv_len = recv(connfd,buff,sizeof(buff),0);
			if(recv_len < 0)
			{
				printf("recv failed,recv_len=%d,errmsg=%s\n",recv_len,strerror(errno));
				return -1;
			}
			else if(recv_len == 0)
			{
				printf("err:connected, but no receive msg, then close,connfd=%d\n",connfd);
				close(connfd);
				continue;
			}

			int sum_amount_one_server;//sum_amount for one server;Excluding the initial 1000
			int sum_amount = 1000;//sum_amount for all server;
			int max_serial_no;//max serial_no for one server
			int max_serial_no_all = 0;//max serial_no for all server
			int is_username_exists;//1:exists,0:not exists
			int is_username_exists_all = 0;//0:not exists in any server, otherwise >0
			int is_username_to_exists;//1:exists,0:not exists
			int is_username_to_exists_all = 0;//0:not exists in any server, otherwise >0
			char server_name;
			struct TRecord arrTRecord[BLOCK_FILE_MAX_ROWS*3];
			int arrTRecord_offset = 0;

			//Parse command
			buff[recv_len] = '\0';
			//printf("receive msg,buff=[%s]\n",buff);

			if(!strncmp(buff,"CHECK",sizeof("CHECK")-1))//if CHECK WALLET
			{
				sprintf(username,"%s",buff + sizeof("CHECK"));
				printf("The main server received input=\"%s\" from the client using TCP over port %u.\n"
						,username,local_tcp_port);
				
				//loop check server A,B,C
				server_name = 'A';
				if(send_check(sockfd_udp,&addrA,username,server_name))
					return -1;
				
				if(receive_check(sockfd_udp,username,&is_username_exists
							,&sum_amount_one_server,&max_serial_no,server_name))
					return -1;

				sum_amount += sum_amount_one_server;
				max_serial_no_all = max_serial_no_all > max_serial_no? max_serial_no_all: max_serial_no;
				is_username_exists_all += is_username_exists;
			
				server_name = 'B';
				if(send_check(sockfd_udp,&addrB,username,server_name))
					return -1;
				
				if(receive_check(sockfd_udp,username,&is_username_exists
							,&sum_amount_one_server,&max_serial_no,server_name))
					return -1;
				
				sum_amount += sum_amount_one_server;
				max_serial_no_all = max_serial_no_all > max_serial_no? max_serial_no_all: max_serial_no;
				is_username_exists_all += is_username_exists;

				server_name = 'C';
				if(send_check(sockfd_udp,&addrC,username,server_name))
					return -1;
				
				if(receive_check(sockfd_udp,username,&is_username_exists
							,&sum_amount_one_server,&max_serial_no,server_name))
					return -1;
				
				sum_amount += sum_amount_one_server;
				max_serial_no_all = max_serial_no_all > max_serial_no? max_serial_no_all: max_serial_no;
				is_username_exists_all += is_username_exists;

				if(is_username_exists_all == 0) //username is not exists for all servers
				{
					tAckToClient.result_code = 2;//sender not exists
					send_len = send(connfd,&tAckToClient,sizeof(tAckToClient),0);
					if(send_len!=sizeof(tAckToClient))
					{
						printf("send tAckToClient failed,send_len=%d,errmsg=[%s]\n",send_len,strerror(errno));
						return -1;
					}
				}
				else//success
				{
					tAckToClient.result_code = 1;//success
					tAckToClient.balance_amount = htonl(sum_amount);
					send_len = send(connfd,&tAckToClient,sizeof(tAckToClient),0);
					if(send_len!=sizeof(tAckToClient))
					{
						printf("send tAckToClient failed 2,send_len=%d,errmsg=[%s]\n",send_len,strerror(errno));
						return -1;
					}
				}

				printf("The main server sent the current balance to client %c.\n",client_name);
			}
			else if(!strncmp(buff,"TXCOINS",sizeof("TXCOINS")-1))
			{
				char * token = strtok(buff + sizeof("TXCOINS")," ");//strtok will change buff
				sprintf(username,"%s",token);
				token = strtok(NULL," ");
				sprintf(username_to,"%s",token);
				token = strtok(NULL," ");
				amount = atoi(token);

				//loop check server A,B,C
				//server A begin
				server_name = 'A';
				if(send_txcoins(sockfd_udp,&addrA,username,username_to,server_name))
					return -1;
				
				if(receive_txcoins(sockfd_udp,username,username_to
							,&is_username_exists,&is_username_to_exists
							,&sum_amount_one_server,&max_serial_no,server_name))
					return -1;
				
				sum_amount += sum_amount_one_server;
				max_serial_no_all = max_serial_no_all > max_serial_no? max_serial_no_all: max_serial_no;
				is_username_exists_all += is_username_exists;
				is_username_to_exists_all += is_username_to_exists;
				//server A end

				//server B begin
				server_name = 'B';
				if(send_txcoins(sockfd_udp,&addrB,username,username_to,server_name))
					return -1;
				
				if(receive_txcoins(sockfd_udp,username,username_to
							,&is_username_exists,&is_username_to_exists
							,&sum_amount_one_server,&max_serial_no,server_name))
					return -1;
								
				sum_amount += sum_amount_one_server;
				max_serial_no_all = max_serial_no_all > max_serial_no? max_serial_no_all: max_serial_no;
				is_username_exists_all += is_username_exists;
				is_username_to_exists_all += is_username_to_exists;
				//server B end

				//server C begin
				server_name = 'C';
				if(send_txcoins(sockfd_udp,&addrC,username,username_to,server_name))
					return -1;
				
				if(receive_txcoins(sockfd_udp,username,username_to
							,&is_username_exists,&is_username_to_exists
							,&sum_amount_one_server,&max_serial_no,server_name))
					return -1;
								
				sum_amount += sum_amount_one_server;
				max_serial_no_all = max_serial_no_all > max_serial_no? max_serial_no_all: max_serial_no;
				is_username_exists_all += is_username_exists;
				is_username_to_exists_all += is_username_to_exists;
				//server C end

				if(is_username_exists_all == 0 && is_username_to_exists_all == 0)//both not exists
				{
					tAckToClient.result_code = 4;
					send_len = send(connfd,&tAckToClient,sizeof(tAckToClient),0);
					if(send_len!=sizeof(tAckToClient))
					{
						printf("send tAckToClient failed 1,send_len=%d,errmsg=[%s]\n",send_len,strerror(errno));
						return -1;
					}
				}
				else if(is_username_exists_all == 0)//only sender not  exists
				{
					tAckToClient.result_code = 2;
					send_len = send(connfd,&tAckToClient,sizeof(tAckToClient),0);
					if(send_len!=sizeof(tAckToClient))
					{
						printf("send tAckToClient failed 1,send_len=%d,errmsg=[%s]\n",send_len,strerror(errno));
						return -1;
					}
				}
				else if(is_username_to_exists_all == 0)//only receiver not exists
				{
					tAckToClient.result_code = 3;
					send_len = send(connfd,&tAckToClient,sizeof(tAckToClient),0);
					if(send_len!=sizeof(tAckToClient))
					{
						printf("send tAckToClient failed 1,send_len=%d,errmsg=[%s]\n",send_len,strerror(errno));
						return -1;
					}
				}
				else if(sum_amount < amount)//insufficient balance
				{
					tAckToClient.result_code = 5;
					tAckToClient.balance_amount = htonl(sum_amount);
					send_len = send(connfd,&tAckToClient,sizeof(tAckToClient),0);
					if(send_len!=sizeof(tAckToClient))
					{
						printf("send tAckToClient failed 1,send_len=%d,errmsg=[%s]\n",send_len,strerror(errno));
						return -1;
					}
				}
				else
				{
					//random number
					srand((unsigned)time(NULL));
					int random_number = rand()%3;//0 or 1 or 2;

					struct sockaddr *pAddr;
					if(random_number == 0)
						pAddr = &addrA;
					else if(random_number == 1)
						pAddr = &addrB;
					else
						pAddr = &addrC;

					max_serial_no_all++;

					sprintf(buff,"WRITE_FILE %d %s %s %d",max_serial_no_all,username,username_to,amount);

					buff_len = strlen(buff);

					send_len = sendto(sockfd_udp,buff,buff_len,0,pAddr,sizeof(struct sockaddr));
					if(send_len != buff_len)
					{
						printf("sendto failed,send_len=%d,buff_len=%d,errmsg=%s\n",send_len,buff_len,strerror(errno));
						return -1;
					}

					uint8_t result_code;
					recv_len = recv(sockfd_udp,&result_code,sizeof(result_code),0);
					if(recv_len != sizeof(result_code))
					{
						printf("recv result_code recv failed ,recv_len=%d,errmsg=%s\n",recv_len,strerror(errno));
						return -1;
					}

					if(result_code != 1)
					{
						printf("send WRITE_FILE,but receive not success,result_code=%u\n",result_code);
						return -1;
					}

					//printf("sum_amount=%d,amount=%d\n",sum_amount,amount);

					tAckToClient.result_code = 1;
					tAckToClient.balance_amount = htonl(sum_amount - amount);
					send_len = send(connfd,&tAckToClient,sizeof(tAckToClient),0);
					if(send_len!=sizeof(tAckToClient))
					{
						printf("send tAckToClient failed 2,send_len=%d,errmsg=[%s]\n",send_len,strerror(errno));
						return -1;
					}
				}

				printf("The main server sent the result of the transaction to client %c.\n",client_name);
			}
			else if(!strncmp(buff,"TXLIST",sizeof("TXLIST")-1))
			{
				//server A begin
				if(send_txlist(sockfd_udp,&addrA))
					return -1;
				
				if(receive_txlist(sockfd_udp,arrTRecord,&arrTRecord_offset))
					return -1;
				//server A end

				//server B begin
				if(send_txlist(sockfd_udp,&addrB))
					return -1;
				
				if(receive_txlist(sockfd_udp,arrTRecord,&arrTRecord_offset))
					return -1;
				//server B end

				//server C begin
				if(send_txlist(sockfd_udp,&addrC))
					return -1;
				
				if(receive_txlist(sockfd_udp,arrTRecord,&arrTRecord_offset))
					return -1;
				//server C end
				
				//Bubble sorting
				sort_record(arrTRecord,arrTRecord_offset);

				//write file begin
				FILE *fp = fopen("alichain.txt","w");
				if(fp == NULL)
				{
					printf("open alichain.txt failed\n");
					return -1;
				}
				int i;
				for(i=0;i<arrTRecord_offset;i++)
				{
					sprintf(buff,"%d %s %s %d\n"
							,arrTRecord[i].serial_no
							,arrTRecord[i].sender
							,arrTRecord[i].receiver
							,arrTRecord[i].transfer_amount);
					if(fputs(buff,fp) == EOF)
					{
						printf("fputs failed,buff=[%s]\n",buff);
						return -1;
					}
				}
				fclose(fp);
				//write file end

			}
			else if(!strncmp(buff,"STATS",sizeof("STATS")-1))
			{
				sprintf(username,"%s",buff + sizeof("STATS"));
	
				//server A begin
				if(send_stats(sockfd_udp,&addrA,username))
					return -1;
				
				if(receive_txlist(sockfd_udp,arrTRecord,&arrTRecord_offset))
					return -1;
				//server A end

				//server B begin
				if(send_stats(sockfd_udp,&addrB,username))
					return -1;
				
				if(receive_txlist(sockfd_udp,arrTRecord,&arrTRecord_offset))
					return -1;
				//server B end

				//server C begin
				if(send_stats(sockfd_udp,&addrC,username))
					return -1;
				
				if(receive_txlist(sockfd_udp,arrTRecord,&arrTRecord_offset))
					return -1;
				//server C end
				
				//Bubble sorting
				sort_record(arrTRecord,arrTRecord_offset);

				//begin send to client
				int i;
				struct TRecord tRecordSend;
				for(i=arrTRecord_offset-1; i>=0; i--)
				{
					//Host sequence to network sequence
					tRecordSend = arrTRecord[i];
					tRecordSend.serial_no = htonl(tRecordSend.serial_no);
					tRecordSend.transfer_amount = htonl(tRecordSend.transfer_amount);
					send_len = send(connfd,&tRecordSend,sizeof(tRecordSend),0);
					if(send_len!=sizeof(tRecordSend))
					{
						printf("send tRecordSend failed,send_len=%d,errmsg=[%s]\n",send_len,strerror(errno));
						return -1;
					}
				}

				//send end msg
				tRecordSend.serial_no = htonl(-1);
				send_len = send(connfd,&tRecordSend,sizeof(tRecordSend),0);
				if(send_len!=sizeof(tRecordSend))
				{
					printf("send tRecordSend failed 2,send_len=%d,errmsg=[%s]\n",send_len,strerror(errno));
					return -1;
				}

			}
			else
			{
				printf("receive error msg=%s\n",buff);
				return -1;
			}
			
			//recv close
			recv_len = recv(connfd,buff,sizeof(buff),0);
			if(recv_len == 0)
			{
				//printf("close connfd=%d\n",connfd);
				close(connfd);
			}
			else
			{
				printf("err recv_len=%d\n",recv_len);
				return -1;
			}

		}
		else //if(sockfd_client<=0)
		{
			printf("select failed,ready_cnt=%d,errmsg=%s\n",ready_cnt,strerror(errno));
			continue;
		}
	}

	

	close(sockfd_udp);
	close(sockfd_tcp_A);
	close(sockfd_tcp_B);

	return 0;
}

//Bubble sorting
static void sort_record(struct TRecord *arrTRecord,int rows)
{
	struct TRecord tRecordTmp;
	int i,j;
	for(i=0;i<rows-1;i++)
	{
		for(j=0;j<rows-1-i;j++)
		{
			if(arrTRecord[j].serial_no > arrTRecord[j+1].serial_no)
			{
				tRecordTmp = arrTRecord[j+1];
				arrTRecord[j+1] = arrTRecord[j];
				arrTRecord[j] = tRecordTmp;
			}
		}
	}
}

static int send_stats(int sockfd_udp,const struct sockaddr* pAddr,const char *username)
{
	char buff[MSG_BUFF_MAX_SIZE];

	sprintf(buff,"STATS %s",username);

	int buff_len = strlen(buff);

	int send_len = sendto(sockfd_udp,buff,buff_len,0,pAddr,sizeof(struct sockaddr));
	if(send_len != buff_len)
	{
		printf("sendto STATS failed,send_len=%d,buff_len=%d,errmsg=%s\n",send_len,buff_len,strerror(errno));
		return -1;
	}

	return 0;
}

static int send_txlist(int sockfd_udp,const struct sockaddr* pAddr)
{
	char buff[MSG_BUFF_MAX_SIZE];

	sprintf(buff,"TXLIST");

	int buff_len = strlen(buff);

	int send_len = sendto(sockfd_udp,buff,buff_len,0,pAddr,sizeof(struct sockaddr));
	if(send_len != buff_len)
	{
		printf("sendto TXLIST failed,send_len=%d,buff_len=%d,errmsg=%s\n",send_len,buff_len,strerror(errno));
		return -1;
	}

	return 0;
}
				
static int receive_txlist(int sockfd_udp,struct TRecord *arrTRecord,int *p_arrTRecord_offset)
{
	struct sockaddr addrFrom;
	socklen_t addrFromLen = sizeof(addrFrom);
	while(1)
	{
		struct TRecord *ptRecord = arrTRecord + *p_arrTRecord_offset;
		int recv_len = recvfrom(sockfd_udp,ptRecord,sizeof(struct TRecord),0,&addrFrom,&addrFromLen);
		if(recv_len != sizeof(struct TRecord))
		{
			printf("receive_txlist recv failed ,recv_len=%d,errmsg=%s\n",recv_len,strerror(errno));
			return -1;
		}

		ptRecord->serial_no = ntohl(ptRecord->serial_no);

		if(ptRecord->serial_no == -1)//end msg
		{
			break;
		}

		ptRecord->transfer_amount = ntohl(ptRecord->transfer_amount);

		(*p_arrTRecord_offset)++;
	}
	return 0;
}

static int send_txcoins(int sockfd_udp,const struct sockaddr* pAddr
					,const char *username,const char *username_to,char server_name)
{
	char buff[MSG_BUFF_MAX_SIZE];

	sprintf(buff,"TXCOINS %s %s",username,username_to);

	int buff_len = strlen(buff);

	int send_len = sendto(sockfd_udp,buff,buff_len,0,pAddr,sizeof(struct sockaddr));
	if(send_len != buff_len)
	{
		printf("sendto failed 3,send_len=%d,buff_len=%d,errmsg=%s\n",send_len,buff_len,strerror(errno));
		return -1;
	}

	printf("The main server sent a request to server %c.\n",server_name);
	
	return 0;
}

static int receive_txcoins(int sockfd_udp,const char *username,const char *username_to
			,int *p_is_username_exists,int *p_is_username_to_exists
			,int *p_sum_amount_one_server,int *p_max_serial_no
			,char server_name)
{
	struct TRecord tRecord;
	struct sockaddr addrFrom;
	socklen_t addrFromLen = sizeof(addrFrom);
	int i;

	*p_is_username_exists = 0;
	*p_is_username_to_exists = 0;
	*p_sum_amount_one_server = 0;
	*p_max_serial_no = 0;

	for(i=0;;i++)
	{
		int recv_len = recvfrom(sockfd_udp,&tRecord,sizeof(tRecord),0,&addrFrom,&addrFromLen);
		if(recv_len != sizeof(tRecord))
		{
			printf("receive_txcoins recv failed 1,recv_len=%d,errmsg=%s,i=%d\n",recv_len,strerror(errno),i);
			return -1;
		}

		tRecord.transfer_amount = ntohl(tRecord.transfer_amount);

		if(ntohl(tRecord.serial_no) == -1)//end msg
		{
			*p_max_serial_no = tRecord.transfer_amount;
			break;
		}

		if(!strcmp(tRecord.sender,username))
		{
			*p_sum_amount_one_server -= tRecord.transfer_amount;
			*p_is_username_exists = 1;
		}
		else if(!strcmp(tRecord.receiver,username))
		{
			*p_sum_amount_one_server += tRecord.transfer_amount;
			*p_is_username_exists = 1;
		}
		
		if(!strcmp(tRecord.sender,username_to))
		{
			*p_is_username_to_exists = 1;
		}
		else if(!strcmp(tRecord.receiver,username_to))
		{
			*p_is_username_to_exists = 1;
		}
	}

	uint16_t remote_port = ntohs( ((struct sockaddr_in *)&addrFrom)->sin_port );//port for server A,B,orC

	printf("The main server received the feedback from server %c using UDP over port %u.\n",server_name,remote_port);

	return 0;
}


//return 0 if success , otherwise is error
static int send_check(int sockfd_udp,const struct sockaddr* pAddr
			,const char *username,char server_name)
{
	char buff[MSG_BUFF_MAX_SIZE];

	sprintf(buff,"CHECK %s",username);

	int buff_len = strlen(buff);

	int send_len = sendto(sockfd_udp,buff,buff_len,0,pAddr,sizeof(struct sockaddr));
	if(send_len != buff_len)
	{
		printf("send failed,send_len=%d,buff_len=%d,errmsg=%s\n",send_len,buff_len,strerror(errno));
		return -1;
	}

	printf("The main server sent a request to server %c.\n",server_name);
	
	return 0;
}

//return 0 if success , otherwise is error
static int receive_check(int sockfd_udp,const char *username
			,int *p_is_username_exists,int *p_sum_amount_one_server
			,int *p_max_serial_no,char server_name)
{
	struct TRecord tRecord;
	struct sockaddr addrFrom;
	socklen_t addrFromLen = sizeof(addrFrom);
	int i;

	*p_is_username_exists = 0;
	*p_sum_amount_one_server = 0;
	*p_max_serial_no = 0;

	for(i=0;;i++)
	{
		int recv_len = recvfrom(sockfd_udp,&tRecord,sizeof(tRecord),0,&addrFrom,&addrFromLen);
		if(recv_len != sizeof(tRecord))
		{
			printf("receive_check recv failed 1,recv_len=%d,errmsg=%s,i=%d\n",recv_len,strerror(errno),i);
			return -1;
		}

		tRecord.transfer_amount = ntohl(tRecord.transfer_amount);

		if(ntohl(tRecord.serial_no) == -1)//end msg
		{
			*p_max_serial_no = tRecord.transfer_amount;
			break;
		}

		if(!strcmp(tRecord.sender,username))
		{
			*p_sum_amount_one_server -= tRecord.transfer_amount;
		}
		else if(!strcmp(tRecord.receiver,username))
		{
			*p_sum_amount_one_server += tRecord.transfer_amount;
		}
		else //shouldn't happen
		{
			printf("Neither of sender(%s) or receiver(%s) are username(%s)\n",tRecord.sender,tRecord.receiver,username);
			return -1;
		}
		
	}

	if(i>0)
		*p_is_username_exists = 1;

	uint16_t remote_port  = ntohs( ((struct sockaddr_in *)&addrFrom)->sin_port );//port for server A,B,orC

	printf("The main server received transactions from Server %c using UDP over port %u.\n",server_name,remote_port);

	return 0;
}
