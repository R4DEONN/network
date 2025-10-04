#pragma once

#include <utility>

#include "string"
#include "../socket/TcpClient.h"

class Client
{
public:
	Client(std::string address, u_short port, std::string name);

	void run() const;

private:
	std::string m_address;
	u_short m_port;
	std::string m_name;
	TcpClient m_tcp;
};
