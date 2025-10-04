#pragma once

#include <utility>

#include "string"

class Client
{
public:
	Client(std::string address, int port, std::string name)
		: m_address(std::move(address)),
		  m_port(port),
		  m_name(std::move(name))
	{
	}

	void Run();

private:
	std::string m_address;
	int m_port;
	std::string m_name;
};
