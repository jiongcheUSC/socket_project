#ifndef __COMMON_H__
#define __COMMON_H__
/*
myself socket common lib
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
//add for main server
//#include <sys/select.h>
#include <netinet/tcp.h>//TCP_NODELAY
#include <time.h>//time

#define USC_ID_END 015	//xxx is the last 3 digits of your USC ID
#define SERVER_A_UDP_PORT 21000 + USC_ID_END
#define SERVER_B_UDP_PORT 22000 + USC_ID_END
#define SERVER_C_UDP_PORT 23000 + USC_ID_END
#define SERVER_M_UDP_PORT 24000 + USC_ID_END
#define SERVER_M_TCP_PORT_A 25000 + USC_ID_END
#define SERVER_M_TCP_PORT_B 26000 + USC_ID_END
#define HOST_NAME "127.0.0.1"	//The host name must be hard coded as localhost (127.0.0.1) in all codes

#define MSG_BUFF_MAX_SIZE 128	//socket msg max length = 127
#define USER_NAME_MAX_SIZE 15+1	//username max length = 15
#define BLOCK_FILE_MAX_ROWS 1024//blocki.txt max rows

//the format for each Alichain transaction
struct TRecord
{
	int serial_no;
	char sender[USER_NAME_MAX_SIZE];
	char receiver[USER_NAME_MAX_SIZE];
	int  transfer_amount;//when serial_no>=0 ,it means transfer_amount,
						 //When serial_no=-1,it means max serial_no for this server
}__attribute__((packed));//Remove memory alignment


struct TAckToClient
{
	uint8_t result_code;//1:success ,2:sender is not exists ,3:receiver is not exists,
						//4: both are not exists ,5:insufficient balance
	int balance_amount;
}__attribute__((packed));//Remove memory alignment

int init_udp(uint16_t port,const char *ip);
int init_tcp_server(uint16_t port,const char *ip);
int init_tcp_client(uint16_t port,const char *ip);

int client(const char *client_name,uint16_t remote_port,int argc,char *argv[]);
int server(const char *server_name,uint16_t local_port,const char *filename);




#endif