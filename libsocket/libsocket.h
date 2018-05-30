#ifndef LIBSOCKET_H
#define LIBSOCKET_H

#include <map>

enum{
	F_COMMON = 0,
	F_ACK = 1,
};
typedef unsigned char byte_t;
struct head_t
{
	int id; //packet id
	int cid; //client id
	int ts; //timestamp
	int len; //data length
	byte_t flag;
};

struct packet_t
{		
	head_t head;
	void *data;	
};

void make_packet(char *pbuff, packet_t &packet);
void release_packet(packet_t &packet);

class lost_packet_man_t
{
public:
	lost_packet_man_t();
	~lost_packet_man_t();

	typedef std::map<int, packet_t> map_id_packet_t;
	typedef std::map<int, map_id_packet_t> map_uid_packet_t;

	int add_lost_packet(int cid, packet_t &packet);
	map_id_packet_t* get_lost_packet(int cid);
	int del_lost_packet(int cid);		
	int del_lost_packet(int cid, int packet_id);
	void clear();
private:	
	map_uid_packet_t m_map_uid_packet;
};

class socket_t
{
public:
	socket_t();
	~socket_t();

	int connect_serv(unsigned short port, char* ip = NULL);
	virtual int send_packet(packet_t &packet);
	virtual int recv_packet(packet_t &packet);
	int disconnect();	
protected:
	int m_fd;
};

class socket_client_t : public socket_t
{
public:
	friend class socket_srv_t;
	socket_client_t();
	~socket_client_t();

	int connect_serv(int cid, unsigned short port, char* ip = NULL);

	int send_packet(packet_t &packet);
	int recv_packet(packet_t &packet);

	int get_cid(){ return m_cid; }
	void set_cid(int cid){ m_cid = cid; }
private:
	int m_cid;
};

typedef int(*p_fun_socket_connect_t)(socket_client_t*);
typedef int(*p_fun_socket_read_t)(socket_client_t*, packet_t&);

class socket_srv_t : public socket_t
{
public:
	socket_srv_t();
	~socket_srv_t();

	int listen_client(unsigned short port, char* ip = NULL, int backlog = 64);

	int run();

	int on_connect(socket_client_t *pclient);

	int on_read(socket_client_t *pclient, packet_t &packet);

	int send_packet(packet_t &packet);	
	int recv_packet(packet_t &packet);

	void set_connect_callback(p_fun_socket_connect_t p_fun) { m_p_fun_socket_connect = p_fun; }
	void set_read_callback(p_fun_socket_read_t p_fun) { m_p_fun_socket_read = p_fun; }
private:	
	p_fun_socket_connect_t m_p_fun_socket_connect;
	p_fun_socket_read_t m_p_fun_socket_read;

	typedef std::map<int, socket_client_t*> map_cid_client_t;
	map_cid_client_t m_map_client;

	lost_packet_man_t m_lost_packet_man;	
};

#endif