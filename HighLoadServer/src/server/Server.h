#pragma once

#include "string"
#include "../socket/TcpServer.h"

class Server
{
public:
	Server(u_short port, std::string name);

	void run() const;

private:
	TcpServer m_tcp;
	std::string m_name;
	u_short m_port;
};
