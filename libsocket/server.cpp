#include <stdio.h>
#include "libsocket.h"

#define SERV_PORT 11111

int on_connect(socket_client_t *pclient)
{
	if (!pclient)
		return -1;

	printf("on_connect, client_id[%d]\n", pclient->get_cid());
	return 0;
}

int on_read(socket_client_t *pclient, packet_t &packet)
{
	if (!pclient || !packet.data || packet.head.len <= 0)
		return -1;

	printf("on_read, client_id[%d], content[%s]\n", pclient->get_cid(), (char*)packet.data);
	return 0;
}

int main(int argc, char *argv[])
{
	socket_srv_t server;
	server.set_connect_callback(on_connect);
	server.set_read_callback(on_read);

	server.listen_client(SERV_PORT);
	server.run();
	return 0;
}