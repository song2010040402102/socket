#include <stdio.h>
#include <stdlib.h>
#include "libsocket.h"

#define SERV_PORT 11111

int main(int argc, char *argv[])
{
	if (argc < 2)
		return 0;

	int cid = atoi(argv[1]);
	socket_client_t client;	
	client.connect_serv(cid, SERV_PORT);

	char *p_content = "hello";
	packet_t packet;
	make_packet(p_content, packet);	
	client.send_packet(packet);
	release_packet(packet);
	return 0;
}