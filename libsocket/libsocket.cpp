#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>	
#include <arpa/inet.h>
#include <time.h>
#include "libsocket.h"

#define MAX_CONCURRENCE 1024*10
#define RESEND_TIMEOUT 5

int socket_error(int socket, char *function)
{
	if (!function)
		return -1;

	int err_code = errno;
	printf("[ERROR] socket: %d, function: %s, error: %d, detail: %s\n", socket, function, err_code, strerror(err_code));

	shutdown(socket, SHUT_RDWR);
	close(socket);

	return err_code;
}

void complete_packet(packet_t &packet)
{
	static int packet_id = 0;
	if (packet.head.id == 0)
		packet.head.id = ++packet_id;
	if (packet.head.ts == 0)
		packet.head.ts = time(NULL);
}

void copy_packet(packet_t &src, packet_t &dst)
{	
	if (src.data && src.head.len > 0)
	{		
		memcpy(&dst.head, &src.head, sizeof(head_t));
		dst.data = new char[src.head.len];
		memcpy(dst.data, src.data, src.head.len);
	}
}

void make_packet(char *pbuff, packet_t &packet)
{	
	if (pbuff)
	{
		packet.head.len = strlen(pbuff) + 1;
		packet.data = new char[packet.head.len];
		memcpy(packet.data, pbuff, packet.head.len);
	}	
}

void release_packet(packet_t &packet)
{
	if (packet.data)
	{
		delete[]packet.data, packet.data = NULL;
	}
}

lost_packet_man_t::lost_packet_man_t()
{

}

lost_packet_man_t::~lost_packet_man_t()
{
	clear();
}

int lost_packet_man_t::add_lost_packet(int cid, packet_t &packet)
{		
	map_uid_packet_t::iterator iter = m_map_uid_packet.find(cid);
	if (iter != m_map_uid_packet.end())
	{
		iter->second.insert(std::make_pair(packet.head.id, packet));
	}
	else
	{
		map_id_packet_t map_packet;
		map_packet.insert(std::make_pair(packet.head.id, packet));
		m_map_uid_packet.insert(std::make_pair(cid, map_packet));
	}	

	return 0;
}

lost_packet_man_t::map_id_packet_t* lost_packet_man_t::get_lost_packet(int cid)
{
	map_uid_packet_t::iterator iter = m_map_uid_packet.find(cid);
	if (iter != m_map_uid_packet.end())
		return &iter->second;
	return NULL;
}

int lost_packet_man_t::del_lost_packet(int cid)
{
	map_uid_packet_t::iterator iter = m_map_uid_packet.find(cid);
	if (iter != m_map_uid_packet.end())
	{
		for (map_id_packet_t::iterator it = iter->second.begin(); it != iter->second.end(); it++)
		{
			release_packet(it->second);
		}
		iter->second.clear();
		m_map_uid_packet.erase(iter);
	}			
	return 0;
}

int lost_packet_man_t::del_lost_packet(int cid, int packet_id)
{
	map_uid_packet_t::iterator iter = m_map_uid_packet.find(cid);
	if (iter != m_map_uid_packet.end())
	{
		map_id_packet_t::iterator it = iter->second.find(packet_id);
		if (it != iter->second.end())
		{
			release_packet(it->second);
			iter->second.erase(it);
		}
	}
}

void lost_packet_man_t::clear()
{
	for (map_uid_packet_t::iterator iter = m_map_uid_packet.begin(); iter != m_map_uid_packet.end(); iter++)
	{
		for (map_id_packet_t::iterator it = iter->second.begin(); it != iter->second.end(); it++)
		{
			release_packet(it->second);
		}
		iter->second.clear();
	}
	m_map_uid_packet.clear();
}

socket_t::socket_t()
{
	m_fd = 0;
}

socket_t::~socket_t()
{

}

int socket_t::connect_serv(unsigned short port, char* ip)
{
	int socket_client = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_client < 0)
	{
		return socket_error(socket_client, "socket");		
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;	
	if (inet_aton(ip?ip:"127.0.0.1", &server_addr.sin_addr) == 0)
	{
		printf("[ERROR] function: inet_aton, error: %d, detail: %s\n", errno, strerror(errno));
		return 0;
	}
	server_addr.sin_port = htons(port);
	if (connect(socket_client, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		return socket_error(socket_client, "connect");		
	}
	m_fd = socket_client;
	printf("connect succeed!\n");
	return 0;
}

int socket_t::send_packet(packet_t &packet)
{
	int ret = 0;
	if (packet.head.len <= 0 || !packet.data)
		return -1;

	int sum_len = sizeof(head_t)+packet.head.len;
	char *data = new char[sum_len];	
	if (!data)
		return -1;
	
	memset(data, 0, sum_len);
	memcpy(data, &packet.head, sizeof(head_t));
	memcpy(data+sizeof(head_t), &packet.data, packet.head.len);

	complete_packet(packet);

	int send_len = 0;
	while (1)
	{
		int len = send(m_fd, data + send_len, sum_len - send_len, 0);
		if (len < 0)
		{			
			ret = socket_error(m_fd, "send");
			break;
		}
		send_len += len;
		if (send_len >= sum_len)
			break;
	}
	delete []data, data = NULL;
	return ret;
}

int socket_t::recv_packet(packet_t &packet)
{
	int ret = 0;
	int len = recv(m_fd, &packet.head, sizeof(head_t), 0);
	if (len == sizeof(head_t) && packet.head.len > 0)
	{
		packet.data = new char[packet.head.len];
		if (!packet.data)
			return -1;
		memset(packet.data, 0, packet.head.len);

		int recv_len = 0;
		while (1)
		{
			len = recv(m_fd, packet.data + recv_len, packet.head.len - recv_len, 0);
			if (len < 0)
			{
				ret = socket_error(m_fd, "recv");
				break;
			}
			recv_len += len;
			if (recv_len >= packet.head.len)
				break;
		}
	}
	return ret;
}

int socket_t::disconnect()
{
	if (m_fd > 0)
	{
		shutdown(m_fd, SHUT_RDWR);
		close(m_fd);
	}
}

socket_client_t::socket_client_t()
{

}

socket_client_t::~socket_client_t()
{

}

int socket_client_t::connect_serv(int cid, unsigned short port, char* ip /* = NULL */)
{
	int ret = socket_t::connect_serv(port, ip);
	if (ret == 0)
	{
		cid = htonl(cid);
		if (send(m_fd, &cid, sizeof(int), 0) > 0)
			m_cid = cid;
		else
			ret = socket_error(m_fd, "send");		
	}
	return ret;
}

int socket_client_t::send_packet(packet_t &packet)
{
	packet.head.cid = m_cid;
	return socket_t::send_packet(packet);
}

int socket_client_t::recv_packet(packet_t &packet)
{
	int ret = recv_packet(packet);
	if (ret == 0)
	{
		packet_t copy;
		copy.head.id = packet.head.id;
		copy.head.cid = packet.head.cid;
		copy.head.ts = time_t(NULL);
		copy.head.flag = F_ACK;
		copy.head.len = 0, copy.data = NULL;
		socket_t::send_packet(copy);
	}
	return ret;
}

socket_srv_t::socket_srv_t()
{

}

socket_srv_t::~socket_srv_t()
{

}

int socket_srv_t::listen_client(unsigned short port, char* ip, int backlog)
{
	int socket_serv = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_serv < 0)
	{
		return socket_error(socket_serv, "socket");
	}

	int on = 1;
	if (setsockopt(socket_serv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
	{
		return socket_error(socket_serv, "setsockopt");
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	if (ip)
		server_addr.sin_addr.s_addr = inet_addr(ip);
	else
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	if (bind(socket_serv, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		return socket_error(socket_serv, "bind");
	}

	if (listen(socket_serv, backlog) < 0)
	{
		return socket_error(socket_serv, "listen");
	}
	m_fd = socket_serv;

	printf("server running...\nlisten address: %s\n", inet_ntoa(server_addr.sin_addr));
	return 0;
}

int socket_srv_t::run()
{
	struct epoll_event  ee_t, ee_a[MAX_CONCURRENCE + 1];
	int efd = epoll_create(MAX_CONCURRENCE + 1);
	if (efd == -1)
		return 0;

	ee_t.events = EPOLLIN, ee_t.data.fd = m_fd;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, m_fd, &ee_t) == -1)
		return 0;

	int socket_client;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	while (1)
	{
		size_t read_num = epoll_wait(efd, ee_a, MAX_CONCURRENCE + 1, -1);
		int i;
		for (i = 0; i < read_num; i++)
		{
			if (ee_a[i].data.fd == m_fd)
			{
				client_addr_len = sizeof(client_addr);
				socket_client = accept(m_fd, (struct sockaddr*)&client_addr, &client_addr_len);
				if (socket_client > 0)
				{
					ee_t.events = EPOLLIN, ee_t.data.fd = socket_client;
					epoll_ctl(efd, EPOLL_CTL_ADD, socket_client, &ee_t);
					printf("[client] ip: %s, port: %d, connected\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);

					//read cid
					int cid = 0;
					if (read(socket_client, &cid, sizeof(int)) > 0)
					{
						socket_client_t *pclient = new socket_client_t;
						pclient->m_fd = socket_client;
						pclient->set_cid(ntohl(cid));
						map_cid_client_t::iterator iter = m_map_client.find(pclient->get_cid());
						if (iter != m_map_client.end())
						{
							delete iter->second;
							iter->second = pclient;
						}
						else
							m_map_client.insert(std::make_pair(pclient->get_cid(), pclient));
						on_connect(pclient);
					}
				}
			}
			else
			{
				packet_t packet;
				int ret = recv_packet(packet);
				if (ret == 0)
				{
					map_cid_client_t::iterator iter = m_map_client.find(packet.head.cid);
					if (iter != m_map_client.end())
					{
						on_read(iter->second, packet);
					}
				}
			}
		}
	}
}

int socket_srv_t::on_connect(socket_client_t *pclient)
{
	if (!pclient)
		return -1;
	lost_packet_man_t::map_id_packet_t* p_map_packet = m_lost_packet_man.get_lost_packet(pclient->get_cid());
	if (p_map_packet)
	{
		time_t now = time_t(NULL);		
		for (lost_packet_man_t::map_id_packet_t::iterator iter = p_map_packet->begin(); iter != p_map_packet->end(); iter++)
		{
			if (now - iter->second.head.ts < RESEND_TIMEOUT)
			{
				iter->second.head.ts = now;
				socket_t::send_packet(iter->second);
			}
		}
	}
	if (m_p_fun_socket_connect)
		return m_p_fun_socket_connect(pclient);
	return 0;
}

int socket_srv_t::on_read(socket_client_t *pclient, packet_t &packet)
{
	if (!pclient)
		return -1;
	if (packet.head.flag == F_ACK)
	{
		m_lost_packet_man.del_lost_packet(packet.head.cid, packet.head.id);
		return 0;
	}		
	if (m_p_fun_socket_read)
		return m_p_fun_socket_read(pclient, packet);
	return 0;
}

int socket_srv_t::send_packet(packet_t &packet)
{	
	int ret = socket_t::send_packet(packet);	
	if (ret == 0)
	{
		m_lost_packet_man.add_lost_packet(packet.head.cid, packet);
	}
	return ret;
}

int socket_srv_t::recv_packet(packet_t &packet)
{
	return socket_t::recv_packet(packet);
}