#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>	
#include <arpa/inet.h>

//#define IP "192.168.199.157"
#define IP "47.88.224.240"
#define PORT 30998

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

int main(int argc, char * argv[])
{
	int socket_client = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_client < 0)
	{
		return socket_error(socket_client, "socket");
	}

	struct sockaddr_in server_addr;    
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	if(inet_aton(IP, &server_addr.sin_addr) == 0) 
	{
		printf("[ERROR] function: inet_aton, error: %d, detail: %s\n", errno, strerror(errno));
		return errno;
	}
	server_addr.sin_port = htons(PORT);    
	if(connect(socket_client, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		return socket_error(socket_client, "connect");
	}
	printf("connect succeed!\n");

	socket_send(socket_client, "hello", 6);

	char *pbuf = NULL;
	int buf_len = 0;
	socket_recv(socket_client, &pbuf, &buf_len);
	printf("%s\n", pbuf);
	return 0;
}
