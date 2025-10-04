#include "Server.h"

#include <iostream>

#include "../socket/TcpServer.h"

Server::Server(u_short port, std::string name)
	: m_name(std::move(name)),
	  m_port(port)
{
}

void Server::run() const
{
	if (!m_tcp.bind(m_port) || !m_tcp.listen())
	{
		throw std::runtime_error("Server: failed to bind");
	}

	if (auto client = m_tcp.accept())
	{
		const std::string message = client->receiveString();
		std::cout << "Received: " << message << std::endl;
		client->sendString("Echo: " + message);
	}
}
