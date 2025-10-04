#pragma once

#include <utility>

#include "string"

class Server
{
public:
	Server(int port, std::string name)
		: m_port(port),
		  m_name(std::move(name))
	{
	}

private:
	int m_port;
	std::string m_name;
};
