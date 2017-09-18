#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>	
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 30998
#define MAX_CONNECT_NUM 10

int socket_error(int socket, char *function)
{
	if(!function)
		return -1;

	int err_code = errno;
	printf("[ERROR] socket: %d, function: %s, error: %d, detail: %s\n", socket, function, err_code, strerror(err_code));

	shutdown(socket, SHUT_RDWR);
	close(socket);

	return err_code;
}

int socket_recv(int socket, char** pbuf, int* pbuf_len)
{	
	if(!pbuf || !pbuf_len)
		return -1;
	int read_len = recv(socket, pbuf_len, sizeof(int), 0);
	if(read_len < 0)
	{
		socket_error(socket, "recv");
		return -2;		
	}	
	if(read_len == 0)
	{
		printf("client shutdown!\n");
		return -3;
	}
	if(read_len < sizeof(int))
	{
		printf("read_len should be %lu bytes\n", sizeof(int));
		return -4;
	}
	*pbuf_len = ntohl(*pbuf_len);
	printf("read_len: %d, buf_len: %d\n", read_len, *pbuf_len);
	if(*pbuf_len <= 0)
	{
		return -5;
	}

	read_len = 0;
	*pbuf = (char*)malloc(*pbuf_len);
	while(1)
	{
		read_len += recv(socket, *pbuf+read_len, *pbuf_len-read_len, 0);
		if(read_len >= *pbuf_len)
			break;
	}	
	return 0; 
}

int socket_send(int socket, char* pbuf, int buf_len)
{
	if(!pbuf || buf_len <= 0)
		return -1;

	int write_len = htonl(buf_len);
	write_len = send(socket, &write_len, sizeof(int), 0);
	if(write_len < 0)
	{
		socket_error(socket, "send");
		return -2;
	}
	if(write_len == 0)
	{
		printf("client shutdown!\n");
		return -3;
	}
	if(write_len < sizeof(int))
	{
		printf("write_len should be %lu bytes\n", sizeof(int));
		return -4;
	}
	printf("write_len: %d, buf_len: %d\n", write_len, buf_len);

	write_len = 0;
	while(1)
	{
		write_len += send(socket, pbuf+write_len, buf_len-write_len, 0);
		if(write_len >= buf_len)
			break;
	}

	return 0;
}

void *handle_client(void* args)
{
	if(!args) return NULL;
	int socket_client = *(int*)args;		

	char *pbuf = NULL;
	int buf_len = 0;
	while(1)
	{		
		getchar();
		pbuf = NULL, buf_len = 0;
		if(socket_recv(socket_client, &pbuf, &buf_len) < 0 || !pbuf || buf_len == 0)
		{
			break;
		}		

		getchar();
		if(socket_send(socket_client, pbuf, buf_len) < 0)
		{
			break;
		}

		free(pbuf);
	}	
}

int main(int argc, char * argv[])
{
	int socket_serv = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_serv < 0)
	{
		return socket_error(socket_serv, "socket");
	}

	int on = 1;
	if(setsockopt(socket_serv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
	{
		return socket_error(socket_serv, "setsockopt");
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORT);
	if(bind(socket_serv, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		return socket_error(socket_serv, "bind");
	}

	if(listen(socket_serv, MAX_CONNECT_NUM) < 0)
	{
		return socket_error(socket_serv, "listen");	
	}
	printf("server running...\nlisten address: %s\n", inet_ntoa(server_addr.sin_addr));

	int socket_client;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	pthread_t thread_id;
	while(1)
	{
		client_addr_len = sizeof(client_addr);
		socket_client = accept(socket_serv, (struct sockaddr*)&client_addr, &client_addr_len);
		if(socket_client < 0)
		{
			return socket_error(socket_serv, "accept");
		}

		if(pthread_create(&thread_id, NULL, handle_client, (void*)(&socket_client)) < 0)
		{
			printf("[ERROR] function: pthread_create, error: %d, detail: %s\n", errno, strerror(errno));
			return errno;
		}
		pthread_detach(thread_id);
	}

	return 0;
}
